// SPDX-License-Identifier: GPL-2.0+

#include "vkms_connector.h"
#include "vkms_drv.h"
#include "vkms_config.h"
#include <drm/drm_atomic_helper.h>
#include <drm/drm_managed.h>

int vkms_output_init(struct vkms_device *vkmsdev)
{
	struct vkms_config_plane *config_plane;
	struct drm_device *dev = &vkmsdev->drm;
	struct vkms_connector *connector;
	struct drm_encoder *encoder;
	struct vkms_output *output;
	struct vkms_plane *primary, *cursor = NULL;
	int ret;
	int writeback;

	output = vkms_crtc_init(dev, &primary->base,
				cursor ? &cursor->base : NULL);
	if (IS_ERR(output)) {
		DRM_ERROR("Failed to allocate CRTC\n");
		return PTR_ERR(output);
	}

	list_for_each_entry(config_plane, &vkmsdev->config->planes, link) {
		config_plane->plane = vkms_plane_init(vkmsdev, config_plane);
		if (IS_ERR(config_plane->plane)) {
			ret = PTR_ERR(config_plane->plane);
			return ret;
		}
		if (config_plane->type == DRM_PLANE_TYPE_PRIMARY)
			primary = config_plane->plane;
		else if (config_plane->type == DRM_PLANE_TYPE_CURSOR)
			cursor = config_plane->plane;
	}

	connector = vkms_connector_init(vkmsdev);
	if (IS_ERR(connector)) {
		DRM_ERROR("Failed to init connector\n");
		return PTR_ERR(connector);
	}

	encoder = drmm_kzalloc(dev, sizeof(*encoder), GFP_KERNEL);
	if (!encoder) {
		DRM_ERROR("Failed to allocate encoder\n");
		return -ENOMEM;
	}
	ret = drmm_encoder_init(dev, encoder, NULL,
				DRM_MODE_ENCODER_VIRTUAL, NULL);
	if (ret) {
		DRM_ERROR("Failed to init encoder\n");
		return ret;
	}
	encoder->possible_crtcs = drm_crtc_mask(&output->crtc);

	/* Attach the encoder and the connector */
	ret = drm_connector_attach_encoder(&connector->base, encoder);
	if (ret) {
		DRM_ERROR("Failed to attach connector to encoder\n");
		return ret;
	}

	/* Initialize the writeback component */
	if (vkmsdev->config->writeback) {
		writeback = vkms_enable_writeback_connector(vkmsdev, output);
		if (writeback)
			DRM_ERROR("Failed to init writeback connector\n");
	}

	drm_mode_config_reset(dev);

	return ret;
}
