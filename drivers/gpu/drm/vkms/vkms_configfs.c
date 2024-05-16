// SPDX-License-Identifier: GPL-2.0+

#include <linux/configfs.h>
#include <linux/mutex.h>
#include <drm/drm_print.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include "vkms_configfs.h"
#include "vkms_device_drv.h"

static ssize_t device_enable_show(struct config_item *item, char *page)
{
	return sprintf(page, "%d\n",
		       config_item_to_vkms_configfs_device(item)->enabled);
}

static ssize_t device_enable_store(struct config_item *item,
				   const char *page, size_t count)
{
	struct vkms_configfs_device *vkms_configfs_device =
		config_item_to_vkms_configfs_device(item);
	struct vkms_platform_data platform_data = {
		.config = vkms_configfs_device->vkms_config
	};
	bool value;
	int ret;

	ret = kstrtobool(page, &value);
	if (ret)
		return -EINVAL;

	mutex_lock(&vkms_configfs_device->lock);

	vkms_configfs_device->enabled = value;

	if (value)
		vkms_configfs_device->platform_device = vkms_create_device(&platform_data);
	else
		vkms_delete_device(vkms_configfs_device->platform_device);

	mutex_unlock(&vkms_configfs_device->lock);

	return (ssize_t)count;
}

CONFIGFS_ATTR(device_, enable);

static struct configfs_attribute *device_attrs[] = {
	&device_attr_enable,
	NULL,
};

static void device_release(struct config_item *item)
{
	struct vkms_configfs_device *vkms_configfs_device =
					    config_item_to_vkms_configfs_device(item);

	mutex_destroy(&vkms_configfs_device->lock);
	vkms_config_free(vkms_configfs_device->vkms_config);

	kfree(vkms_configfs_device);
}

static struct configfs_item_operations device_item_operations = {
	.release	= &device_release,
};

static const struct config_item_type device_item_type = {
	.ct_attrs	= device_attrs,
	.ct_item_ops	= &device_item_operations,
	.ct_owner	= THIS_MODULE,
};

/* Top directory management. Each new directory here is a new device */
static struct config_group *root_make_group(struct config_group *group,
					    const char *name)
{
	struct vkms_config_plane *plane;
	struct vkms_config_crtc *crtc;
	struct vkms_config_encoder *encoder;
	struct vkms_configfs_device *configfs = kzalloc(sizeof(*configfs), GFP_KERNEL);

	if (!configfs)
		return ERR_PTR(-ENOMEM);

	mutex_init(&configfs->lock);

	configfs->vkms_config = vkms_config_alloc();

	if (!configfs->vkms_config) {
		kfree(configfs);
		return ERR_PTR(-ENOMEM);
	}

	plane = vkms_config_create_plane(configfs->vkms_config);
	crtc = vkms_config_create_crtc(configfs->vkms_config);
	encoder = vkms_config_create_encoder(configfs->vkms_config);

	if (!plane || !crtc || !encoder ||
	    vkms_config_plane_attach_crtc(plane, crtc) ||
	    vkms_config_encoder_attach_crtc(encoder, crtc)) {
		vkms_config_free(configfs->vkms_config);
		kfree(configfs);
		return ERR_PTR(-ENOMEM);
	}

	plane->type = DRM_PLANE_TYPE_PRIMARY;

	config_group_init_type_name(&configfs->group, name,
				    &device_item_type);

	return &configfs->group;
}

static struct configfs_group_operations root_group_operations = {
	.make_group	= &root_make_group,
};

static struct config_item_type root_item_type = {
	.ct_group_ops	= &root_group_operations,
	.ct_owner	= THIS_MODULE,
};

static struct configfs_subsystem vkms_subsys = {
	.su_group = {
		.cg_item = {
			.ci_name = "vkms",
			.ci_type = &root_item_type,
		},
	},
	.su_mutex = __MUTEX_INITIALIZER(vkms_subsys.su_mutex),
};

int vkms_init_configfs(void)
{
	config_group_init(&vkms_subsys.su_group);

	return configfs_register_subsystem(&vkms_subsys);
}

void vkms_unregister_configfs(void)
{
	configfs_unregister_subsystem(&vkms_subsys);
}
