#ifndef VKMS_MST_DISPLAY_H_
#define VKMS_MST_DISPLAY_H_

#include "vkms_mst.h"

struct vkms_mst_display_emulator {
	struct vkms_mst_emulator base;
};

void vkms_mst_display_emulator_init(struct vkms_mst_display_emulator *vkms_mst_display_emulator, const char* name);

struct vkms_mst_display_emulator *vkms_mst_display_emulator_alloc(const char* name);

#endif /* VKMS_MST_DISPLAY_H_ */
