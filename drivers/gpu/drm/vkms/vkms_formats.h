/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef _VKMS_FORMATS_H_
#define _VKMS_FORMATS_H_

#include <drm/drm_color_mgmt.h>

struct vkms_plane_state;
struct vkms_writeback_job;

struct pixel_argb_u16 {
	u16 a, r, g, b;
};

/**
 * typedef pixel_write_line_t - These functions are used to read a pixel line from a
 * struct pixel_argb_u16 buffer, convert it and write it in the @wb_job.
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

struct line_buffer {
	size_t n_pixels;
	struct pixel_argb_u16 *pixels;
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

struct vkms_color_lut {
	struct drm_color_lut *base;
	size_t lut_length;
	s64 channel_value2index_ratio;
};

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

pixel_read_line_t get_pixel_read_line_function(u32 format);

pixel_write_line_t get_pixel_write_line_function(u32 format);

void get_conversion_matrix_to_argb_u16(u32 format, enum drm_color_encoding encoding,
				       enum drm_color_range range,
				       struct conversion_matrix *matrix);

#if IS_ENABLED(CONFIG_KUNIT)
struct pixel_argb_u16 argb_u16_from_yuv161616(const struct conversion_matrix *matrix,
					      u16 y, u16 channel_1, u16 channel_2);
#endif

#endif /* _VKMS_FORMATS_H_ */
