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

	return config;
}
EXPORT_SYMBOL_IF_KUNIT(vkms_config_create);

struct vkms_config *vkms_config_alloc_default(bool enable_writeback, bool enable_overlay,
					      bool enable_cursor)
{
	struct vkms_config_plane *plane;
	struct vkms_config *vkms_config = vkms_config_create();

	if (IS_ERR(vkms_config))
		return vkms_config;

	vkms_config->writeback = enable_writeback;

	plane = vkms_config_create_plane(vkms_config);
	if (!plane)
		goto err_alloc;

	plane->type = DRM_PLANE_TYPE_PRIMARY;

	if (enable_overlay) {
		for (int i = 0; i < NUM_OVERLAY_PLANES; i++) {
			plane = vkms_config_create_plane(vkms_config);
			if (!plane)
				goto err_alloc;
			plane->type = DRM_PLANE_TYPE_OVERLAY;
		}
	}
	if (enable_cursor) {
		plane = vkms_config_create_plane(vkms_config);
		if (!plane)
			goto err_alloc;
		plane->type = DRM_PLANE_TYPE_CURSOR;
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

	list_add(&vkms_config_overlay->link, &vkms_config->planes);

	return vkms_config_overlay;
}
EXPORT_SYMBOL_IF_KUNIT(vkms_config_create_plane);

void vkms_config_delete_plane(struct vkms_config_plane *vkms_config_overlay)
{
	if (!vkms_config_overlay)
		return;
	list_del(&vkms_config_overlay->link);
	kfree(vkms_config_overlay);
}

void vkms_config_destroy(struct vkms_config *config)
{
	struct vkms_config_plane *vkms_config_plane, *tmp_plane;

	list_for_each_entry_safe(vkms_config_plane, tmp_plane, &config->planes, link) {
		vkms_config_delete_plane(vkms_config_plane);
	}

	kfree(config);
}
EXPORT_SYMBOL_IF_KUNIT(vkms_config_destroy);

bool vkms_config_is_valid(struct vkms_config *config)
{
	struct vkms_config_plane *config_plane;

	bool has_cursor = false;
	bool has_primary = false;

	list_for_each_entry(config_plane, &config->planes, link) {
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
EXPORT_SYMBOL_IF_KUNIT(vkms_config_is_valid);

static int vkms_config_show(struct seq_file *m, void *data)
{
	struct drm_debugfs_entry *entry = m->private;
	struct drm_device *dev = entry->dev;
	struct vkms_device *vkmsdev = drm_device_to_vkms_device(dev);
	struct vkms_config_plane *config_plane;

	seq_printf(m, "writeback=%d\n", vkmsdev->config->writeback);
	list_for_each_entry(config_plane, &vkmsdev->config->planes, link) {
		seq_puts(m, "plane:\n");
		seq_printf(m, "\ttype: %d\n", config_plane->type);
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
