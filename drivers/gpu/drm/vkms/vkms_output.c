// SPDX-License-Identifier: GPL-2.0+

#include "vkms_connector.h"
#include "vkms_drv.h"
#include "vkms_config.h"
#include <drm/drm_atomic_helper.h>
#include <drm/drm_managed.h>


int vkms_output_init(struct vkms_device *vkmsdev)
{
	struct vkms_config_encoder *config_encoder;
	struct drm_device *dev = &vkmsdev->drm;
	struct vkms_config_plane *config_plane;
	struct vkms_config_crtc *config_crtc;
	struct vkms_config_connector *config_connector;
	unsigned long idx;
	int ret;

	list_for_each_entry(config_plane, &vkmsdev->config->planes, link) {
		config_plane->plane = vkms_plane_init(vkmsdev, config_plane);
		if (IS_ERR(config_plane->plane)) {
			ret = PTR_ERR(config_plane->plane);
			return ret;
		}
	}

	list_for_each_entry(config_crtc, &vkmsdev->config->crtcs, link) {
		struct drm_plane *primary = NULL, *cursor = NULL;

		xa_for_each(&config_crtc->possible_planes, idx, config_plane) {
			if (config_plane->type == DRM_PLANE_TYPE_PRIMARY)
				primary = &config_plane->plane->base;
			else if (config_plane->type == DRM_PLANE_TYPE_CURSOR)
				cursor = &config_plane->plane->base;
		}

		config_crtc->output = vkms_crtc_init(vkmsdev, primary, cursor, config_crtc);

		if (IS_ERR(config_crtc->output)) {
			ret = PTR_ERR(config_crtc->output);
			return ret;
		}
	}

	list_for_each_entry(config_crtc, &vkmsdev->config->crtcs, link) {
		xa_for_each(&config_crtc->possible_planes, idx, config_plane) {
			config_plane->plane->base.possible_crtcs |= drm_crtc_mask(&config_crtc->output->crtc);
		}
	}

	list_for_each_entry(config_encoder, &vkmsdev->config->encoders, link) {
		config_encoder->encoder = drmm_kzalloc(dev, sizeof(*config_encoder->encoder),
						       GFP_KERNEL);
		if (!config_encoder->encoder)
			return -ENOMEM;
		ret = drmm_encoder_init(dev, config_encoder->encoder, NULL,
					DRM_MODE_ENCODER_VIRTUAL, config_encoder->name);
		if (ret) {
			DRM_ERROR("Failed to init encoder\n");
			return ret;
		}

		xa_for_each(&config_encoder->possible_crtcs, idx, config_crtc) {
			config_encoder->encoder->possible_crtcs |= drm_crtc_mask(&config_crtc->output->crtc);
		}
		if (IS_ERR(config_encoder->encoder))
			return PTR_ERR(config_encoder->encoder);
	}

	list_for_each_entry(config_connector, &vkmsdev->config->connectors, link) {
		config_connector->connector = vkms_connector_init(vkmsdev, config_connector);
		if (!config_connector->connector)
			return -ENOMEM;

		xa_for_each(&config_connector->possible_encoders, idx, config_encoder) {
			ret = drm_connector_attach_encoder(&config_connector->connector->base,
							   config_encoder->encoder);
			if (ret)
				return ret;
		}
	}

	drm_mode_config_reset(dev);

	return ret;
}
