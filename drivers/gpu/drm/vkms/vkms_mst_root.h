#ifndef _VKMS_MST_ROOT_H_
#define _VKMS_MST_ROOT_H_

#include "vkms_mst.h"

/**
 * struct vkms_mst_emulator_root - Root MST device, representing the main device.
 * It is used to emulate for example up request from a children device.
 *
 * @base: Base emulator
 */
struct vkms_mst_emulator_root {
	struct vkms_mst_emulator base;
};

/**
 * vkms_mst_emulator_root_init - Initialize a root emulated MST device
 *
 * @emulator: Structure to initialize
 * @name: Name of the root device. Mainly used for logging purpose.
 */
void vkms_mst_emulator_root_init(struct vkms_mst_emulator_root *emulator,
				 const char *name);

/**
 * vkms_mst_emulator_root_transfer -
 * @emulator: Root device that should initiate the transfer
 *
 * A root device is used to emulate the main device itself. This function is
 * used to request a transfer on the only output of this main device.
 *
 */
ssize_t vkms_mst_emulator_root_transfer(struct vkms_mst_emulator_root *emulator,
					struct drm_dp_aux_msg *msg);

#endif
