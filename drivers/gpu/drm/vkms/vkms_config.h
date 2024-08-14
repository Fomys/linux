/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef _VKMS_CONFIG_H
#define _VKMS_CONFIG_H

#include <linux/types.h>
#include "vkms_drv.h"

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
 * vkms_config_register_debugfs() - Register the debugfs file to display current configuration
 */
void vkms_config_register_debugfs(struct vkms_device *vkms_device);

struct vkms_config *vkms_config_create(void);
void vkms_config_destroy(struct vkms_config *config);

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

#endif //_VKMS_CONFIG_H
