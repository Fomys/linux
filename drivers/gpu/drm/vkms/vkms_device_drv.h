/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef _VKMS_DEVICE_DRV_H_
#define _VKMS_DEVICE_DRV_H_

#include <drm/drm_device.h>
#include <linux/platform_device.h>

#define XRES_MIN 10
#define YRES_MIN 10

#define XRES_DEF 1024
#define YRES_DEF 768

#define XRES_MAX 8192
#define YRES_MAX 8192

#define VKMS_LUT_SIZE 256

#define NUM_OVERLAY_PLANES 8

/**
 * struct vkms_device - Description of a vkms device
 *
 * @drm - Base device in drm
 */
struct vkms_device {
	struct drm_device drm;
};

struct vkms_platform_data {
	struct vkms_config *config;
};

/**
 * struct vkms_config - General configuration for VKMS driver
 *
 * @writeback: If true, a writeback buffer can be attached to the CRTC
 * @cursor: If true, a cursor plane is created in the VKMS device
 * @overlay: If true, NUM_OVERLAY_PLANES will be created for the VKMS device
 */
struct vkms_config {
	bool writeback;
	bool cursor;
	bool overlays;
};

/* Device creation with vkms_config */
struct platform_device *vkms_create_device(struct vkms_platform_data *pdata);
void vkms_delete_device(struct platform_device *pdev);
int vkms_configure_device(struct vkms_device *vkms_device,
			  struct vkms_config *vkms_config);
/**
 * vkms_config_alloc() - Allocate a configuration structure
 *
 * In order to configure vkms device, you must allocate a configuration structure. This function
 * will initializer properly all its members and set some default values.
 */
struct vkms_config *vkms_config_alloc(void);

/**
 * vkms_config_is_valid() - Validate a configuration
 *
 * Check if all the property defined in the configuration are valids. This will return false for
 * example if:
 * - no or many primary planes are present;
 * - the default rotation of a plane is not in its supported rotation;
 * - a CRTC don't have any encoder...
 *
 * @vkms_config: Configuration to validate
 */
bool vkms_config_is_valid(struct vkms_config *vkms_config);

/**
 * vkms_config_free() - Free the memory used by a VKMS config
 *
 * @vkms_config: Configuration to free
 */
void vkms_config_free(struct vkms_config *vkms_config);

#endif //_VKMS_DEVICE_DRV_H_
