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

static struct drm_encoder *vkms_encoder_init(struct vkms_device *vkmsdev,
					     struct vkms_config_encoder *config)
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
	struct vkms_config_plane *config_plane;
	struct drm_connector *connector;
	int ret;

	/*
	 * Initialize used planes. One primary plane is required to perform the composition.
	 *
	 * The overlays and cursor planes are not mandatory, but can be used to perform complex
	 * composition.
	 */
	struct vkms_config_crtc *config_crtc;
	struct vkms_config_encoder *config_encoder;
	unsigned long idx;

	list_for_each_entry(config_plane, &vkms_config->planes, link) {
		config_plane->plane = vkms_plane_init(vkms_device, config_plane);
		if (IS_ERR(config_plane->plane)) {
			ret = PTR_ERR(config_plane->plane);
			return ret;
		}
	}

	list_for_each_entry(config_crtc, &vkms_config->crtcs, link) {
		struct drm_plane *primary = NULL, *cursor = NULL;

		xa_for_each(&config_crtc->possible_planes, idx, config_plane) {
			if (config_plane->type == DRM_PLANE_TYPE_PRIMARY)
				primary = &config_plane->plane->base;
			else if (config_plane->type == DRM_PLANE_TYPE_CURSOR)
				cursor = &config_plane->plane->base;
		}

		config_crtc->crtc = vkms_crtc_init(vkms_device, primary, cursor, config_crtc);

		if (IS_ERR(config_crtc->crtc)) {
			ret = PTR_ERR(config_crtc->crtc);
			return ret;
		}
	}

	list_for_each_entry(config_crtc, &vkms_config->crtcs, link) {
		xa_for_each(&config_crtc->possible_planes, idx, config_plane) {
			config_plane->plane->base.possible_crtcs |= drm_crtc_mask(&config_crtc->crtc->base);
		}
	}

	connector = vkms_connector_init(vkms_device);
	if (IS_ERR(connector))
		return PTR_ERR(connector);

	list_for_each_entry(config_encoder, &vkms_config->encoders, link) {
		config_encoder->encoder = vkms_encoder_init(vkms_device, config_encoder);
		xa_for_each(&config_encoder->possible_crtcs, idx, config_crtc) {
			config_encoder->encoder->possible_crtcs |= drm_crtc_mask(&config_crtc->crtc->base);
		}
		if (IS_ERR(config_encoder->encoder))
			return PTR_ERR(config_encoder->encoder);
		ret = drm_connector_attach_encoder(connector, config_encoder->encoder);
	}

	drm_connector_helper_add(connector, &vkms_conn_helper_funcs);

	/* Attach the encoder and the connector */
	if (ret) {
		DRM_ERROR("Failed to attach connector to encoder\n");
		return ret;
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
	INIT_LIST_HEAD(&vkms_config->crtcs);
	INIT_LIST_HEAD(&vkms_config->encoders);
	vkms_config->writeback = false;

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
	vkms_config_overlay->supported_color_encoding = BIT(DRM_COLOR_YCBCR_BT601) |
							BIT(DRM_COLOR_YCBCR_BT709) |
							BIT(DRM_COLOR_YCBCR_BT2020);
	vkms_config_overlay->default_color_encoding = DRM_COLOR_YCBCR_BT601;
	vkms_config_overlay->supported_color_range = BIT(DRM_COLOR_YCBCR_LIMITED_RANGE) |
						     BIT(DRM_COLOR_YCBCR_FULL_RANGE);
	vkms_config_overlay->default_color_range = DRM_COLOR_YCBCR_FULL_RANGE;
	xa_init_flags(&vkms_config_overlay->possible_crtcs, XA_FLAGS_ALLOC);

	list_add(&vkms_config_overlay->link, &vkms_config->planes);

	return vkms_config_overlay;
}

struct vkms_config_crtc *vkms_config_create_crtc(struct vkms_config *vkms_config)
{
	if (!vkms_config)
		return NULL;

	struct vkms_config_crtc *vkms_config_crtc = kzalloc(sizeof(*vkms_config_crtc),
							    GFP_KERNEL);

	if (!vkms_config_crtc)
		return NULL;

	list_add(&vkms_config_crtc->link, &vkms_config->crtcs);
	xa_init_flags(&vkms_config_crtc->possible_planes, XA_FLAGS_ALLOC);
	xa_init_flags(&vkms_config_crtc->possible_encoders, XA_FLAGS_ALLOC);

	return vkms_config_crtc;
}

struct vkms_config_encoder *vkms_config_create_encoder(struct vkms_config *vkms_config)
{
	if (!vkms_config)
		return NULL;

	struct vkms_config_encoder *vkms_config_encoder = kzalloc(sizeof(*vkms_config_encoder),
								  GFP_KERNEL);

	if (!vkms_config_encoder)
		return NULL;

	list_add(&vkms_config_encoder->link, &vkms_config->encoders);
	xa_init_flags(&vkms_config_encoder->possible_crtcs, XA_FLAGS_ALLOC);

	return vkms_config_encoder;
}

void vkms_config_delete_plane(struct vkms_config_plane *vkms_config_plane,
			      struct vkms_config *vkms_config)
{
	struct vkms_config_crtc *crtc_config;
	struct vkms_config_plane *plane;

	if (!vkms_config_plane)
		return;
	list_del(&vkms_config_plane->link);
	xa_destroy(&vkms_config_plane->possible_crtcs);

	list_for_each_entry(crtc_config, &vkms_config->crtcs, link) {
		unsigned long idx = 0;

		xa_for_each(&crtc_config->possible_planes, idx, plane) {
			if (plane == vkms_config_plane)
				xa_erase(&crtc_config->possible_planes, idx);
		}
	}

	kfree(vkms_config_plane->name);
	kfree(vkms_config_plane);
}

void vkms_config_delete_crtc(struct vkms_config_crtc *vkms_config_crtc,
			     struct vkms_config *vkms_config)
{
	struct vkms_config_crtc *crtc_config;
	struct vkms_config_plane *plane_config;
	struct vkms_config_encoder *encoder_config;

	if (!vkms_config_crtc)
		return;
	list_del(&vkms_config_crtc->link);
	xa_destroy(&vkms_config_crtc->possible_planes);
	xa_destroy(&vkms_config_crtc->possible_encoders);

	list_for_each_entry(plane_config, &vkms_config->planes, link) {
		unsigned long idx = 0;

		xa_for_each(&plane_config->possible_crtcs, idx, crtc_config) {
			if (crtc_config == vkms_config_crtc)
				xa_erase(&plane_config->possible_crtcs, idx);
		}
	}

	list_for_each_entry(encoder_config, &vkms_config->encoders, link) {
		unsigned long idx = 0;

		xa_for_each(&encoder_config->possible_crtcs, idx, crtc_config) {
			if (crtc_config == vkms_config_crtc)
				xa_erase(&encoder_config->possible_crtcs, idx);
		}
	}

	kfree(vkms_config_crtc->name);
	kfree(vkms_config_crtc);
}

void vkms_config_delete_encoder(struct vkms_config_encoder *vkms_config_encoder,
				struct vkms_config *vkms_config)
{
	if (!vkms_config_encoder)
		return;
	list_del(&vkms_config_encoder->link);
	xa_destroy(&vkms_config_encoder->possible_crtcs);

	struct vkms_config_crtc *crtc_config;
	struct vkms_config_encoder *encoder;

	list_for_each_entry(crtc_config, &vkms_config->crtcs, link) {
		unsigned long idx = 0;

		xa_for_each(&crtc_config->possible_encoders, idx, encoder) {
			if (encoder == vkms_config_encoder)
				xa_erase(&crtc_config->possible_encoders, idx);
		}
	}

	kfree(vkms_config_encoder->name);
	kfree(vkms_config_encoder);
}

void vkms_config_free(struct vkms_config *vkms_config)
{
	struct vkms_config_plane *vkms_config_plane, *tmp_plane;
	struct vkms_config_encoder *vkms_config_encoder, *tmp_encoder;
	struct vkms_config_crtc *vkms_config_crtc, *tmp_crtc;

	list_for_each_entry_safe(vkms_config_plane, tmp_plane, &vkms_config->planes, link) {
		vkms_config_delete_plane(vkms_config_plane,
					 vkms_config);
	}
	list_for_each_entry_safe(vkms_config_encoder, tmp_encoder, &vkms_config->encoders, link) {
		vkms_config_delete_encoder(vkms_config_encoder,
					   vkms_config);
	}
	list_for_each_entry_safe(vkms_config_crtc, tmp_crtc, &vkms_config->crtcs, link) {
		vkms_config_delete_crtc(vkms_config_crtc,
					vkms_config);
	}
	kfree(vkms_config);
}

int __must_check vkms_config_plane_attach_crtc(struct vkms_config_plane *vkms_config_plane,
					       struct vkms_config_crtc *vkms_config_crtc)
{
	u32 crtc_idx, encoder_idx;
	int ret;

	ret = xa_alloc(&vkms_config_plane->possible_crtcs, &crtc_idx, vkms_config_crtc,
		       xa_limit_32b, GFP_KERNEL);
	if (ret)
		return ret;

	ret = xa_alloc(&vkms_config_crtc->possible_planes, &encoder_idx, vkms_config_plane,
		       xa_limit_32b, GFP_KERNEL);
	if (ret) {
		xa_erase(&vkms_config_plane->possible_crtcs, crtc_idx);
		return ret;
	}

	return ret;
}

int __must_check vkms_config_encoder_attach_crtc(struct vkms_config_encoder *vkms_config_encoder,
						 struct vkms_config_crtc *vkms_config_crtc)
{
	u32 crtc_idx, encoder_idx;
	int ret;

	ret = xa_alloc(&vkms_config_encoder->possible_crtcs, &crtc_idx, vkms_config_crtc,
		       xa_limit_32b, GFP_KERNEL);
	if (ret)
		return ret;

	ret = xa_alloc(&vkms_config_crtc->possible_encoders, &encoder_idx, vkms_config_encoder,
		       xa_limit_32b, GFP_KERNEL);
	if (ret) {
		xa_erase(&vkms_config_encoder->possible_crtcs, crtc_idx);
		return ret;
	}

	return ret;
}

bool vkms_config_is_valid(struct vkms_config *vkms_config)
{
	struct vkms_config_plane *config_plane;
	struct vkms_config_crtc *config_crtc;
	struct vkms_config_encoder *config_encoder;

	list_for_each_entry(config_plane, &vkms_config->planes, link) {
		// Default rotation not in supported rotations
		if ((config_plane->default_rotation & config_plane->supported_rotations) !=
		    config_plane->default_rotation)
			return false;

		// Default color encoding not in supported color encodings
		if ((config_plane->default_color_encoding &
		     config_plane->supported_color_encoding) !=
		    config_plane->default_color_encoding)
			return false;

		// Default color range not in supported color range
		if ((config_plane->default_color_range & config_plane->supported_color_range) !=
		    config_plane->default_color_range)
			return false;

		// No CRTC linked to this plane
		if (xa_empty(&config_plane->possible_crtcs))
			return false;
	}

	list_for_each_entry(config_encoder, &vkms_config->encoders, link) {
		// No CRTC linked to this encoder
		if (xa_empty(&config_encoder->possible_crtcs))
			return false;
	}

	list_for_each_entry(config_crtc, &vkms_config->crtcs, link) {
		bool has_primary = false;
		bool has_cursor = false;
		unsigned long idx = 0;

		// No encoder attached to this CRTC
		if (xa_empty(&config_crtc->possible_encoders))
			return false;

		xa_for_each(&config_crtc->possible_planes, idx, config_plane) {
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

		// No primary plane for this CRTC
		if (!has_primary)
			return false;
	}

	return true;
}
