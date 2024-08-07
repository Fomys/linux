/* SPDX-License-Identifier: GPL-2.0+ */

#include <linux/configfs.h>
#include <linux/mutex.h>
#include "vkms_device_drv.h"

#ifndef _VKMS_CONFIGFS_H
#define _VKMS_CONFIGFS_H

/**
 * struct vkms_configfs_device - Internal object to manage all the configfs items related to one
 * device
 *
 * @group: Main configfs group for a device
 * @platform_device: If a device was created (@enabled = true), stores a pointer to it
 * @lock: Mutex used to avoid conflicting edition of @vkms_config
 * @enabled: Store if the device was created or not
 * @vkms_config: Current vkms configuration
 */
struct vkms_configfs_device {
	struct config_group group;

	struct config_group plane_group;
	struct config_group crtc_group;

	struct config_group encoder_group;
	struct platform_device *platform_device;

	struct mutex lock;
	bool enabled;

	struct vkms_config *vkms_config;
};

struct vkms_configfs_plane {
	struct config_group group;
	struct config_group possible_crtc_group;

	struct vkms_configfs_device *vkms_configfs_device;
	struct vkms_config_plane *vkms_config_plane;
};

struct vkms_configfs_crtc {
	struct config_group group;

	struct vkms_configfs_device *vkms_configfs_device;
	struct vkms_config_crtc *vkms_config_crtc;
};

struct vkms_configfs_encoder {
	/* must be first because it is a krefcounted stuff */
	struct config_group group;

	struct config_group possible_crtc_group;
	struct vkms_configfs_device *vkms_configfs_device;
	struct vkms_config_encoder *vkms_config_encoder;
};

#define config_item_to_vkms_configfs_device(item) \
	container_of(to_config_group((item)), struct vkms_configfs_device, group)

#define planes_item_to_vkms_configfs_device(item) \
	config_item_to_vkms_configfs_device((item)->ci_parent)

#define encoders_item_to_vkms_configfs_device(item) \
        config_item_to_vkms_configfs_device((item)->ci_parent)

#define crtc_item_to_vkms_configfs_crtc(item) \
        container_of(to_config_group((item)), struct vkms_configfs_crtc, group)

#define encoder_item_to_vkms_configfs_encoder(item) \
        container_of(to_config_group((item)), struct vkms_configfs_encoder, group)

#define plane_item_to_vkms_configfs_device(item) \
	planes_item_to_vkms_configfs_device((item)->ci_parent)

#define plane_item_to_vkms_configfs_plane(item) \
	container_of(to_config_group((item)), struct vkms_configfs_plane, group)

#define plane_possible_crtc_src_item_to_vkms_configfs_device(item) \
	plane_item_to_vkms_configfs_device((item)->ci_parent)

#define plane_possible_crtc_src_item_to_vkms_configfs_plane(item) \
        plane_item_to_vkms_configfs_plane((item)->ci_parent)

#define crtc_item_to_vkms_configfs_device(item) \
	config_item_to_vkms_configfs_device((item)->ci_parent)

#define crtc_child_item_to_vkms_configfs_device(item) \
        crtc_item_to_vkms_configfs_device((item)->ci_parent)

#define encoder_item_to_vkms_configfs_device(item) \
	config_item_to_vkms_configfs_device((item)->ci_parent)

#define encoder_child_item_to_vkms_configfs_device(item) \
	encoder_item_to_vkms_configfs_device((item)->ci_parent)

#define encoder_possible_crtc_src_item_to_vkms_configfs_device(item) \
	encoder_child_item_to_vkms_configfs_device((item)->ci_parent)

#define encoder_possible_crtc_src_item_to_vkms_configfs_encoder(item) \
	encoder_item_to_vkms_configfs_encoder((item)->ci_parent)

/* ConfigFS Support */
int vkms_init_configfs(void);

void vkms_unregister_configfs(void);

#endif /* _VKMS_CONFIGFS_H */
