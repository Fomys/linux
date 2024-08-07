// SPDX-License-Identifier: GPL-2.0+

#include <linux/configfs.h>
#include <linux/mutex.h>
#include <drm/drm_print.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/generic-radix-tree.h>

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

static ssize_t plane_color_range_show(struct config_item *item, char *page)
{
	struct vkms_config_plane *plane;
	unsigned int plane_type;
	struct vkms_configfs_device *vkms_configfs = plane_item_to_vkms_configfs_device(item);

	mutex_lock(&vkms_configfs->lock);
	plane = plane_item_to_vkms_configfs_plane(item)->vkms_config_plane;
	plane_type = plane->supported_color_range;
	mutex_unlock(&vkms_configfs->lock);

	return sprintf(page, "%u", plane_type);
}

static ssize_t plane_color_range_store(struct config_item *item,
				       const char *page, size_t count)
{
	struct vkms_config_plane *plane;
	struct vkms_configfs_device *vkms_configfs = plane_item_to_vkms_configfs_device(item);
	int ret, val = 0;

	ret = kstrtouint(page, 10, &val);
	if (ret)
		return ret;

	/* Should be a supported value */
	if (val & ~(BIT(DRM_COLOR_YCBCR_LIMITED_RANGE) |
		    BIT(DRM_COLOR_YCBCR_FULL_RANGE)))
		return -EINVAL;
	/* Should at least provide one color range */
	if ((val & (BIT(DRM_COLOR_YCBCR_LIMITED_RANGE) |
		    BIT(DRM_COLOR_YCBCR_FULL_RANGE))) == 0)
		return -EINVAL;

	mutex_lock(&vkms_configfs->lock);

	plane = plane_item_to_vkms_configfs_plane(item)->vkms_config_plane;

	/* Ensures that the default rotation is included in supported rotation */
	if (vkms_configfs->enabled || (val & plane->default_color_range) !=
				      plane->default_color_range) {
		mutex_unlock(&vkms_configfs->lock);
		return -EINVAL;
	}
	plane->supported_color_range = val;
	mutex_unlock(&vkms_configfs->lock);

	return count;
}

static ssize_t plane_default_color_range_show(struct config_item *item, char *page)
{
	struct vkms_config_plane *plane;
	unsigned int plane_type;
	struct vkms_configfs_device *vkms_configfs = plane_item_to_vkms_configfs_device(item);

	mutex_lock(&vkms_configfs->lock);
	plane = plane_item_to_vkms_configfs_plane(item)->vkms_config_plane;
	plane_type = plane->default_color_range;
	mutex_unlock(&vkms_configfs->lock);

	return sprintf(page, "%u", plane_type);
}

static ssize_t plane_default_color_range_store(struct config_item *item,
					       const char *page, size_t count)
{
	struct vkms_config_plane *plane;
	struct vkms_configfs_device *vkms_configfs = plane_item_to_vkms_configfs_device(item);
	int ret, val = 0;

	ret = kstrtouint(page, 10, &val);
	if (ret)
		return ret;

	/* Should be a supported value */
	if (val & ~(BIT(DRM_COLOR_YCBCR_LIMITED_RANGE) |
		    BIT(DRM_COLOR_YCBCR_FULL_RANGE)))
		return -EINVAL;
	/* Should at least provide one color range */
	if ((val & (BIT(DRM_COLOR_YCBCR_LIMITED_RANGE) |
		    BIT(DRM_COLOR_YCBCR_FULL_RANGE))) == 0)
		return -EINVAL;

	mutex_lock(&vkms_configfs->lock);

	plane = plane_item_to_vkms_configfs_plane(item)->vkms_config_plane;

	/* Ensures that the default rotation is included in supported rotation */
	if (vkms_configfs->enabled || (val & plane->supported_color_range) != val) {
		mutex_unlock(&vkms_configfs->lock);
		return -EINVAL;
	}
	plane->default_color_range = val;
	mutex_unlock(&vkms_configfs->lock);

	return count;
}

static ssize_t plane_supported_color_encoding_show(struct config_item *item, char *page)
{
	struct vkms_config_plane *plane;
	unsigned int supported_color_encoding;
	struct vkms_configfs_device *vkms_configfs = plane_item_to_vkms_configfs_device(item);

	mutex_lock(&vkms_configfs->lock);
	plane = plane_item_to_vkms_configfs_plane(item)->vkms_config_plane;
	supported_color_encoding = plane->supported_color_encoding;
	mutex_unlock(&vkms_configfs->lock);

	return sprintf(page, "%u", supported_color_encoding);
}

static ssize_t plane_supported_color_encoding_store(struct config_item *item,
						    const char *page, size_t count)
{
	struct vkms_configfs_device *vkms_configfs = plane_item_to_vkms_configfs_device(item);
	struct vkms_config_plane *plane;
	int ret, val = 0;

	ret = kstrtouint(page, 10, &val);
	if (ret)
		return ret;

	/* Should be a supported value */
	if (val & ~(BIT(DRM_COLOR_YCBCR_BT601) |
		    BIT(DRM_COLOR_YCBCR_BT709) |
		    BIT(DRM_COLOR_YCBCR_BT2020)))
		return -EINVAL;
	/* Should at least provide one color range */
	if ((val & (BIT(DRM_COLOR_YCBCR_BT601) |
		    BIT(DRM_COLOR_YCBCR_BT709) |
		    BIT(DRM_COLOR_YCBCR_BT2020))) == 0)
		return -EINVAL;

	mutex_lock(&vkms_configfs->lock);
	plane = plane_item_to_vkms_configfs_plane(item)->vkms_config_plane;

	/* Ensures that the default rotation is included in supported rotation */
	if (vkms_configfs->enabled || (val & plane->default_color_encoding) !=
				      plane->default_color_encoding) {
		mutex_unlock(&vkms_configfs->lock);
		return -EINVAL;
	}
	plane->supported_color_encoding = val;
	mutex_unlock(&vkms_configfs->lock);

	return count;
}

/* Plane default_color_encoding : vkms/<device>/planes/<plane>/default_color_encoding */

static ssize_t plane_default_color_encoding_show(struct config_item *item, char *page)
{
	struct vkms_config_plane *plane;
	unsigned int default_color_encoding;
	struct vkms_configfs_device *vkms_configfs = plane_item_to_vkms_configfs_device(item);

	mutex_lock(&vkms_configfs->lock);
	plane = plane_item_to_vkms_configfs_plane(item)->vkms_config_plane;
	default_color_encoding = plane->default_color_encoding;
	mutex_unlock(&vkms_configfs->lock);

	return sprintf(page, "%u", default_color_encoding);
}

static ssize_t plane_default_color_encoding_store(struct config_item *item,
						  const char *page, size_t count)
{
	struct vkms_config_plane *plane;
	struct vkms_configfs_device *vkms_configfs = plane_item_to_vkms_configfs_device(item);
	int ret, val = 0;

	ret = kstrtouint(page, 10, &val);
	if (ret)
		return ret;

	/* Should be a supported value */
	if (val & ~(BIT(DRM_COLOR_YCBCR_BT601) |
		    BIT(DRM_COLOR_YCBCR_BT709) |
		    BIT(DRM_COLOR_YCBCR_BT2020)))
		return -EINVAL;
	/* Should at least provide one color range */
	if ((val & (BIT(DRM_COLOR_YCBCR_BT601) |
		    BIT(DRM_COLOR_YCBCR_BT709) |
		    BIT(DRM_COLOR_YCBCR_BT2020))) == 0)
		return -EINVAL;
	mutex_lock(&vkms_configfs->lock);

	plane = plane_item_to_vkms_configfs_plane(item)->vkms_config_plane;

	/* Ensures that the default rotation is included in supported rotation */
	if (vkms_configfs->enabled || (val & plane->supported_color_encoding) != val) {
		mutex_unlock(&vkms_configfs->lock);
		return -EINVAL;
	}
	plane->default_color_encoding = val;
	mutex_unlock(&vkms_configfs->lock);

	return count;
}

CONFIGFS_ATTR(plane_, type);
CONFIGFS_ATTR(plane_, supported_rotations);
CONFIGFS_ATTR(plane_, default_rotation);
CONFIGFS_ATTR(plane_, color_range);
CONFIGFS_ATTR(plane_, default_color_range);
CONFIGFS_ATTR(plane_, supported_color_encoding);
CONFIGFS_ATTR(plane_, default_color_encoding);

static struct configfs_attribute *plane_attrs[] = {
	&plane_attr_type,
	&plane_attr_supported_rotations,
	&plane_attr_default_rotation,
	&plane_attr_color_range,
	&plane_attr_default_color_range,
	&plane_attr_supported_color_encoding,
	&plane_attr_default_color_encoding,
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
static const struct config_item_type crtc_item_type;
static const struct config_item_type planes_item_type;

static int possible_crtcs_allow_link(struct config_item *src,
				     struct config_item *target)
{
	struct vkms_configfs_device *vkms_configfs = plane_possible_crtc_src_item_to_vkms_configfs_device(src);
	struct vkms_config_crtc *crtc;

	mutex_lock(&vkms_configfs->lock);

	if (target->ci_type != &crtc_item_type) {
		mutex_unlock(&vkms_configfs->lock);
		return -EINVAL;
	}

	crtc = crtc_item_to_vkms_configfs_crtc(target)->vkms_config_crtc;
	struct vkms_config_plane *plane = plane_possible_crtc_src_item_to_vkms_configfs_plane(src)->vkms_config_plane;

	struct vkms_config_crtc *crtc_entry;
	unsigned long idx = 0;
	xa_for_each(&plane->possible_crtcs, idx, crtc_entry) {
		if (crtc_entry == crtc) {
			mutex_unlock(&vkms_configfs->lock);
			return -EINVAL;
		}
	}

	if (vkms_config_plane_attach_crtc(plane, crtc))
		return -EINVAL;

	mutex_unlock(&vkms_configfs->lock);

	return 0;
}

static void possible_crtcs_drop_link(struct config_item *src,
				     struct config_item *target)
{
	struct vkms_config_crtc *crtc;
	struct vkms_configfs_device *vkms_configfs = plane_possible_crtc_src_item_to_vkms_configfs_device(src);

	mutex_lock(&vkms_configfs->lock);

	crtc = crtc_item_to_vkms_configfs_crtc(target)->vkms_config_crtc;
	struct vkms_config_plane*plane = plane_possible_crtc_src_item_to_vkms_configfs_plane(src)->vkms_config_plane;

	struct vkms_config_crtc  *crtc_entry;
	struct vkms_config_plane *plane_entry;
	unsigned long crtc_idx  = -1;
	xa_for_each(&plane->possible_crtcs, crtc_idx, crtc_entry) {
		if (crtc_entry == crtc)
			break;
	}
	unsigned long plane_idx = -1;
	xa_erase(&plane->possible_crtcs, crtc_idx);
	xa_for_each(&crtc->possible_planes, plane_idx, plane_entry) {
		if (plane_entry == plane) {
			break;
		}
	}
	xa_erase(&crtc->possible_planes, plane_idx);

	mutex_unlock(&vkms_configfs->lock);
}

static struct configfs_item_operations plane_possible_crtcs_item_ops = {
	.allow_link = &possible_crtcs_allow_link,
	.drop_link = &possible_crtcs_drop_link,
};

static struct config_item_type plane_possible_crtcs_group_type = {
	.ct_item_ops = &plane_possible_crtcs_item_ops,
	.ct_owner = THIS_MODULE,
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
	if (!vkms_configfs_plane->vkms_config_plane) {
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

	config_group_init_type_name(&vkms_configfs_plane->possible_crtc_group, "possible_crtcs",
				    &plane_possible_crtcs_group_type);
	configfs_add_default_group(&vkms_configfs_plane->possible_crtc_group,
				   &vkms_configfs_plane->group);
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

static void crtc_release(struct config_item *item)
{
	struct vkms_configfs_crtc *vkms_configfs_crtc = crtc_item_to_vkms_configfs_crtc(item);

	mutex_lock(&vkms_configfs_crtc->vkms_configfs_device->lock);
	vkms_config_delete_crtc(vkms_configfs_crtc->vkms_config_crtc, vkms_configfs_crtc->vkms_configfs_device->vkms_config);
	mutex_unlock(&vkms_configfs_crtc->vkms_configfs_device->lock);

	kfree(vkms_configfs_crtc);
}

static struct configfs_item_operations crtc_item_operations = {
	.release 	= crtc_release,
};

static const struct config_item_type   crtc_item_type       = {
	.ct_owner	= THIS_MODULE,
	.ct_item_ops 	= &crtc_item_operations,
};

static struct config_group *crtcs_make_group(struct config_group *config_group,
					     const char *name)
{
	struct config_item *root_item = config_group->cg_item.ci_parent;
	struct vkms_configfs_device *vkms_configfs = config_item_to_vkms_configfs_device(root_item);
	struct vkms_configfs_crtc *vkms_configfs_crtc;

	vkms_configfs_crtc = kzalloc(sizeof(*vkms_configfs_crtc), GFP_KERNEL);

	if (!vkms_configfs_crtc)
		return ERR_PTR(-ENOMEM);

	mutex_lock(&vkms_configfs->lock);
	vkms_configfs_crtc->vkms_configfs_device = vkms_configfs;

	if (vkms_configfs->enabled) {
		kfree(vkms_configfs_crtc);
		mutex_unlock(&vkms_configfs->lock);
		return ERR_PTR(-EINVAL);
	}

	vkms_configfs_crtc->vkms_config_crtc = vkms_config_create_crtc(vkms_configfs->vkms_config);

	if (!vkms_configfs_crtc->vkms_config_crtc) {
		kfree(vkms_configfs_crtc);
		mutex_unlock(&vkms_configfs->lock);
		return ERR_PTR(-ENOMEM);
	}

	vkms_configfs_crtc->vkms_config_crtc->name = kzalloc(strlen(name) + 1, GFP_KERNEL);
	if (!vkms_configfs_crtc->vkms_config_crtc->name) {
		kfree(vkms_configfs_crtc->vkms_config_crtc);
		kfree(vkms_configfs_crtc);
		mutex_unlock(&vkms_configfs->lock);
		return ERR_PTR(-ENOMEM);
	}

	vkms_configfs_crtc->vkms_configfs_device = vkms_configfs;

	strcpy(vkms_configfs_crtc->vkms_config_crtc->name, name);
	config_group_init_type_name(&vkms_configfs_crtc->group, name,
				    &crtc_item_type);


	mutex_unlock(&vkms_configfs->lock);

	return &vkms_configfs_crtc->group;
}


static struct configfs_group_operations crtcs_group_operations = {
	.make_group	= &crtcs_make_group,
};


static const struct config_item_type crtcs_item_type = {
	.ct_group_ops    = &crtcs_group_operations,
	.ct_owner        = THIS_MODULE,
};

static int encoder_possible_crtcs_allow_link(struct config_item *src,
					     struct config_item *target)
{
	struct vkms_config_crtc *crtc;
	struct vkms_configfs_device *vkms_configfs = encoder_possible_crtc_src_item_to_vkms_configfs_device(src);

	mutex_lock(&vkms_configfs->lock);

	if (target->ci_type != &crtc_item_type) {
		DRM_ERROR("Unable to link non-CRTCs.\n");
		mutex_unlock(&vkms_configfs->lock);
		return -EINVAL;
	}

	crtc = crtc_item_to_vkms_configfs_crtc(target)->vkms_config_crtc;
	struct vkms_config_encoder *encoder =encoder_possible_crtc_src_item_to_vkms_configfs_encoder(src)->vkms_config_encoder;

	struct vkms_config_crtc *crtc_entry;
	unsigned long           idx = 0;
	xa_for_each(&encoder->possible_crtcs, idx, crtc_entry) {
		if (crtc_entry == crtc) {
			pr_err("Tried to add two symlinks to the same CRTC from the same object"
			       ".\n");
			mutex_unlock(&vkms_configfs->lock);
			return -EINVAL;
		}
	}

	if (vkms_config_encoder_attach_crtc(encoder, crtc))
		return -EINVAL;

	mutex_unlock(&vkms_configfs->lock);

	return 0;
}

static void encoder_possible_crtcs_drop_link(struct config_item *src,
					     struct config_item *target)
{
	struct vkms_config_crtc *crtc;
	struct vkms_configfs_device *vkms_configfs = encoder_possible_crtc_src_item_to_vkms_configfs_device(src);

	mutex_lock(&vkms_configfs->lock);

	crtc = crtc_item_to_vkms_configfs_crtc(target)->vkms_config_crtc;
	struct vkms_config_encoder *encoder = encoder_possible_crtc_src_item_to_vkms_configfs_encoder(src)->vkms_config_encoder;

	struct vkms_config_encoder *encoder_entry;
	struct vkms_config_crtc *crtc_entry;
	unsigned long encoder_idx = -1;
	unsigned long crtc_idx = -1;
	xa_for_each(&encoder->possible_crtcs, crtc_idx, crtc_entry) {
		if (crtc_entry == crtc)
			break;
	}
	xa_erase(&encoder->possible_crtcs, crtc_idx);
	xa_for_each(&crtc->possible_encoders, encoder_idx, encoder_entry) {
		if (encoder_entry == encoder)
			break;
	}
	xa_erase(&crtc->possible_encoders, encoder_idx);

	mutex_unlock(&vkms_configfs->lock);
}

static struct configfs_item_operations encoder_possible_crtcs_item_operations = {
	.allow_link	= &encoder_possible_crtcs_allow_link,
	.drop_link	= &encoder_possible_crtcs_drop_link,
};

static struct config_item_type encoder_possible_crtcs_item_type = {
	.ct_item_ops	= &encoder_possible_crtcs_item_operations,
	.ct_owner	= THIS_MODULE,
};

static void encoder_release(struct config_item *item)
{
	struct vkms_configfs_encoder *vkms_configfs_encoder = encoder_item_to_vkms_configfs_encoder(item);

	mutex_lock(&vkms_configfs_encoder->vkms_configfs_device->lock);
	vkms_config_delete_encoder(vkms_configfs_encoder->vkms_config_encoder, vkms_configfs_encoder->vkms_configfs_device->vkms_config);
	mutex_unlock(&vkms_configfs_encoder->vkms_configfs_device->lock);

	kfree(vkms_configfs_encoder);
}

static struct configfs_item_operations encoder_item_operations = {
	.release 	= encoder_release,
};

static const struct config_item_type encoder_item_type = {
	.ct_item_ops	= &encoder_item_operations,
	.ct_owner        = THIS_MODULE,
};

static struct config_group *encoder_make_group(struct config_group *config_group,
					       const char *name)
{
	struct vkms_configfs_device *vkms_configfs = encoder_item_to_vkms_configfs_device(&config_group->cg_item);
	struct vkms_configfs_encoder *vkms_configfs_encoder;

	vkms_configfs_encoder = kzalloc(sizeof(*vkms_configfs_encoder), GFP_KERNEL);

	if (!vkms_configfs_encoder)
		return ERR_PTR(-ENOMEM);

	mutex_lock(&vkms_configfs->lock);

	if (vkms_configfs->enabled) {
		kfree(vkms_configfs_encoder);
		mutex_unlock(&vkms_configfs->lock);
		return ERR_PTR(-EINVAL);
	}

	vkms_configfs_encoder->vkms_config_encoder = vkms_config_create_encoder(
		vkms_configfs->vkms_config);

	if (!vkms_configfs_encoder->vkms_config_encoder) {
		kfree(vkms_configfs_encoder);
		mutex_unlock(&vkms_configfs->lock);
		return ERR_PTR(-ENOMEM);
	}

	vkms_configfs_encoder->vkms_config_encoder->name = kzalloc(strlen(name) + 1, GFP_KERNEL);
	if (!vkms_configfs_encoder->vkms_config_encoder->name) {
		kfree(vkms_configfs_encoder->vkms_config_encoder);
		kfree(vkms_configfs_encoder);
		mutex_unlock(&vkms_configfs->lock);
		return ERR_PTR(-ENOMEM);
	}

	strcpy(vkms_configfs_encoder->vkms_config_encoder->name, name);
	config_group_init_type_name(&vkms_configfs_encoder->group, name,
				    &encoder_item_type);

	config_group_init_type_name(&vkms_configfs_encoder->possible_crtc_group, "possible_crtcs",
				    &encoder_possible_crtcs_item_type);
	configfs_add_default_group(&vkms_configfs_encoder->possible_crtc_group,
				   &vkms_configfs_encoder->group);
	vkms_configfs_encoder->vkms_configfs_device = vkms_configfs;

	mutex_unlock(&vkms_configfs->lock);

	return &vkms_configfs_encoder->group;
}

static struct configfs_group_operations encoder_group_operations = {
	.make_group	= &encoder_make_group,
};

static const struct config_item_type encoders_item_type = {
	.ct_group_ops	= &encoder_group_operations,
	.ct_owner	= THIS_MODULE,
};

/**
 * configfs_lock_dependencies() - In order to forbid the userspace to delete items when the
 * device is enabled, mark all configfs items as dependent
 *
 * @vkms_configfs_device - Device to lock
 */
static void configfs_lock_dependencies(struct vkms_configfs_device *vkms_configfs_device)
{
	/* Lock the group itself */
	configfs_depend_item_unlocked(vkms_configfs_device->group.cg_subsys,
				      &vkms_configfs_device->group.cg_item);
	/* Lock the planes elements */

	struct config_item *item;
	list_for_each_entry(item, &vkms_configfs_device->plane_group.cg_children, ci_entry) {
		configfs_depend_item_unlocked(vkms_configfs_device->plane_group.cg_subsys,
					      item);
	}
	list_for_each_entry(item, &vkms_configfs_device->crtc_group.cg_children, ci_entry) {
		configfs_depend_item_unlocked(vkms_configfs_device->crtc_group.cg_subsys,
					      item);
	}
}

/**
 * configfs_unlock_dependencies() - Once the device is disable, its configuration can be modified.
 *
 * @vkms_configfs_device - Device to unlock
 */
static void configfs_unlock_dependencies(struct vkms_configfs_device *vkms_configfs_device)
{
	configfs_undepend_item_unlocked(&vkms_configfs_device->group.cg_item);

	struct config_item *item;
	list_for_each_entry(item, &vkms_configfs_device->plane_group.cg_children, ci_entry) {
		configfs_undepend_item_unlocked(item);
	}
	list_for_each_entry(item, &vkms_configfs_device->crtc_group.cg_children, ci_entry) {
		configfs_undepend_item_unlocked(item);
	}
}


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
	if (vkms_configfs_device->enabled == value) {
		mutex_unlock(&vkms_configfs_device->lock);
		return (ssize_t) count;
	}

	if (value && !vkms_config_is_valid(vkms_configfs_device->vkms_config)) {
		mutex_unlock(&vkms_configfs_device->lock);
		return -EINVAL;
	}

	vkms_configfs_device->enabled = value;

	if (value) {
		configfs_lock_dependencies(vkms_configfs_device);
		vkms_configfs_device->platform_device = vkms_create_device(&platform_data);
	} else {
		configfs_unlock_dependencies(vkms_configfs_device);
		vkms_delete_device(vkms_configfs_device->platform_device);
	}

	mutex_unlock(&vkms_configfs_device->lock);

	return (ssize_t)count;
}

static ssize_t device_writeback_show(struct config_item *item, char *page)
{
	return sprintf(page, "%d\n",
		       config_item_to_vkms_configfs_device(item)->vkms_config->writeback);
}

static ssize_t device_writeback_store(struct config_item *item,
				      const char *page, size_t count)
{
	struct vkms_configfs_device *vkms_configfs_device =
					    config_item_to_vkms_configfs_device(item);
	bool                        value;
	int                         ret;

	ret = kstrtobool(page, &value);
	if (ret)
		return -EINVAL;

	mutex_lock(&vkms_configfs_device->lock);
	if (vkms_configfs_device->enabled) {
		mutex_unlock(&vkms_configfs_device->lock);
		return -EINVAL;
	}

	vkms_configfs_device->vkms_config->writeback = value;

	mutex_unlock(&vkms_configfs_device->lock);

	return count;
}

CONFIGFS_ATTR(device_, enable);
CONFIGFS_ATTR(device_, writeback);

static struct configfs_attribute *device_attrs[] = {
	&device_attr_enable,
	&device_attr_writeback,
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
	struct vkms_configfs_device *configfs = kzalloc(sizeof(*configfs), GFP_KERNEL);

	if (!configfs)
		return ERR_PTR(-ENOMEM);

	mutex_init(&configfs->lock);

	configfs->vkms_config = vkms_config_alloc();

	if (!configfs->vkms_config) {
		kfree(configfs);
		return ERR_PTR(-ENOMEM);
	}

	config_group_init_type_name(&configfs->group, name,
				    &device_item_type);

	config_group_init_type_name(&configfs->plane_group, "planes", &planes_item_type);
	configfs_add_default_group(&configfs->plane_group, &configfs->group);

	config_group_init_type_name(&configfs->crtc_group, "crtcs", &crtcs_item_type);
	configfs_add_default_group(&configfs->crtc_group, &configfs->group);

	config_group_init_type_name(&configfs->encoder_group, "encoders", &encoders_item_type);
	configfs_add_default_group(&configfs->encoder_group, &configfs->group);

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
