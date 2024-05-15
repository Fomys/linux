/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef _VKMS_CRTC_H
#define _VKMS_CRTC_H

#include <drm/drm_writeback.h>
#include <drm/drm_crtc.h>
#include <linux/workqueue_types.h>

#include "vkms_writeback.h"
#include "vkms_plane.h"

/**
 * struct vkms_crtc_state - Driver specific CRTC state
 *
 * @base: base CRTC state
 * @composer_work: work struct to compose and add CRC entries
 *
 * @num_active_planes: Number of active planes
 * @active_planes: List containing all the active planes (counted by
 *  @num_active_planes). They should be stored in z-order.
 * @active_writeback: Current active writeback job
 * @gamma_lut: Look up table for gamma used in this CRTC
 * @crc_pending: Protected by @vkms_output.composer_lock.
 * @wb_pending: Protected by @vkms_output.composer_lock.
 * @frame_start: Protected by @vkms_output.composer_lock.
 * @frame_end: Protected by @vkms_output.composer_lock.
 */
struct vkms_crtc_state {
	struct drm_crtc_state base;
	struct work_struct composer_work;

	int num_active_planes;
	struct vkms_plane_state **active_planes;
	struct vkms_writeback_job *active_writeback;
	struct vkms_color_lut gamma_lut;

	bool crc_pending;
	bool wb_pending;
	u64 frame_start;
	u64 frame_end;
};

/**
 * struct vkms_crtc - crtc internal representation
 *
 * @crtc: Base crtc in drm
 * @wb_connecter: DRM writeback connector used for this output
 * @vblank_hrtimer:
 * @period_ns:
 * @event:
 * @composer_workq: Ordered workqueue for composer_work
 * @lock: Lock used to project concurrent acces to the composer
 * @composer_enabled: Protected by @lock.
 * @composer_lock: Lock used internally to protect @composer_state members
 * @composer_state: Protected by @lock.
 */
struct vkms_crtc {
	struct drm_crtc base;

	struct drm_writeback_connector wb_connector;
	struct hrtimer vblank_hrtimer;
	ktime_t period_ns;
	struct drm_pending_vblank_event *event;
	struct workqueue_struct *composer_workq;
	spinlock_t lock;

	bool composer_enabled;
	struct vkms_crtc_state *composer_state;

	spinlock_t composer_lock;
};

#define to_vkms_crtc_state(target)\
	container_of(target, struct vkms_crtc_state, base)

/**
 * vkms_crtc_init() - Initialize a crtc for vkms
 * @dev: drm_device associated with the vkms buffer
 * @crtc: uninitialized crtc device
 * @primary: primary plane to attach to the crtc
 * @cursor plane to attach to the crtc
 */
int vkms_crtc_init(struct drm_device *dev, struct drm_crtc *crtc,
		   struct drm_plane *primary, struct drm_plane *cursor);

#endif //_VKMS_CRTC_H
