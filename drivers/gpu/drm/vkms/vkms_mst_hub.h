#ifndef _VKMS_MST_HUB_
#define _VKMS_MST_HUB_
#include "vkms_mst.h"

struct vkms_mst_hub_emulator {
	struct vkms_mst_emulator base;
};

void vkms_mst_hub_emulator_init(struct vkms_mst_hub_emulator *vkms_mst_hub_emulator,unsigned int children_count, const char *name);

struct vkms_mst_hub_emulator *vkms_mst_hub_emulator_alloc(unsigned int children_count, const char *name);

#endif /* _VKMS_MST_HUB_ */
