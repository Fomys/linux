// SPDX-License-Identifier: GPL-2.0+

#include "vkms_mst.h"

#include <linux/string.h>

#include <drm/display/drm_dp_mst_helper.h>

/**
 * vkms_mst_emulator_init_memory - Initialize the DPCD memory of the device
 */
static void vkms_mst_emulator_init_memory(struct vkms_dpcd_memory * dpcd_memory)
{
	memset(dpcd_memory, 0, sizeof(*dpcd_memory));

	dpcd_memory->DPCD_REV = DP_DPCD_REV_14;
}

void vkms_mst_emulator_init(struct vkms_mst_emulator *emulator,	const char *name)
{
	vkms_mst_emulator_init_memory(&emulator->dpcd_memory);
	memset(&emulator->dpcd_memory, 0, sizeof(emulator->dpcd_memory));
	emulator->name = kstrdup_const(name, GFP_KERNEL);
}

void vkms_mst_emulator_destroy(struct vkms_mst_emulator *emulator)
{
	kfree_const(emulator->name);
}