// SPDX-License-Identifier: GPL-2.0+

#include "vkms_mst.h"

#include <drm/drm_print.h>
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
		case DP_MAX_LINK_RATE:
			*buffer = emulator->dpcd_memory.MAX_LINK_RATE;
			break;
		case DP_MAX_LANE_COUNT:
			*buffer = emulator->dpcd_memory.MAX_LANE_COUNT;
			break;
		case DP_MAX_DOWNSPREAD:
			*buffer = emulator->dpcd_memory.MAX_DOWNSPREAD;
			break;
		case DP_NORP:
			*buffer = emulator->dpcd_memory.NORP;
			break;
		case DP_DOWNSTREAMPORT_PRESENT:
			*buffer = emulator->dpcd_memory.DOWNSTREAMPORT_PRESENT;
			break;
		case DP_MAIN_LINK_CHANNEL_CODING:
			*buffer = emulator->dpcd_memory.MAIN_LINK_CHANNEL_CODING;
			break;
		case DP_DOWN_STREAM_PORT_COUNT:
			*buffer = emulator->dpcd_memory.DOWNSTREAM_PORT_COUNT;
			break;
		case DP_RECEIVE_PORT_0_CAP_0:
			*buffer = emulator->dpcd_memory.RECEIVE_PORT_0_CAP_0;
			break;
		case DP_RECEIVE_PORT_0_BUFFER_SIZE:
			*buffer = emulator->dpcd_memory.RECEIVE_PORT_0_BUFFER_SIZE;
			break;
		case DP_RECEIVE_PORT_1_CAP_0:
			*buffer = emulator->dpcd_memory.RECEIVE_PORT_1_CAP_0;
			break;
		case DP_RECEIVE_PORT_1_BUFFER_SIZE:
			*buffer = emulator->dpcd_memory.RECEIVE_PORT_1_BUFFER_SIZE;
			break;
		case DP_I2C_SPEED_CAP:
			*buffer = emulator->dpcd_memory.I2C_SPEED_CAP;
			break;
		case DP_EDP_CONFIGURATION_CAP:
			*buffer = emulator->dpcd_memory.EDP_CONFIGURATION_CAP;
			break;
		case DP_TRAINING_AUX_RD_INTERVAL:
			*buffer = emulator->dpcd_memory.TRAINING_AUX_RD_INTERVAL;
			break;
		case DP_MSTM_CAP:
			*buffer = emulator->dpcd_memory.MSTM_CAP;
			break;
		case DP_PAYLOAD_TABLE_UPDATE_STATUS:
			*buffer = emulator->dpcd_memory.PAYLOAD_TABLE_UPDATE_STATUS;
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
		case DP_MSTM_CTRL:
			emulator->dpcd_memory.MSTM_CTRL = *buffer;
			break;
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

struct vkms_mst_transfer_helpers vkms_mst_transfer_helpers_default = {
	.transfer = vkms_mst_emulator_transfer_default,
	.transfer_read = vkms_mst_emulator_transfer_read_default,
	.transfer_write = vkms_mst_emulator_transfer_write_default,
	.transfer_i2c_read = vkms_mst_emulator_transfer_i2c_read_default,
	.transfer_i2c_write = vkms_mst_emulator_transfer_i2c_write_default
};
EXPORT_SYMBOL(vkms_mst_transfer_helpers_default);
