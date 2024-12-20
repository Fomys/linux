#ifndef _VKMS_MST_H_
#define _VKMS_MST_H_

#include <linux/types.h>

/**
 * struct vkms_dpcd_memory - Representation of the DPCD memory of a device
 * For exact meaning of each field, refer to the DisplayPort specification.
 */
struct vkms_dpcd_memory {
	u8 DPCD_REV;
};

/**
 * struct vkms_mst_emulator - Base structure for all MST device emulators.
 *
 * @dpcd_memory: Representation of the internal DPCD memory. This is a private
 *               field that should not be accessed outside the device itself.
 * @name: Name of the device. Mainly used for logging purpose.
 */
struct vkms_mst_emulator {
	struct vkms_dpcd_memory dpcd_memory;

	const char *name;
};

/**
 * vkms_mst_emulator_init - Initialize an MST emulator device
 * @emulator: Structure to initialize
 * @name: Name of the device. Used mainly for logging purpose.
 */
void vkms_mst_emulator_init(struct vkms_mst_emulator *emulator,	const char *name);

/**
 * vkms_mst_emulator_destroy: Destroy all resources allocated for an emulator
 * @emulator: Device to destroy
 */
void vkms_mst_emulator_destroy(struct vkms_mst_emulator *emulator);

#endif
