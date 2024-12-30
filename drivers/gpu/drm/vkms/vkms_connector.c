// SPDX-License-Identifier: GPL-2.0+

#include <drm/drm_atomic_helper.h>
#include <drm/drm_edid.h>
#include <drm/drm_managed.h>
#include <drm/drm_probe_helper.h>

#include "vkms_config.h"
#include "vkms_connector.h"

static ssize_t vkms_connector_mst_transfer(struct drm_dp_aux *aux, struct drm_dp_aux_msg *msg)
{
	return -ETIMEDOUT;
}

static const struct drm_connector_funcs vkms_mst_connector_funcs = {
	.fill_modes = drm_helper_probe_single_connector_modes,
	.reset = drm_atomic_helper_connector_reset,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
	.destroy = drm_connector_cleanup
};

static int
vkms_mst_detect_ctx(struct drm_connector *connector,
		    struct drm_modeset_acquire_ctx *ctx, bool force)
{
	struct vkms_mst_connector *vkms_mst_connector =
		container_of(connector, struct vkms_mst_connector, base);

	if (drm_connector_is_unregistered(connector))
		return connector_status_disconnected;
	int connection_status = drm_dp_mst_detect_port(connector, ctx,
						       vkms_mst_connector->mst_output_port->mgr,
						       vkms_mst_connector->mst_output_port);

	return connection_status;
}

static int vkms_mst_get_modes(struct drm_connector *connector)
{
	struct vkms_mst_connector *vkms_mst_connector =
		container_of(connector, struct vkms_mst_connector, base);
	const struct drm_edid *drm_edid;
	int count;

	if (drm_connector_is_unregistered(connector)) {
		return 0;
	}

	drm_edid = drm_dp_mst_edid_read(
		connector, vkms_mst_connector->mst_output_port->mgr,
		vkms_mst_connector->mst_output_port);

	drm_edid_connector_update(connector, drm_edid);

	count = drm_edid_connector_add_modes(connector);

	drm_edid_free(drm_edid);

	return count;
}

static struct drm_encoder *
vkms_mst_atomic_best_encoder(struct drm_connector *connector,
			     struct drm_atomic_state *state)
{
	struct vkms_mst_connector *vkms_mst_connector =
		container_of(connector, struct vkms_mst_connector, base);

	struct vkms_connector *vkms_master_connector = vkms_mst_connector->master_connector;

	if(vkms_master_connector->connector_cfg->encoder_count)
		return &vkms_mst_connector->master_connector->mst_encoders[0];
	return NULL;
}

static const struct drm_connector_helper_funcs vkms_mst_connector_helper_funcs = {
	.get_modes = vkms_mst_get_modes,
	.detect_ctx = vkms_mst_detect_ctx,
	.atomic_best_encoder = vkms_mst_atomic_best_encoder
};

static struct drm_connector *vkms_mst_add_connector(struct drm_dp_mst_topology_mgr *mgr,
					   struct drm_dp_mst_port *port, const char *path)
{
	struct vkms_connector *vkms_master_connector =
		container_of(mgr, struct vkms_connector, mst_mgr);
	struct drm_device *dev = vkms_master_connector->base.dev;
	struct vkms_device *vkms_device = drm_device_to_vkms_device(dev);
	struct vkms_mst_connector *vkms_mst_connector;
	struct drm_connector *connector;

	vkms_mst_connector = drmm_kzalloc(&vkms_device->drm, sizeof(*vkms_mst_connector), GFP_KERNEL);
	if (!vkms_mst_connector)
		return ERR_PTR(-ENOMEM);

	drm_dp_mst_get_port_malloc(port);
	vkms_mst_connector->mst_output_port = port;
	vkms_mst_connector->master_connector = vkms_master_connector;

	connector = &vkms_mst_connector->base;
	if (drm_connector_dynamic_init(dev, connector, &vkms_mst_connector_funcs,
				       DRM_MODE_CONNECTOR_DisplayPort, NULL)) {
		drmm_kfree(dev, vkms_mst_connector);
		drm_dp_mst_put_port_malloc(vkms_mst_connector->mst_output_port);
		return NULL;
	}
	drm_connector_helper_add(connector, &vkms_mst_connector_helper_funcs);
	drm_object_attach_property(
			&connector->base,
			dev->mode_config.path_property,
			0);
	drm_connector_set_path_property(connector, path);

	connector->funcs->reset(connector);

	struct vkms_config_encoder *cfg_encoder = NULL;
	unsigned long idx;

	vkms_config_connector_for_each_possible_encoder(vkms_master_connector->connector_cfg, idx, cfg_encoder)
		drm_connector_attach_encoder(connector, cfg_encoder->encoder);



	return connector;
}

static const struct drm_dp_mst_topology_cbs mst_cbs = {
	.add_connector = vkms_mst_add_connector,
};

static enum drm_connector_status vkms_connector_detect(struct drm_connector *connector,
						       bool force)
{
	struct vkms_connector *vkms_connector;
	enum drm_connector_status status;

	vkms_connector = drm_connector_to_vkms_connector(connector);
	if (!vkms_connector->connector_cfg->mst_support)
		status = vkms_config_connector_get_status(vkms_connector->connector_cfg);
	else
		status = connector_status_disconnected;

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
	if (connector->connector_type == DRM_MODE_CONNECTOR_DisplayPort && context->mst_support)
		drm_edid = drm_edid_read_ddc(connector, &vkms_connector->aux.ddc);
	else if (context)
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

	if (vkms_config_connector_get_type(connector_cfg) == DRM_MODE_CONNECTOR_DisplayPort && connector_cfg->mst_support) {
		connector->mst_encoders = drmm_kzalloc(dev, sizeof(*connector->mst_encoders) * connector_cfg->encoder_count, GFP_KERNEL);
		for (int i = 0; i < connector_cfg->encoder_count; i++) {
			struct drm_crtc *crtc;
			drm_for_each_crtc(crtc, dev)
			{
				connector->mst_encoders[i].possible_crtcs |= drm_crtc_mask(crtc);
			}
			drmm_encoder_init(dev,
					  &connector->mst_encoders[i],
					  NULL, DRM_MODE_ENCODER_DPMST,
					  "MST %d", i);
		}


		connector->aux.transfer = vkms_connector_mst_transfer;
		connector->aux.drm_dev = dev;
		connector->aux.name = "MST AUX";
		connector->mst_mgr.cbs = &mst_cbs;

		ret = drm_dp_mst_topology_mgr_init(
			&connector->mst_mgr, dev, &connector->aux, 16,
			connector_cfg->encoder_count,
			connector->base.base.id);

		drm_dp_aux_register(&connector->aux);
		drm_dp_mst_topology_mgr_set_mst(&connector->mst_mgr, true);
	}

	return connector;
}

void vkms_trigger_connector_hotplug(struct vkms_device *vkmsdev)
{
	struct drm_device *dev = &vkmsdev->drm;

	drm_kms_helper_hotplug_event(dev);
}
