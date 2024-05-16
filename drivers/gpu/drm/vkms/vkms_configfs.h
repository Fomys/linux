/* SPDX-License-Identifier: GPL-2.0+ */

#include <linux/configfs.h>
#include <linux/mutex.h>

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
	struct platform_device *platform_device;

	struct mutex lock;
	bool enabled;

	struct vkms_config *vkms_config;
};

#define config_item_to_vkms_configfs_device(item) \
	container_of(to_config_group((item)), struct vkms_configfs_device, group)

/* ConfigFS Support */
int vkms_init_configfs(void);
void vkms_unregister_configfs(void);

#endif /* _VKMS_CONFIGFS_H */
