/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef _VKMS_CONNECTOR_H_
#define _VKMS_CONNECTOR_H_

#include "vkms_drv.h"
#include <drm/display/drm_dp_mst_helper.h>

struct vkms_config_connector;

#define drm_connector_to_vkms_connector(target) \
	container_of(target, struct vkms_connector, base)

/**
 * struct vkms_connector - VKMS custom type wrapping around the DRM connector
 *
 * @drm: Base DRM connector
 * @connector_cfg: Connector configuration
 */
struct vkms_connector {
	struct drm_connector base;
	struct drm_dp_mst_topology_mgr mst_mgr;
	struct drm_dp_aux aux;
	
	struct drm_encoder *mst_encoders;

	struct vkms_config_connector *connector_cfg;
};

struct vkms_mst_connector {
	struct drm_connector base;
	struct vkms_connector *master_connector;
	struct drm_dp_mst_port *mst_output_port;
};


/**
 * vkms_connector_init() - Initialize a connector
 * @vkmsdev: VKMS device containing the connector
 * @connector_cfg: Configuration for the connector
 *
 * Returns:
 * The connector or an error on failure.
 */
struct vkms_connector *vkms_connector_init(struct vkms_device *vkmsdev,
					   struct vkms_config_connector *connector_cfg);

/**
 * struct vkms_device *vkmsdev() - Update the device's connectors status
 * @vkmsdev: VKMS device to update
 */
void vkms_trigger_connector_hotplug(struct vkms_device *vkmsdev);

#endif /* _VKMS_CONNECTOR_H_ */
