/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef _VKMS_CONFIG_H
#define _VKMS_CONFIG_H

#include <linux/types.h>
#include "vkms_drv.h"

/**
 * struct vkms_config - General configuration for VKMS driver
 *
 * @writeback: If true, a writeback buffer can be attached to the CRTC
 * @planes: List of planes configured for this device. They are created by the function
 *          vkms_config_create_plane().
 * @dev: Used to store the current vkms device. Only set when the device is instancied.
 */
struct vkms_config {
	bool writeback;
	bool cursor;
	bool overlay;
	struct vkms_device *dev;

	struct list_head planes;
};

/**
 * struct vkms_config_plane
 *
 * @link: Link to the others planes
 * @type: Type of the plane. The creator of configuration needs to ensures that at least one
 *        plane is primary.
 * @plane: Internal usage. This pointer should never be considered as valid. It can be used to
 *         store a temporary reference to a vkms plane during device creation. This pointer is
 *         not managed by the configuration and must be managed by other means.
 */
struct vkms_config_plane {
	struct list_head link;

	enum drm_plane_type type;

	/* Internal usage */
	struct vkms_plane *plane;
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

/**
 * vkms_config_create_plane() - Create a plane configuration
 *
 * This will allocate and add a new plane to @vkms_config. This plane will have by default the
 * maximum supported values.
 * @vkms_config: Configuration where to insert new plane
 */
struct vkms_config_plane *vkms_config_create_plane(struct vkms_config *vkms_config);

/**
 * vkms_config_delete_plane() - Remove a plane configuration and frees its memory
 *
 * This will delete a plane configuration from the parent configuration. This will NOT
 * cleanup and frees the vkms_plane that can be stored in @vkms_config_plane.
 * @vkms_config_plane: Plane configuration to cleanup
 */
void vkms_config_delete_plane(struct vkms_config_plane *vkms_config_plane);

/**
 * vkms_config_alloc_default() - Allocate the configuration for the default device
 * @enable_writeback: Enable the writeback connector for this configuration
 * @enable_overlay: Create some overlay planes
 * @enable_cursor:  Create a cursor plane
 */
struct vkms_config *vkms_config_alloc_default(bool enable_writeback, bool enable_overlay,
					      bool enable_cursor);

#endif //_VKMS_CONFIG_H
