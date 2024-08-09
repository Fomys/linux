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
 * struct vkms_config - General configuration for VKMS driver
 *
 * @writeback: If true, a writeback buffer can be attached to the CRTC
 * @cursor: If true, a cursor plane is created in the VKMS device
 * @overlay: If true, NUM_OVERLAY_PLANES will be created for the VKMS device
 * @dev: Used to store the current vkms device. Only set when the device is instancied.
 */
struct vkms_config {
	bool writeback;
	bool cursor;
	bool overlay;
	struct vkms_device *dev;
};

/**
 * struct vkms_device - Description of a vkms device
 *
 * @drm - Base device in drm
 * @platform - Associated platform device
 * @output - Configuration and sub-components of the vkms device
 * @config: Configuration used in this vkms device
 */
struct vkms_device {
	struct drm_device drm;
	struct platform_device *platform;
	const struct vkms_config *config;
};

/*
 * The following helpers are used to convert a member of a struct into its parent.
 */

#define drm_device_to_vkms_device(target) \
	container_of(target, struct vkms_device, drm)

#endif /* _VKMS_DRV_H_ */
