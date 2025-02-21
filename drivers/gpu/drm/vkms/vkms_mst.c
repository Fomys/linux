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

void vkms_mst_emulator_init(struct vkms_mst_emulator *emulator,
			    const struct vkms_mst_transfer_helpers *transfer_helpers,
			    const char *name)
{
	vkms_mst_emulator_init_memory(&emulator->dpcd_memory);

	emulator->transfer_helpers = transfer_helpers;
	emulator->name = kstrdup_const(name, GFP_KERNEL);
}

void vkms_mst_emulator_destroy(struct vkms_mst_emulator *emulator)
{
	kfree_const(emulator->name);
}

ssize_t vkms_mst_emulator_transfer_default(struct vkms_mst_emulator *emulator, u8 port_id, struct drm_dp_aux_msg *msg)
{
	if (emulator->transfer_helpers) {
		switch (msg->request & ~DP_AUX_I2C_MOT) {
		case DP_AUX_NATIVE_READ:
			if (emulator->transfer_helpers->transfer_read)
				return emulator->transfer_helpers->transfer_read(emulator, port_id, msg);
			break;
		case DP_AUX_NATIVE_WRITE:
			if (emulator->transfer_helpers->transfer_write)
				return emulator->transfer_helpers->transfer_write(emulator, port_id, msg);
			break;
		case DP_AUX_I2C_WRITE:
			if (emulator->transfer_helpers->transfer_i2c_write)
				return emulator->transfer_helpers->transfer_i2c_write(emulator, port_id, msg);
			break;
		case DP_AUX_I2C_READ:
			if (emulator->transfer_helpers->transfer_i2c_read)
				return emulator->transfer_helpers->transfer_i2c_read(emulator, port_id, msg);
			break;
		default:
			break;
		}
	}

	return -EPROTO;
}

ssize_t vkms_mst_emulator_transfer_read_default(struct vkms_mst_emulator *emulator, u8 port_id, struct drm_dp_aux_msg *msg)
{
	if (msg->request != DP_AUX_NATIVE_READ) {
		pr_err("Wrong request type, the caller must only call this with DP_AUX_NATIVE_READ requests.\n");
		return -EPROTO;
	}
	u8 *buffer = msg->buffer;
	msg->reply = DP_AUX_NATIVE_REPLY_ACK;

	int i = 0;
	while (i < msg->size) {
		int curr_offset = msg->address + i;
		switch (curr_offset) {
		case DP_DPCD_REV:
			*buffer = emulator->dpcd_memory.DPCD_REV;
			break;
		default:
			*buffer = 0;
			msg->reply = DP_AUX_NATIVE_REPLY_NACK;
			pr_err("Requested reading unsupported register %d\n", curr_offset);
			return -EPROTO;
		}

		buffer++;
		i++;
	}

	return i;
}

ssize_t vkms_mst_emulator_transfer_write_default(struct vkms_mst_emulator *emulator, u8 port_id, struct drm_dp_aux_msg *msg)
{
	if (msg->request != DP_AUX_NATIVE_WRITE) {
		pr_err("Wrong request type, the caller must only call this with DP_AUX_NATIVE_WRITE requests.\n");
		return 0;
	}
	u8 *buffer = msg->buffer;
	msg->reply = DP_AUX_NATIVE_REPLY_ACK;

	int i = 0;
	while (i < msg->size) {
		int curr_offset = msg->address + i;

		switch (curr_offset) {
		default:
			*buffer = 0;
			msg->reply = DP_AUX_NATIVE_REPLY_NACK;
			pr_err("Requested write unsupported register %d\n", curr_offset);
			return -EPROTO;
		}

		buffer++;
		i++;
	}
	return i;
}

ssize_t vkms_mst_emulator_transfer_i2c_write_default(struct vkms_mst_emulator *emulator, u8 port_id, struct drm_dp_aux_msg *msg)
{
	if ((msg->request & ~DP_AUX_I2C_MOT) != DP_AUX_I2C_WRITE) {
		pr_err("Wrong request type, the caller must only call this with DP_AUX_I2C_WRITE requests.\n");
		return 0;
	}
	msg->reply = DP_AUX_I2C_REPLY_NACK;

	return -EPROTO;
}

ssize_t vkms_mst_emulator_transfer_i2c_read_default(struct vkms_mst_emulator *emulator, u8 port_id, struct drm_dp_aux_msg *msg)
{
	if ((msg->request & ~DP_AUX_I2C_MOT) != DP_AUX_I2C_READ) {
		pr_err("Wrong request type, the caller must only call this with DP_AUX_I2C_READ requests.\n");
		return 0;
	}
	msg->reply = DP_AUX_I2C_REPLY_NACK;

	return -EPROTO;
}