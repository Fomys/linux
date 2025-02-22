// SPDX-License-Identifier: GPL-2.0+

#include <drm/drm_atomic_helper.h>
#include <drm/drm_edid.h>
#include <drm/drm_managed.h>
#include <drm/drm_probe_helper.h>

#include "vkms_config.h"
#include "vkms_connector.h"

static enum drm_connector_status vkms_connector_detect(struct drm_connector *connector,
						       bool force)
{
	struct vkms_connector *vkms_connector;
	enum drm_connector_status status;

	vkms_connector = drm_connector_to_vkms_connector(connector);
	status = vkms_config_connector_get_status(vkms_connector->connector_cfg);

	return status;
}

static const struct drm_connector_funcs vkms_connector_funcs = {
	.detect = vkms_connector_detect,
	.fill_modes = drm_helper_probe_single_connector_modes,
	.reset = drm_atomic_helper_connector_reset,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
};

static int vkms_connector_read_block(void *context, u8 *buf, unsigned int block, size_t len)
{
	struct vkms_config_connector *config = context;
	unsigned int edid_len;
	const u8 *edid = vkms_config_connector_get_edid(config, &edid_len);

	if (block * len + len > edid_len)
		return 1;
	memcpy(buf, &edid[block * len], len);
	return 0;
}

static int vkms_conn_get_modes(struct drm_connector *connector)
{
	struct vkms_connector *vkms_connector = drm_connector_to_vkms_connector(connector);
	const struct drm_edid *drm_edid = NULL;
	int count;
	struct vkms_config_connector *context = NULL;

	context = vkms_connector->connector_cfg;
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

static struct drm_encoder *vkms_conn_best_encoder(struct drm_connector *connector)
{
	struct drm_encoder *encoder;

	drm_connector_for_each_possible_encoder(connector, encoder)
		return encoder;

	return NULL;
}

static const struct drm_connector_helper_funcs vkms_conn_helper_funcs = {
	.get_modes    = vkms_conn_get_modes,
	.best_encoder = vkms_conn_best_encoder,
};

struct vkms_connector *vkms_connector_init(struct vkms_device *vkmsdev,
					   struct vkms_config_connector *connector_cfg)
{
	struct drm_device *dev = &vkmsdev->drm;
	struct vkms_connector *connector;
	int ret;

	connector = drmm_kzalloc(dev, sizeof(*connector), GFP_KERNEL);
	if (!connector)
		return ERR_PTR(-ENOMEM);

	connector->connector_cfg = connector_cfg;

	ret = drmm_connector_init(dev, &connector->base, &vkms_connector_funcs,
				  connector_cfg->type, NULL);
	if (ret)
		return ERR_PTR(ret);

	drm_connector_helper_add(&connector->base, &vkms_conn_helper_funcs);

	return connector;
}

void vkms_trigger_connector_hotplug(struct vkms_device *vkmsdev)
{
	struct drm_device *dev = &vkmsdev->drm;

	drm_kms_helper_hotplug_event(dev);
}
