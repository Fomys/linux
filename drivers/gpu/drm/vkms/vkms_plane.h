/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef _VKMS_PLANE_H
#define _VKMS_PLANE_H

#include <drm/drm_framebuffer.h>
#include <drm/drm_plane.h>
#include <drm/drm_gem_atomic_helper.h>
#include <linux/iosys-map.h>

#include "vkms_drv.h"
#include "vkms_formats.h"

struct vkms_plane {
	struct drm_plane base;
};

/**
 * struct vkms_plane_state - Driver specific plane state
 * @base: base plane state
 * @frame_info: data required for composing computation
 * @pixel_read_line: function to read a pixel line in this plane. The creator of a vkms_plane_state
 * must ensure that this pointer is valid
 * @conversion_matrix: matrix used for yuv formats to convert to rgb
 */
struct vkms_plane_state {
	struct drm_shadow_plane_state base;
	struct vkms_frame_info *frame_info;
	pixel_read_line_t pixel_read_line;
	struct conversion_matrix conversion_matrix;
};

/**
 * struct vkms_frame_info - structure to store the state of a frame
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

/**
 * vkms_plane_init() - Initialize a plane
 *
 * @vkmsdev: vkms device containing the plane
 * @type: type of plane to initialize
 * @possible_crtc_index: Crtc which can be attached to the plane. The caller must ensure that
 * possible_crtc_index is positive and less or equals to 31.
 */
struct vkms_plane *vkms_plane_init(struct vkms_device *vkmsdev,
				   enum drm_plane_type type, int possible_crtc_index);

#define drm_plane_state_to_vkms_plane_state(target) \
	container_of(target, struct vkms_plane_state, base.base)

#endif //_VKMS_PLANE_H
