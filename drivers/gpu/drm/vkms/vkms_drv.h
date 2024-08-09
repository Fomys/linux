/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef _VKMS_DRV_H_
#define _VKMS_DRV_H_

#include <linux/hrtimer.h>

#include <drm/drm.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_gem.h>
#include <drm/drm_gem_atomic_helper.h>
#include <drm/drm_encoder.h>
#include <drm/drm_writeback.h>

#define XRES_MIN    10
#define YRES_MIN    10

#define XRES_DEF  1024
#define YRES_DEF   768

#define XRES_MAX  8192
#define YRES_MAX  8192

#define NUM_OVERLAY_PLANES 8

#define VKMS_LUT_SIZE 256

/**
 * struct vkms_frame_info - Structure to store the state of a frame
 *
 * @fb: backing drm framebuffer
 * @src: source rectangle of this frame in the source framebuffer, stored in 16.16 fixed-point form
 * @dst: destination rectangle in the crtc buffer, stored in whole pixel units
 * @map: see drm_shadow_plane_state@data
 * @rotation: rotation applied to the source.
 *
 * @src and @dst should have the same size modulo the rotation.
 */
struct vkms_frame_info {
	struct drm_framebuffer *fb;
	struct drm_rect src, dst;
	struct iosys_map map[DRM_FORMAT_MAX_PLANES];
	unsigned int rotation;
};

struct pixel_argb_u16 {
	u16 a, r, g, b;
};

struct line_buffer {
	size_t n_pixels;
	struct pixel_argb_u16 *pixels;
};

struct vkms_writeback_job;
/**
 * typedef pixel_write_line_t - These functions are used to read a pixel line from a
 * struct pixel_argb_u16 buffer, convert it and write it in the @wb job.
 *
 * @wb: the writeback job to write the output of the conversion
 * @in_pixels: Source buffer containing the line to convert
 * @count: The width of a line
 * @x_start: The x (width) coordinate in the destination plane
 * @y_start: The y (height) coordinate in the destination plane
 */
typedef void (*pixel_write_line_t)(struct vkms_writeback_job *wb,
			      struct pixel_argb_u16 *in_pixels, int count, int x_start,
			      int y_start);

struct vkms_writeback_job {
	struct iosys_map data[DRM_FORMAT_MAX_PLANES];
	struct vkms_frame_info wb_frame_info;
	pixel_write_line_t pixel_write;
};

/**
 * enum pixel_read_direction - Enum used internaly by VKMS to represent a reading direction in a
 * plane.
 */
enum pixel_read_direction {
	READ_BOTTOM_TO_TOP,
	READ_TOP_TO_BOTTOM,
	READ_RIGHT_TO_LEFT,
	READ_LEFT_TO_RIGHT
};

struct vkms_plane_state;

/**
 * typedef pixel_read_line_t - These functions are used to read a pixel line in the source frame,
 * convert it to `struct pixel_argb_u16` and write it to @out_pixel.
 *
 * @plane: plane used as source for the pixel value
 * @x_start: X (width) coordinate of the first pixel to copy. The caller must ensure that x_start
 * is non-negative and smaller than @plane->frame_info->fb->width.
 * @y_start: Y (height) coordinate of the first pixel to copy. The caller must ensure that y_start
 * is non-negative and smaller than @plane->frame_info->fb->height.
 * @direction: direction to use for the copy, starting at @x_start/@y_start
 * @count: number of pixels to copy
 * @out_pixel: pointer where to write the pixel values. They will be written from @out_pixel[0]
 * (included) to @out_pixel[@count] (excluded). The caller must ensure that out_pixel have a
 * length of at least @count.
 */
typedef void (*pixel_read_line_t)(const struct vkms_plane_state *plane, int x_start,
				  int y_start, enum pixel_read_direction direction, int count,
				  struct pixel_argb_u16 out_pixel[]);

/**
 * struct conversion_matrix - Matrix to use for a specific encoding and range
 *
 * @matrix: Conversion matrix from yuv to rgb. The matrix is stored in a row-major manner and is
 * used to compute rgb values from yuv values:
 *     [[r],[g],[b]] = @matrix * [[y],[u],[v]]
 *   OR for yvu formats:
 *     [[r],[g],[b]] = @matrix * [[y],[v],[u]]
 *  The values of the matrix are signed fixed-point values with 32 bits fractional part.
 * @y_offset: Offset to apply on the y value.
 */
struct conversion_matrix {
	s64 matrix[3][3];
	int y_offset;
};

/**
 * struct vkms_plane_state - Driver specific plane state
 * @base: base plane state
 * @frame_info: data required for composing computation
 * @pixel_read_line: function to read a pixel line in this plane. The creator of a
 *		     struct vkms_plane_state must ensure that this pointer is valid
 * @conversion_matrix: matrix used for yuv formats to convert to rgb
 */
struct vkms_plane_state {
	struct drm_shadow_plane_state base;
	struct vkms_frame_info *frame_info;
	pixel_read_line_t pixel_read_line;
	struct conversion_matrix conversion_matrix;
};

struct vkms_plane {
	struct drm_plane base;
};

struct vkms_color_lut {
	struct drm_color_lut *base;
	size_t lut_length;
	s64 channel_value2index_ratio;
};

/**
 * vkms_crtc_state - Driver specific CRTC state
 * @base: base CRTC state
 * @composer_work: work struct to compose and add CRC entries
 * @n_frame_start: start frame number for computed CRC
 * @n_frame_end: end frame number for computed CRC
 */
struct vkms_crtc_state {
	struct drm_crtc_state base;
	struct work_struct composer_work;

	int num_active_planes;
	/* stack of active planes for crc computation, should be in z order */
	struct vkms_plane_state **active_planes;
	struct vkms_writeback_job *active_writeback;
	struct vkms_color_lut gamma_lut;

	/* below four are protected by vkms_output.composer_lock */
	bool crc_pending;
	bool wb_pending;
	u64 frame_start;
	u64 frame_end;
};

struct vkms_output {
	struct drm_crtc crtc;
	struct drm_encoder encoder;
	struct drm_connector connector;
	struct drm_writeback_connector wb_connector;
	struct hrtimer vblank_hrtimer;
	ktime_t period_ns;
	/* ordered wq for composer_work */
	struct workqueue_struct *composer_workq;
	/* protects concurrent access to composer */
	spinlock_t lock;

	/* protected by @lock */
	bool composer_enabled;
	struct vkms_crtc_state *composer_state;

	spinlock_t composer_lock;
};

struct vkms_device;

struct vkms_config {
	bool writeback;
	bool cursor;
	bool overlay;
	/* only set when instantiated */
	struct vkms_device *dev;
};

struct vkms_device {
	struct drm_device drm;
	struct platform_device *platform;
	struct vkms_output output;
	const struct vkms_config *config;
};

#define drm_crtc_to_vkms_output(target) \
	container_of(target, struct vkms_output, crtc)

#define drm_device_to_vkms_device(target) \
	container_of(target, struct vkms_device, drm)

#define to_vkms_crtc_state(target)\
	container_of(target, struct vkms_crtc_state, base)

#define to_vkms_plane_state(target)\
	container_of(target, struct vkms_plane_state, base.base)

/* CRTC */
int vkms_crtc_init(struct drm_device *dev, struct drm_crtc *crtc,
		   struct drm_plane *primary, struct drm_plane *cursor);

int vkms_output_init(struct vkms_device *vkmsdev, int index);

struct vkms_plane *vkms_plane_init(struct vkms_device *vkmsdev,
				   enum drm_plane_type type, int index);

/* CRC Support */
const char *const *vkms_get_crc_sources(struct drm_crtc *crtc,
					size_t *count);
int vkms_set_crc_source(struct drm_crtc *crtc, const char *src_name);
int vkms_verify_crc_source(struct drm_crtc *crtc, const char *source_name,
			   size_t *values_cnt);

/* Composer Support */
void vkms_composer_worker(struct work_struct *work);
void vkms_set_composer(struct vkms_output *out, bool enabled);

/* Writeback */
int vkms_enable_writeback_connector(struct vkms_device *vkmsdev);

#endif /* _VKMS_DRV_H_ */
