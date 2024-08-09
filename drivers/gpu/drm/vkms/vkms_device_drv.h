/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef _VKMS_DEVICE_DRV_H_
#define _VKMS_DEVICE_DRV_H_

#include <drm/drm_device.h>
#include <drm/drm_plane.h>
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
 * @planes: List of planes configured for this device. They are created by the function
 *          vkms_config_create_plane().
 */
struct vkms_config {
	bool writeback;

	struct list_head planes;
};

/**
 * struct vkms_config_plane
 *
 * @link: Link to the others planes
 * @name: Name of the plane
 * @type: Type of the plane. The creator of configuration needs to ensures that at least one
 *        plane is primary.
 * @plane: Internal usage. This pointer should never be considered as valid. It can be used to
 *         store a temporary reference to a vkms plane during device creation. This pointer is
 *         not managed by the configuration and must be managed by other means.
 */
struct vkms_config_plane {
	struct list_head link;

	char *name;
	enum drm_plane_type type;

	/* Internal usage */
	struct vkms_plane *plane;
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
 * vkms_config_create_plane() - Create a plane configuration
 *
 * This will allocate and add a new plane to @vkms_config. This plane will have by default the
 * maximum supported values.
 * @vkms_config: Configuration where to insert new plane
 */
struct vkms_config_plane *vkms_config_create_plane(struct vkms_config *vkms_config);


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
/**
 * vkms_config_delete_plane() - Remove a plane configuration and frees its memory
 *
 * This will delete a plane configuration from the parent configuration. This will NOT
 * cleanup and frees the vkms_plane that can be stored in @vkms_config_plane.
 * @vkms_config_plane: Plane configuration to cleanup
 */
void vkms_config_delete_plane(struct vkms_config_plane *vkms_config_plane);
#endif //_VKMS_DEVICE_DRV_H_
