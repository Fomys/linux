// SPDX-License-Identifier: GPL-2.0+

#include <kunit/visibility.h>
#include <drm/drm_debugfs.h>

#include "vkms_config.h"
#include "vkms_drv.h"

struct vkms_config *vkms_config_create(void)
{
	struct vkms_config *config;

	config = kzalloc(sizeof(*config), GFP_KERNEL);
	if (!config)
		return ERR_PTR(-ENOMEM);

	INIT_LIST_HEAD(&config->planes);
	INIT_LIST_HEAD(&config->crtcs);
	INIT_LIST_HEAD(&config->encoders);

	return config;
}
EXPORT_SYMBOL_IF_KUNIT(vkms_config_create);

struct vkms_config *vkms_config_alloc_default(bool enable_writeback, bool enable_overlay,
					      bool enable_cursor)
{
	struct vkms_config_plane *plane;
	struct vkms_config_encoder *encoder;
	struct vkms_config_crtc *crtc;
	struct vkms_config *vkms_config = vkms_config_create();

	if (IS_ERR(vkms_config))
		return vkms_config;

	crtc = vkms_config_create_crtc(vkms_config);
	if (!crtc)
		goto err_alloc;
	crtc->writeback = enable_writeback;

	encoder = vkms_config_create_encoder(vkms_config);
	if (!encoder)
		goto err_alloc;
	encoder->name = kzalloc(sizeof("Main Encoder"), GFP_KERNEL);
	if (!encoder->name)
		goto err_alloc;
	sprintf(encoder->name, "Main Encoder");

	if (vkms_config_encoder_attach_crtc(encoder, crtc))
		goto err_alloc;

	plane = vkms_config_create_plane(vkms_config);
	if (!plane)
		goto err_alloc;

	plane->type = DRM_PLANE_TYPE_PRIMARY;
	plane->name = kzalloc(sizeof("primary"), GFP_KERNEL);
	if (!plane->name)
		goto err_alloc;
	sprintf(plane->name, "primary");

	if (vkms_config_plane_attach_crtc(plane, crtc))
		goto err_alloc;

	if (enable_overlay) {
		for (int i = 0; i < NUM_OVERLAY_PLANES; i++) {
			plane = vkms_config_create_plane(vkms_config);
			if (!plane)
				goto err_alloc;
			plane->type = DRM_PLANE_TYPE_OVERLAY;
			plane->name = kzalloc(10, GFP_KERNEL);
			if (!plane->name)
				goto err_alloc;
			snprintf(plane->name, 10, "plane-%d", i);

			if (vkms_config_plane_attach_crtc(plane, crtc))
				goto err_alloc;
		}
	}
	if (enable_cursor) {
		plane = vkms_config_create_plane(vkms_config);
		if (!plane)
			goto err_alloc;
		plane->type = DRM_PLANE_TYPE_CURSOR;
		plane->name = kzalloc(sizeof("cursor"), GFP_KERNEL);
		if (!plane->name)
			goto err_alloc;
		sprintf(plane->name, "cursor");

		if (vkms_config_plane_attach_crtc(plane, crtc))
			goto err_alloc;
	}
	return vkms_config;

err_alloc:
	vkms_config_destroy(vkms_config);
	return ERR_PTR(-ENOMEM);
}

struct vkms_config_plane *vkms_config_create_plane(struct vkms_config *vkms_config)
{
	if (!vkms_config)
		return NULL;

	struct vkms_config_plane *vkms_config_overlay = kzalloc(sizeof(*vkms_config_overlay),
								GFP_KERNEL);

	if (!vkms_config_overlay)
		return NULL;

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
EXPORT_SYMBOL_IF_KUNIT(vkms_config_create_plane);

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
EXPORT_SYMBOL_IF_KUNIT(vkms_config_create_crtc);

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
EXPORT_SYMBOL_IF_KUNIT(vkms_config_create_encoder);

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
EXPORT_SYMBOL_IF_KUNIT(vkms_config_delete_plane);

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

void vkms_config_destroy(struct vkms_config *config)
{
	struct vkms_config_plane *vkms_config_plane, *tmp_plane;
	struct vkms_config_encoder *vkms_config_encoder, *tmp_encoder;
	struct vkms_config_crtc *vkms_config_crtc, *tmp_crtc;

	list_for_each_entry_safe(vkms_config_plane, tmp_plane, &config->planes, link) {
		vkms_config_delete_plane(vkms_config_plane, config);
	}
	list_for_each_entry_safe(vkms_config_encoder, tmp_encoder, &config->encoders, link) {
		vkms_config_delete_encoder(vkms_config_encoder, config);
	}
	list_for_each_entry_safe(vkms_config_crtc, tmp_crtc, &config->crtcs, link) {
		vkms_config_delete_crtc(vkms_config_crtc, config);
	}

	kfree(config);
}
EXPORT_SYMBOL_IF_KUNIT(vkms_config_destroy);

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
EXPORT_SYMBOL_IF_KUNIT(vkms_config_plane_attach_crtc);

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
EXPORT_SYMBOL_IF_KUNIT(vkms_config_encoder_attach_crtc);

bool vkms_config_is_valid(struct vkms_config *config)
{
	struct vkms_config_plane *config_plane;

	struct vkms_config_crtc *config_crtc;
	struct vkms_config_encoder *config_encoder;

	list_for_each_entry(config_plane, &config->planes, link) {
		// Default rotation not in supported rotations
		if ((config_plane->default_rotation & config_plane->supported_rotations) !=
		    config_plane->default_rotation)
			return false;

		// Default color range not in supported color range
		if ((BIT(config_plane->default_color_encoding) &
		     config_plane->supported_color_encoding) !=
		    BIT(config_plane->default_color_encoding))
			return false;

		// Default color range not in supported color range
		if ((BIT(config_plane->default_color_range) &
		     config_plane->supported_color_range) !=
		    BIT(config_plane->default_color_range))
			return false;

		// No CRTC linked to this plane
		if (xa_empty(&config_plane->possible_crtcs))
			return false;
	}

	list_for_each_entry(config_encoder, &config->encoders, link) {
		// No CRTC linked to this encoder
		if (xa_empty(&config_encoder->possible_crtcs))
			return false;
	}

	list_for_each_entry(config_crtc, &config->crtcs, link) {
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
EXPORT_SYMBOL_IF_KUNIT(vkms_config_is_valid);

static int vkms_config_show(struct seq_file *m, void *data)
{
	struct drm_debugfs_entry *entry = m->private;
	struct drm_device *dev = entry->dev;
	struct vkms_device *vkmsdev = drm_device_to_vkms_device(dev);
	struct vkms_config_plane *config_plane;
	struct vkms_config_crtc *config_crtc;
	struct vkms_config_encoder *config_encoder;

	list_for_each_entry(config_plane, &vkmsdev->config->planes, link) {
		seq_puts(m, "plane:\n");
		seq_printf(m, "\tname: %s\n", config_plane->name);
		seq_printf(m, "\ttype: %d\n", config_plane->type);
		seq_printf(m, "\tsupported rotations: 0x%x\n", config_plane->supported_rotations);
		seq_printf(m, "\tdefault rotation: 0x%x\n", config_plane->default_rotation);
		seq_printf(m, "\tsupported color encoding: 0x%x\n",
			   config_plane->supported_color_encoding);
		seq_printf(m, "\tdefault color encoding: %d\n",
			   config_plane->default_color_encoding);
		seq_printf(m, "\tsupported color range: 0x%x\n",
			   config_plane->supported_color_range);
		seq_printf(m, "\tdefault color range: %d\n",
			   config_plane->default_color_range);
	}

	list_for_each_entry(config_encoder, &vkmsdev->config->encoders, link) {
		seq_puts(m, "encoder:\n");
		seq_printf(m, "\tname: %s\n", config_encoder->name);
	}

	list_for_each_entry(config_crtc, &vkmsdev->config->crtcs, link) {
		seq_puts(m, "crtc:\n");
		seq_printf(m, "\twriteback: %d\n", config_crtc->writeback);
	}

	return 0;
}

static const struct drm_debugfs_info vkms_config_debugfs_list[] = {
	{ "vkms_config", vkms_config_show, 0 },
};

void vkms_config_register_debugfs(struct vkms_device *vkms_device)
{
	drm_debugfs_add_files(&vkms_device->drm, vkms_config_debugfs_list,
			      ARRAY_SIZE(vkms_config_debugfs_list));
}
