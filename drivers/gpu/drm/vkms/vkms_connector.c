// SPDX-License-Identifier: GPL-2.0+

#include <drm/drm_atomic_helper.h>
#include <drm/drm_edid.h>
#include <drm/drm_managed.h>
#include <drm/drm_probe_helper.h>

#include "vkms_connector.h"

#include "vkms_config.h"

static struct vkms_config_connector *vkms_config_connector_from_vkms_connector(struct drm_connector *connector)
{
	struct vkms_connector *vkms_connector = drm_connector_to_vkms_connector(connector);
	struct vkms_device *vkms_device = drm_device_to_vkms_device(connector->dev);
	struct vkms_config_connector *connector_cfg;

	list_for_each_entry(connector_cfg, &vkms_device->config->connectors, link) {
		if (connector_cfg->connector == vkms_connector)
			return connector_cfg;
	}
	return NULL;
}

static enum drm_connector_status vkms_connector_detect(struct drm_connector *connector, bool force)
{
	enum drm_connector_status status = connector->status;
	struct vkms_config_connector *connector_cfg = vkms_config_connector_from_vkms_connector(connector);

	if (connector_cfg)
		status = connector_cfg->status;

	return status;
}
static const struct drm_connector_funcs vkms_connector_funcs = {
	.fill_modes = drm_helper_probe_single_connector_modes,
	.reset = drm_atomic_helper_connector_reset,
	.detect = vkms_connector_detect,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
};

static int vkms_connector_read_block(void *context, u8 *buf, unsigned int block, size_t len)
{
	struct vkms_config_connector *config = context;

	if (block * len + len > config->edid_blob_len)
		return 1;
	memcpy(buf, &config->edid_blob[block * len], len);
	return 0;
}

static int vkms_conn_get_modes(struct drm_connector *connector)
{
	const struct drm_edid *drm_edid = NULL;
	int count;
	struct vkms_config_connector *connector_cfg;
	struct vkms_device *vkmsdev = drm_device_to_vkms_device(connector->dev);
	struct vkms_config_connector *context = NULL;

	list_for_each_entry(connector_cfg, &vkmsdev->config->connectors, link) {
		if (connector_cfg->connector == connector) {
			context = connector_cfg;
			break;
		}
	}
	if (context)
		drm_edid = drm_edid_read_custom(connector, vkms_connector_read_block, context);

	/*
	 * Unconditionally update the connector. If the EDID was read
	 * successfully, fill in the connector information derived from the
	 * EDID. Otherwise, if the EDID is NULL, clear the connector
	 * information.
	 */
	drm_edid_connector_update(connector, drm_edid);

	count = drm_edid_connector_add_modes(connector);

	drm_edid_free(drm_edid);

	return count;
}


static const struct drm_connector_helper_funcs vkms_conn_helper_funcs = {
	.get_modes    = vkms_conn_get_modes,
};

struct vkms_connector *vkms_connector_init(struct vkms_device *vkmsdev, struct vkms_config_connector *config_connector)
{
	struct drm_device *dev = &vkmsdev->drm;
	struct vkms_connector *connector;
	int ret;

	connector = drmm_kzalloc(dev, sizeof(*connector), GFP_KERNEL);
	if (!connector)
		return ERR_PTR(-ENOMEM);

	ret = drmm_connector_init(dev, &connector->base, &vkms_connector_funcs,
				  config_connector->type, NULL);
	if (ret)
		return ERR_PTR(ret);

	drm_connector_helper_add(&connector->base, &vkms_conn_helper_funcs);

	return connector;
}
