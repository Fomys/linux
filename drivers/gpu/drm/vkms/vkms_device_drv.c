// SPDX-License-Identifier: GPL-2.0+

#include <linux/platform_device.h>

#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <drm/drm_drv.h>
#include <drm/drm_vblank.h>
#include <drm/drm_debugfs.h>
#include <drm/drm_fbdev_shmem.h>
#include <drm/drm_edid.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_managed.h>

#include "vkms_device_drv.h"
#include "vkms_crtc.h"
#include "vkms_plane.h"
#include "vkms_writeback.h"

static const struct drm_connector_funcs vkms_connector_funcs = {
	.fill_modes = drm_helper_probe_single_connector_modes,
	.reset = drm_atomic_helper_connector_reset,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
};

static const struct drm_encoder_funcs vkms_encoder_funcs = {};

static int vkms_conn_get_modes(struct drm_connector *connector)
{
	int count;

	/* Use the default modes list from drm */
	count = drm_add_modes_noedid(connector, XRES_MAX, YRES_MAX);
	drm_set_preferred_mode(connector, XRES_DEF, YRES_DEF);

	return count;
}

static const struct drm_connector_helper_funcs vkms_conn_helper_funcs = {
	.get_modes = vkms_conn_get_modes,
};

static struct drm_connector *vkms_connector_init(struct vkms_device *vkmsdev)
{
	struct drm_connector *connector;
	int ret;

	connector = drmm_kzalloc(&vkmsdev->drm, sizeof(*connector), GFP_KERNEL);

	ret = drmm_connector_init(&vkmsdev->drm, connector,
				  &vkms_connector_funcs,
				  DRM_MODE_CONNECTOR_VIRTUAL, NULL);
	if (ret)
		goto err_connector_init;

	drm_connector_helper_add(connector, &vkms_conn_helper_funcs);

	return connector;

err_connector_init:
	drmm_kfree(&vkmsdev->drm, connector);
	return ERR_PTR(ret);
}

static struct drm_encoder *vkms_encoder_init(struct vkms_device *vkmsdev)
{
	struct drm_encoder *encoder;
	int ret;

	encoder = drmm_kzalloc(&vkmsdev->drm, sizeof(*encoder), GFP_KERNEL);

	ret = drmm_encoder_init(&vkmsdev->drm, encoder, &vkms_encoder_funcs,
				DRM_MODE_ENCODER_VIRTUAL, NULL);
	if (ret)
		goto err_encoder_init;

	return encoder;

err_encoder_init:
	drmm_kfree(&vkmsdev->drm, encoder);
	return ERR_PTR(ret);
}

/*
 * This function assumes that the configuration is valid
 */
static int vkms_output_init(struct vkms_device *vkms_device,
			    struct vkms_config *vkms_config)
{
	struct drm_device *dev = &vkms_device->drm;
	struct vkms_plane *primary, *cursor = NULL;
	struct vkms_config_plane *config_plane;
	struct drm_connector *connector;
	struct drm_encoder *encoder;
	struct vkms_crtc *vkms_crtc;
	struct drm_plane *plane;
	int ret;
	int writeback;
	unsigned int n;

	list_for_each_entry(config_plane, &vkms_config->planes, link) {
		config_plane->plane = vkms_plane_init(vkms_device, config_plane);
		if (IS_ERR(config_plane->plane)) {
			ret = PTR_ERR(config_plane->plane);
			return ret;
		}

		if (config_plane->type == DRM_PLANE_TYPE_PRIMARY)
			primary = config_plane->plane;
		else if (config_plane->type == DRM_PLANE_TYPE_CURSOR)
			cursor = config_plane->plane;

	}

	/* [1]: Initialize the crtc component */
	vkms_crtc = vkms_crtc_init(vkms_device, &primary->base,
				   cursor ? &cursor->base : NULL);
	if (IS_ERR(vkms_crtc))
		return PTR_ERR(vkms_crtc);

	/* Enable the output's CRTC for all the planes */
	drm_for_each_plane(plane, &vkms_device->drm) {
		plane->possible_crtcs |= drm_crtc_mask(&vkms_crtc->base);
	}

	connector = vkms_connector_init(vkms_device);
	if (IS_ERR(connector))
		return PTR_ERR(connector);

	encoder = vkms_encoder_init(vkms_device);
	if (IS_ERR(encoder)) {
		DRM_ERROR("Failed to init encoder\n");
		return PTR_ERR(encoder);
	}
	drm_connector_helper_add(connector, &vkms_conn_helper_funcs);
	encoder->possible_crtcs |= drm_crtc_mask(&vkms_crtc->base);

	/* Attach the encoder and the connector */
	ret = drm_connector_attach_encoder(connector, encoder);
	if (ret) {
		DRM_ERROR("Failed to attach connector to encoder\n");
		return ret;
	}

	/* Initialize the writeback component */
	if (vkms_config->writeback) {
		writeback = vkms_enable_writeback_connector(vkms_device, vkms_crtc);
		if (writeback)
			DRM_ERROR("Failed to init writeback connector\n");
	}

	drm_mode_config_reset(dev);

	return 0;
}

static int vkms_atomic_check(struct drm_device *dev, struct drm_atomic_state *state)
{
	struct drm_crtc *crtc;
	struct drm_crtc_state *new_crtc_state;
	int i;

	for_each_new_crtc_in_state(state, crtc, new_crtc_state, i) {
		if (!new_crtc_state->gamma_lut || !new_crtc_state->color_mgmt_changed)
			continue;

		if (new_crtc_state->gamma_lut->length / sizeof(struct drm_color_lut *)
		    > VKMS_LUT_SIZE)
			return -EINVAL;
	}

	return drm_atomic_helper_check(dev, state);
}

static const struct drm_mode_config_funcs vkms_mode_funcs = {
	.fb_create = drm_gem_fb_create,
	.atomic_check = vkms_atomic_check,
	.atomic_commit = drm_atomic_helper_commit,
};

static void vkms_atomic_commit_tail(struct drm_atomic_state *old_state)
{
	struct drm_device *dev = old_state->dev;
	struct drm_crtc *crtc;
	struct drm_crtc_state *old_crtc_state;
	int i;

	drm_atomic_helper_commit_modeset_disables(dev, old_state);

	drm_atomic_helper_commit_planes(dev, old_state, 0);

	drm_atomic_helper_commit_modeset_enables(dev, old_state);

	drm_atomic_helper_fake_vblank(old_state);

	drm_atomic_helper_commit_hw_done(old_state);

	drm_atomic_helper_wait_for_flip_done(dev, old_state);

	for_each_old_crtc_in_state(old_state, crtc, old_crtc_state, i) {
		struct vkms_crtc_state *vkms_state =
			drm_crtc_state_to_vkms_crtc_state(old_crtc_state);

		flush_work(&vkms_state->composer_work);
	}

	drm_atomic_helper_cleanup_planes(dev, old_state);
}

static const struct drm_mode_config_helper_funcs vkms_mode_config_helpers = {
	.atomic_commit_tail = vkms_atomic_commit_tail,
};

static int vkms_modeset_init(struct vkms_device *vkms_device,
			     struct vkms_config *vkms_config)
{
	struct drm_device *dev = &vkms_device->drm;
	int ret;

	ret = drmm_mode_config_init(dev);
	if (ret)
		return ret;

	dev->mode_config.funcs = &vkms_mode_funcs;
	dev->mode_config.min_width = XRES_MIN;
	dev->mode_config.min_height = YRES_MIN;
	dev->mode_config.max_width = XRES_MAX;
	dev->mode_config.max_height = YRES_MAX;
	dev->mode_config.cursor_width = 512;
	dev->mode_config.cursor_height = 512;
	/*
	 * FIXME: There's a confusion between bpp and depth between this and
	 * fbdev helpers. We have to go with 0, meaning "pick the default",
	 * which is XRGB8888 in all cases.
	 */
	dev->mode_config.preferred_depth = 0;
	dev->mode_config.helper_private = &vkms_mode_config_helpers;
	return vkms_output_init(vkms_device, vkms_config);
}

int vkms_configure_device(struct vkms_device *vkms_device,
			  struct vkms_config *vkms_config)
{
	int ret;

	ret = dma_coerce_mask_and_coherent(vkms_device->drm.dev,
					   DMA_BIT_MASK(64));
	if (ret) {
		DRM_DEV_ERROR(vkms_device->drm.dev, "Could not initialize DMA support.\n");
		return ret;
	}

	ret = vkms_modeset_init(vkms_device, vkms_config);
	if (ret) {
		DRM_DEV_ERROR(vkms_device->drm.dev, "Failed to initialize modeset.\n");
		return ret;
	}

	/*
	 * Only one CRTC is used. Once the ConfigFS interface is implemented, this should adapt
	 * to the ConfigFS request
	 */
	ret = drm_vblank_init(&vkms_device->drm, 1);
	if (ret) {
		DRM_DEV_ERROR(vkms_device->drm.dev, "Failed to initialize vblank.\n");
		return ret;
	}

	ret = drm_dev_register(&vkms_device->drm, 0);
	if (ret) {
		DRM_DEV_ERROR(vkms_device->drm.dev, "Failed to register the device.\n");
		return ret;
	}

	drm_fbdev_shmem_setup(&vkms_device->drm, 0);
	return 0;
}


struct platform_device *vkms_create_device(struct vkms_platform_data *pdata)
{
	return platform_device_register_resndata(NULL, "vkms", PLATFORM_DEVID_AUTO,
						 NULL, 0, pdata, sizeof(&pdata));
}


struct vkms_config *vkms_config_alloc(void)
{
	struct vkms_config *vkms_config = kzalloc(sizeof(*vkms_config), GFP_KERNEL);

	if (!vkms_config)
		return NULL;
	vkms_config->writeback = false;
	INIT_LIST_HEAD(&vkms_config->planes);

	return vkms_config;
}

struct vkms_config_plane *vkms_config_create_plane(struct vkms_config *vkms_config)
{
	if (!vkms_config)
		return NULL;

	struct vkms_config_plane *vkms_config_overlay = kzalloc(sizeof(*vkms_config_overlay),
								GFP_KERNEL);

	if (!vkms_config_overlay)
		return NULL;

	vkms_config_overlay->name = NULL;
	vkms_config_overlay->type = DRM_PLANE_TYPE_OVERLAY;
	vkms_config_overlay->supported_rotations = DRM_MODE_ROTATE_MASK | DRM_MODE_REFLECT_MASK;
	vkms_config_overlay->default_rotation = DRM_MODE_ROTATE_0;

	list_add(&vkms_config_overlay->link, &vkms_config->planes);

	return vkms_config_overlay;
}

void vkms_config_delete_plane(struct vkms_config_plane *vkms_config_overlay)
{
	if (!vkms_config_overlay)
		return;
	list_del(&vkms_config_overlay->link);
	kfree(vkms_config_overlay->name);
	kfree(vkms_config_overlay);
}

void vkms_config_free(struct vkms_config *vkms_config)
{
	struct vkms_config_plane *vkms_config_plane, *tmp_plane;

	list_for_each_entry_safe(vkms_config_plane, tmp_plane, &vkms_config->planes, link) {
		vkms_config_delete_plane(vkms_config_plane);
	}
	kfree(vkms_config);
}

bool vkms_config_is_valid(struct vkms_config *vkms_config)
{
	struct vkms_config_plane *config_plane;

	bool has_cursor = false;
	bool has_primary = false;

	list_for_each_entry(config_plane, &vkms_config->planes, link) {
		// Default rotation not in supported rotations
		if ((config_plane->default_rotation & config_plane->supported_rotations) !=
		    config_plane->default_rotation)
			return false;

		if (config_plane->type == DRM_PLANE_TYPE_PRIMARY) {
			// Multiple primary planes for only one CRTC
			if (has_primary)
				return false;
			has_primary = true;
		}
		if (config_plane->type == DRM_PLANE_TYPE_CURSOR) {
			// Multiple cursor planes for only one CRTC
			if (has_cursor)
				return false;
			has_cursor = true;
		}
	}

	if (!has_primary)
		return false;

	return true;
}
