/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef _VKMS_CONFIG_H
#define _VKMS_CONFIG_H

#include <linux/types.h>
#include "vkms_drv.h"

/**
 * struct vkms_config - General configuration for VKMS driver
 *
 * @planes: List of planes configured for this device. They are created by the function
 *          vkms_config_create_plane().
 * @crtcs: List of crtcs configured for this device. They are created by the function
 *         vkms_config_create_crtc().
 * @encoders: List of encoders configured for this device. They are created by the function
 *            vkms_config_create_encoder().
 * @dev: Used to store the current vkms device. Only set when the device is instancied.
 */
struct vkms_config {
	struct vkms_device *dev;

	struct list_head planes;
	struct list_head crtcs;
	struct list_head encoders;
};

/**
 * struct vkms_config_crtc
 *
 * @link: Link to the others CRTCs
 * @name: Name of the CRTC
 * @possible_planes: List of planes that can be used with this CRTC
 * @possible_encoders: List of encoders that can be used with this CRTC
 * @crtc: Internal usage. This pointer should never be considered as valid. It can be used to
 *         store a temporary reference to a vkms crtc during device creation. This pointer is
 *         not managed by the configuration and must be managed by other means.
 */
struct vkms_config_crtc {
	struct list_head link;

	char *name;
	bool writeback;
	struct xarray possible_planes;
	struct xarray possible_encoders;

	/* Internal usage */
	struct vkms_output *output;
};

/**
 * struct vkms_config_encoder
 *
 * @link: Link to the others encoders
 * @name: Name of the encoder
 * @possible_crtcs: List of CRTC that can be used with this encoder
 * @encoder: Internal usage. This pointer should never be considered as valid. It can be used to
 *         store a temporary reference to a vkms encoder during device creation. This pointer is
 *         not managed by the configuration and must be managed by other means.
 */
struct vkms_config_encoder {
	struct list_head link;

	char *name;
	struct xarray possible_crtcs;

	/* Internal usage */
	struct drm_encoder *encoder;
};

/**
 * struct vkms_config_plane
 *
 * @link: Link to the others planes
 * @name: Name of the plane
 * @type: Type of the plane. The creator of configuration needs to ensures that at least one
 *        plane is primary.
 * @default_rotation: Default rotation that should be used by this plane
 * @supported_rotation: Rotation that this plane will support
 * @default_color_encoding: Default color encoding that should be used by this plane
 * @supported_color_encoding: Color encoding that this plane will support
 * @default_color_range: Default color range that should be used by this plane
 * @supported_color_range: Color range that this plane will support
 * @possible_crtcs: List of CRTC that can be used with this plane.
 * @plane: Internal usage. This pointer should never be considered as valid. It can be used to
 *         store a temporary reference to a vkms plane during device creation. This pointer is
 *         not managed by the configuration and must be managed by other means.
 */
struct vkms_config_plane {
	struct list_head link;

	char *name;
	enum drm_plane_type type;
	unsigned int default_rotation;
	unsigned int supported_rotations;
	enum drm_color_encoding default_color_encoding;
	unsigned int supported_color_encoding;
	enum drm_color_range default_color_range;
	unsigned int supported_color_range;

	struct xarray possible_crtcs;
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
 * vkms_config_create_crtc() - Create a crtc configuration
 *
 * This will allocate and add a new crtc configuration to @vkms_config.
 * @vkms_config: Configuration where to insert new crtc configuration
 */
struct vkms_config_crtc *vkms_config_create_crtc(struct vkms_config *vkms_config);

/**
 * vkms_config_create_encoder() - Create an encoder configuration
 *
 * This will allocate and add a new encoder configuration to @vkms_config.
 * @vkms_config: Configuration where to insert new encoder configuration
 */
struct vkms_config_encoder *vkms_config_create_encoder(struct vkms_config *vkms_config);

int __must_check vkms_config_plane_attach_crtc(struct vkms_config_plane *vkms_config_plane,
					       struct vkms_config_crtc *vkms_config_crtc);
int __must_check vkms_config_encoder_attach_crtc(struct vkms_config_encoder *vkms_config_encoder,
						 struct vkms_config_crtc *vkms_config_crtc);

/**
 * vkms_config_delete_plane() - Remove a plane configuration and frees its memory
 *
 * This will delete a plane configuration from the parent configuration. This will NOT
 * cleanup and frees the vkms_plane that can be stored in @vkms_config_plane. It will also remove
 * any reference to this plane in @vkms_config.
 *
 * @vkms_config_plane: Plane configuration to cleanup
 * @vkms_config: Parent configuration
 */
void vkms_config_delete_plane(struct vkms_config_plane *vkms_config_plane,
			      struct vkms_config *vkms_config);
/**
 * vkms_config_delete_crtc() - Remove a CRTC configuration and frees its memory
 *
 * This will delete a CRTC configuration from the parent configuration. This will NOT
 * cleanup and frees the vkms_crtc that can be stored in @vkms_config_crtc. It will also remove
 * any reference to this CRTC in @vkms_config.
 *
 * @vkms_config_crtc: Plane configuration to cleanup
 * @vkms_config: Parent configuration
 */
void vkms_config_delete_crtc(struct vkms_config_crtc *vkms_config_crtc,
			     struct vkms_config *vkms_config);
/**
 * vkms_config_delete_encoder() - Remove an encoder configuration and frees its memory
 *
 * This will delete an encoder configuration from the parent configuration. This will NOT
 * cleanup and frees the vkms_encoder that can be stored in @vkms_config_encoder. It will also
 * remove any reference to this CRTC in @vkms_config.
 *
 * @vkms_config_encoder: Plane configuration to cleanup
 * @vkms_config: Parent configuration
 */
void vkms_config_delete_encoder(struct vkms_config_encoder *vkms_config_encoder,
				struct vkms_config *vkms_config);

/**
 * vkms_config_alloc_default() - Allocate the configuration for the default device
 * @enable_writeback: Enable the writeback connector for this configuration
 * @enable_overlay: Create some overlay planes
 * @enable_cursor:  Create a cursor plane
 */
struct vkms_config *vkms_config_alloc_default(bool enable_writeback, bool enable_overlay,
					      bool enable_cursor);

#endif //_VKMS_CONFIG_H
