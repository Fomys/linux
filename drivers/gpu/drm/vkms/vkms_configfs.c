// SPDX-License-Identifier: GPL-2.0+

#include <linux/configfs.h>
#include <linux/mutex.h>
#include <drm/drm_print.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include "vkms_configfs.h"
#include "vkms_device_drv.h"

static ssize_t plane_type_show(struct config_item *item, char *page)
{
	struct vkms_config_plane *plane;
	enum drm_plane_type plane_type;
	struct vkms_configfs_device *vkms_configfs = plane_item_to_vkms_configfs_device(item);

	mutex_lock(&vkms_configfs->lock);
	plane = plane_item_to_vkms_configfs_plane(item)->vkms_config_plane;
	plane_type = plane->type;
	mutex_unlock(&vkms_configfs->lock);

	return sprintf(page, "%u", plane_type);
}

static ssize_t plane_type_store(struct config_item *item,
				const char *page, size_t count)
{
	struct vkms_configfs_device *vkms_configfs = plane_item_to_vkms_configfs_device(item);
	enum drm_plane_type val = DRM_PLANE_TYPE_OVERLAY;
	struct vkms_config_plane *plane;
	int ret;

	ret = kstrtouint(page, 10, &val);
	if (ret)
		return ret;

	if (val != DRM_PLANE_TYPE_PRIMARY && val != DRM_PLANE_TYPE_CURSOR &&
	    val != DRM_PLANE_TYPE_OVERLAY)
		return -EINVAL;

	mutex_lock(&vkms_configfs->lock);
	if (vkms_configfs->enabled) {
		mutex_unlock(&vkms_configfs->lock);
		return -EINVAL;
	}

	plane = plane_item_to_vkms_configfs_plane(item)->vkms_config_plane;
	plane->type = val;

	mutex_unlock(&vkms_configfs->lock);

	return count;
}

static ssize_t plane_supported_rotations_show(struct config_item *item, char *page)
{
	struct vkms_config_plane *plane;
	unsigned int plane_type;
	struct vkms_configfs_device *vkms_configfs = plane_item_to_vkms_configfs_device(item);

	mutex_lock(&vkms_configfs->lock);
	plane = plane_item_to_vkms_configfs_plane(item)->vkms_config_plane;
	plane_type = plane->supported_rotations;
	mutex_unlock(&vkms_configfs->lock);

	return sprintf(page, "%u", plane_type);
}

static ssize_t plane_supported_rotations_store(struct config_item *item,
					       const char *page, size_t count)
{
	struct vkms_config_plane *plane;
	struct vkms_configfs_device *vkms_configfs = plane_item_to_vkms_configfs_device(item);
	int ret, val = 0;

	ret = kstrtouint(page, 0, &val);
	if (ret)
		return ret;

	/* Should be a supported value */
	if (val & ~(DRM_MODE_ROTATE_MASK | DRM_MODE_REFLECT_MASK))
		return -EINVAL;
	/* Should at least provide one rotation */
	if (!(val & DRM_MODE_ROTATE_MASK))
		return -EINVAL;

	mutex_lock(&vkms_configfs->lock);

	plane = plane_item_to_vkms_configfs_plane(item)->vkms_config_plane;

	/* Ensures that the default rotation is included in supported rotation */
	if (vkms_configfs->enabled || (val & plane->default_rotation) != plane->default_rotation) {
		mutex_unlock(&vkms_configfs->lock);
		return -EINVAL;
	}

	plane->supported_rotations = val;
	mutex_unlock(&vkms_configfs->lock);

	return count;
}

static ssize_t plane_default_rotation_show(struct config_item *item, char *page)
{
	unsigned int plane_type;
	struct vkms_config_plane *plane;
	struct vkms_configfs_device *vkms_configfs = plane_item_to_vkms_configfs_device(item);

	mutex_lock(&vkms_configfs->lock);
	plane = plane_item_to_vkms_configfs_plane(item)->vkms_config_plane;
	plane_type = plane->default_rotation;
	mutex_unlock(&vkms_configfs->lock);

	return sprintf(page, "%u", plane_type);
}

static ssize_t plane_default_rotation_store(struct config_item *item,
					    const char *page, size_t count)
{
	struct vkms_config_plane *plane;
	struct vkms_configfs_device *vkms_configfs = plane_item_to_vkms_configfs_device(item);
	int ret, val = 0;

	ret = kstrtouint(page, 10, &val);
	if (ret)
		return ret;

	/* Should be a supported value */
	if (val & ~(DRM_MODE_ROTATE_MASK | DRM_MODE_REFLECT_MASK))
		return -EINVAL;
	/* Should at least provide one rotation */
	if ((val & DRM_MODE_ROTATE_MASK) == 0)
		return -EINVAL;
	/* Should contains only one rotation */
	if (!is_power_of_2(val & DRM_MODE_ROTATE_MASK))
		return -EINVAL;
	mutex_lock(&vkms_configfs->lock);

	plane = plane_item_to_vkms_configfs_plane(item)->vkms_config_plane;

	/* Ensures that the default rotation is included in supported rotation */
	if (vkms_configfs->enabled ||
	    (val & plane->supported_rotations) != val) {
		mutex_unlock(&vkms_configfs->lock);
		return -EINVAL;
	}
	plane->default_rotation = val;
	mutex_unlock(&vkms_configfs->lock);

	return count;
}

CONFIGFS_ATTR(plane_, type);
CONFIGFS_ATTR(plane_, supported_rotations);
CONFIGFS_ATTR(plane_, default_rotation);

static struct configfs_attribute *plane_attrs[] = {
	&plane_attr_type,
	&plane_attr_supported_rotations,
	&plane_attr_default_rotation,
	NULL,
};

static void plane_release(struct config_item *item)
{
	struct vkms_configfs_plane *vkms_configfs_plane = plane_item_to_vkms_configfs_plane(item);

	mutex_lock(&vkms_configfs_plane->vkms_configfs_device->lock);
	vkms_config_delete_plane(vkms_configfs_plane->vkms_config_plane,
				 vkms_configfs_plane->vkms_configfs_device->vkms_config);
	mutex_unlock(&vkms_configfs_plane->vkms_configfs_device->lock);

	kfree(vkms_configfs_plane);
}

static struct configfs_item_operations plane_item_operations = {
	.release	= plane_release,
};

static const struct config_item_type subgroup_plane = {
	.ct_attrs	= plane_attrs,
	.ct_item_ops	= &plane_item_operations,
	.ct_owner	= THIS_MODULE,
};

static struct config_group *planes_make_group(struct config_group *config_group,
					      const char *name)
{
	struct vkms_configfs_device *vkms_configfs;
	struct vkms_configfs_plane *vkms_configfs_plane;

	vkms_configfs = planes_item_to_vkms_configfs_device(&config_group->cg_item);
	vkms_configfs_plane = kzalloc(sizeof(*vkms_configfs_plane), GFP_KERNEL);

	if (!vkms_configfs_plane)
		return ERR_PTR(-ENOMEM);

	mutex_lock(&vkms_configfs->lock);

	if (vkms_configfs->enabled) {
		kfree(vkms_configfs_plane);
		mutex_unlock(&vkms_configfs->lock);
		return ERR_PTR(-EINVAL);
	}

	vkms_configfs_plane->vkms_config_plane = vkms_config_create_plane(vkms_configfs->vkms_config);

	if (list_count_nodes(&vkms_configfs->vkms_config->planes) == 1)
		vkms_configfs_plane->vkms_config_plane->type = DRM_PLANE_TYPE_PRIMARY;

	if (!vkms_configfs_plane->vkms_config_plane ||
	    vkms_config_plane_attach_crtc(vkms_configfs_plane->vkms_config_plane,
					  vkms_configfs->vkms_config_crtc)) {
		kfree(vkms_configfs_plane);
		mutex_unlock(&vkms_configfs->lock);
		return ERR_PTR(-ENOMEM);
	}

	vkms_configfs_plane->vkms_config_plane->name = kzalloc(strlen(name) + 1, GFP_KERNEL);
	if (!vkms_configfs_plane->vkms_config_plane->name) {
		kfree(vkms_configfs_plane->vkms_config_plane);
		kfree(vkms_configfs_plane);
		mutex_unlock(&vkms_configfs->lock);
		return ERR_PTR(-ENOMEM);
	}
	strscpy(vkms_configfs_plane->vkms_config_plane->name, name, strlen(name) + 1);

	config_group_init_type_name(&vkms_configfs_plane->group, name, &subgroup_plane);

	vkms_configfs_plane->vkms_configfs_device = vkms_configfs;
	mutex_unlock(&vkms_configfs->lock);

	return &vkms_configfs_plane->group;
}

static struct configfs_group_operations planes_group_operations = {
	.make_group	= &planes_make_group,
};

static const struct config_item_type planes_item_type = {
	.ct_group_ops	= &planes_group_operations,
	.ct_owner	= THIS_MODULE,
};

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

	configfs->vkms_config_crtc = vkms_config_create_crtc(configfs->vkms_config);
	configfs->vkms_config_encoder = vkms_config_create_encoder(configfs->vkms_config);
	if (!configfs->vkms_config_crtc || !configfs->vkms_config_encoder ||
	    vkms_config_encoder_attach_crtc(configfs->vkms_config_encoder,
					    configfs->vkms_config_crtc)) {
		vkms_config_free(configfs->vkms_config);
		kfree(configfs);
		return ERR_PTR(-ENOMEM);
	}

	config_group_init_type_name(&configfs->group, name,
				    &device_item_type);

	config_group_init_type_name(&configfs->plane_group, "planes", &planes_item_type);
	configfs_add_default_group(&configfs->plane_group, &configfs->group);

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
