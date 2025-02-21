// SPDX-License-Identifier: GPL-2.0+

#include "vkms_mst.h"

#include <drm/drm_print.h>
#include <drm/display/drm_dp_mst_helper.h>
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

static void vkms_mst_emulation_down_req_worker(struct work_struct *work)
{
	struct vkms_mst_emulator *emulator =
		container_of(work, struct vkms_mst_emulator, w_req);

	struct drm_dp_sideband_msg_req_body req = { 0 };
	struct drm_dp_sideband_msg_reply_body rep = { 0 };
	struct drm_dp_sideband_msg_hdr req_hdr = { 0 };
	struct drm_dp_sideband_msg_hdr rep_hdr = { 0 };
	struct drm_dp_sideband_msg_tx raw_rep = { 0 };
	u8 req_hdr_len = 0;
	int rep_hdr_len = 0;

	bool success = drm_dp_decode_sideband_msg_hdr(
		NULL, &req_hdr, emulator->dpcd_memory.DOWN_REQ,
		sizeof(emulator->dpcd_memory.DOWN_REQ), &req_hdr_len);
	if (!success)
		goto end;

	// TODO: DPCD sideband request can be splitted, need to support this here

	if (req_hdr.broadcast) {
		if (req_hdr.lct != 1)
			pr_warn("Malformed header for a sideband broadcast message.");
	}

	drm_dp_decode_sideband_req((void *)emulator->dpcd_memory.DOWN_REQ + req_hdr_len,
				   &req);

	rep.req_type = req.req_type;
	rep.reply_type = DP_SIDEBAND_REPLY_NAK;
	guid_copy(&rep.u.nak.guid, &emulator->dpcd_memory.GUID);
	rep.u.nak.reason = DP_NAK_BAD_PARAM;

	switch (req.req_type) {
	default:
		pr_err("Unsupported request %s, ignoring\n", drm_dp_mst_req_type_str(req.req_type));
		break;
	}
end:
	drm_dp_encode_sideband_reply(&rep, &raw_rep);

	memset(&emulator->dpcd_memory.DOWN_REP, 0,
	       sizeof(emulator->dpcd_memory.DOWN_REP));

	rep_hdr.broadcast = req_hdr.broadcast;
	rep_hdr.eomt = 1;
	rep_hdr.somt = 1;
	rep_hdr.lcr = req_hdr.lcr;
	rep_hdr.lct = req_hdr.lct;
	memcpy(&rep_hdr.rad, &req_hdr.rad, ARRAY_SIZE(req_hdr.rad));
	rep_hdr.seqno = req_hdr.seqno;
	rep_hdr.path_msg = req_hdr.path_msg;

	rep_hdr.msg_len = raw_rep.cur_len + 1;
	drm_dp_encode_sideband_msg_hdr(&rep_hdr, emulator->dpcd_memory.DOWN_REP,
				       &rep_hdr_len);
	memcpy(&emulator->dpcd_memory.DOWN_REP[rep_hdr_len], raw_rep.msg, raw_rep.cur_len);
	drm_dp_crc_sideband_chunk_req(&emulator->dpcd_memory.DOWN_REP[rep_hdr_len],
				      raw_rep.cur_len);

	emulator->dpcd_memory.DEVICE_SERVICE_IRQ_VECTOR_ESI0 |= DP_DOWN_REP_MSG_RDY;
	// emulator->irq_handler(vkms_mst_emulator, vkms_mst_emulator->irq_handler_data);
}

void vkms_mst_emulator_init(struct vkms_mst_emulator *emulator,
			    const struct vkms_mst_transfer_helpers *transfer_helpers,
			    const enum vkms_mst_port_kind port_kinds[VKMS_MST_MAX_PORTS],
			    const char *name)
{
	vkms_mst_emulator_init_memory(&emulator->dpcd_memory);

	emulator->wq_req = alloc_ordered_workqueue("%s-req", 0, name);
	INIT_WORK(&emulator->w_req, vkms_mst_emulation_down_req_worker);

	for (int i = 0; i < VKMS_MST_MAX_PORTS; i++) {
		emulator->ports[i].to = NULL;
		emulator->ports[i].kind = port_kinds[i];
		emulator->ports[i].other_port_id = -1;
	}

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

		if (curr_offset >= DP_SIDEBAND_MSG_DOWN_REP_BASE &&
		    curr_offset <= DP_SIDEBAND_MSG_DOWN_REP_BASE + 0x200) {
			int msg_end = min(DP_SIDEBAND_MSG_DOWN_REP_BASE + 0x200,
					  msg->address + msg->size);
			int msg_size = msg_end - curr_offset;
			memcpy(buffer,
			       &emulator->dpcd_memory
					.DOWN_REP[curr_offset -
						  DP_SIDEBAND_MSG_DOWN_REP_BASE],
			       msg_size);
			buffer += msg_size;
			i += msg_size;
			continue;
		}

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
		case DP_SINK_COUNT:
		case DP_SINK_COUNT_ESI:
			*buffer = emulator->dpcd_memory.SINK_COUNT_ESI;
			break;
		case DP_DEVICE_SERVICE_IRQ_VECTOR:
		case DP_DEVICE_SERVICE_IRQ_VECTOR_ESI0:
			*buffer = emulator->dpcd_memory.DEVICE_SERVICE_IRQ_VECTOR_ESI0;
			break;
		case DP_LINK_SERVICE_IRQ_VECTOR_ESI0:
			*buffer = emulator->dpcd_memory.LINK_SERVICE_IRQ_VECTOR_ESI0;
			break;
		case DP_DEVICE_SERVICE_IRQ_VECTOR_ESI1:
			*buffer = emulator->dpcd_memory.DEVICE_SERVICE_IRQ_VECTOR_ESI1;
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

static void
vkms_mst_emulator_clear_vc_payload_id_table_if_needed(struct vkms_mst_emulator *emulator)
{
	if (emulator->dpcd_memory.PAYLOAD_ALLOCATE_SET == 0x00 &&
	    emulator->dpcd_memory.PAYLOAD_ALLOCATE_START_TIME_SLOT == 0x00 &&
	    emulator->dpcd_memory.PAYLOAD_ALLOCATE_TIME_SLOT_COUNT == 0x00) {
		emulator->dpcd_memory.PAYLOAD_TABLE_UPDATE_STATUS |= DP_PAYLOAD_TABLE_UPDATED;
		pr_err("TODO: Clear payload id table for mst device %s\n", emulator->name);
	    }
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

		if (curr_offset >= DP_SIDEBAND_MSG_DOWN_REQ_BASE &&
		    curr_offset <= DP_SIDEBAND_MSG_DOWN_REQ_BASE + 0x200) {
			int msg_end = min(DP_SIDEBAND_MSG_DOWN_REQ_BASE + 0x200,
					  msg->address + msg->size);
			int msg_size = msg_end - curr_offset;

			memcpy(&emulator->dpcd_memory.DOWN_REQ, buffer,
			       msg_size);

			int ret = queue_work(emulator->wq_req, &emulator->w_req);
			if (!ret)
				pr_warn("A down request is already pending for %s.\n", emulator->name);

			buffer += msg_size;
			i += msg_size;
			continue;
		}

		if (curr_offset >= DP_GUID && curr_offset <= DP_GUID + 0x10) {
			int msg_end =
				min(DP_SIDEBAND_MSG_DOWN_REP_BASE + 0x10, msg->address + msg->size);
			int msg_size = msg_end - curr_offset;

			import_guid(&emulator->dpcd_memory.GUID, buffer);

			buffer += msg_size;
			i += msg_size;
			continue;
		}

		switch (curr_offset) {
		case DP_MSTM_CTRL:
			emulator->dpcd_memory.MSTM_CTRL = *buffer;
			break;
		case DP_PAYLOAD_TABLE_UPDATE_STATUS:
			if (*buffer & DP_PAYLOAD_TABLE_UPDATED)
				emulator->dpcd_memory.PAYLOAD_TABLE_UPDATE_STATUS &=
					~DP_PAYLOAD_TABLE_UPDATED;
			if (*buffer & DP_PAYLOAD_ACT_HANDLED)
				emulator->dpcd_memory.PAYLOAD_TABLE_UPDATE_STATUS &=
					~DP_PAYLOAD_ACT_HANDLED;
			break;
		case DP_PAYLOAD_ALLOCATE_SET:
			emulator->dpcd_memory.PAYLOAD_ALLOCATE_SET = *buffer;
			vkms_mst_emulator_clear_vc_payload_id_table_if_needed(emulator);
			break;
		case DP_PAYLOAD_ALLOCATE_START_TIME_SLOT:
			emulator->dpcd_memory.PAYLOAD_ALLOCATE_START_TIME_SLOT = *buffer;
			vkms_mst_emulator_clear_vc_payload_id_table_if_needed(emulator);
			break;
		case DP_PAYLOAD_ALLOCATE_TIME_SLOT_COUNT:
			emulator->dpcd_memory.PAYLOAD_ALLOCATE_TIME_SLOT_COUNT = *buffer;
			vkms_mst_emulator_clear_vc_payload_id_table_if_needed(emulator);
			break;
		case DP_DEVICE_SERVICE_IRQ_VECTOR:
		case DP_DEVICE_SERVICE_IRQ_VECTOR_ESI0:
			emulator->dpcd_memory.DEVICE_SERVICE_IRQ_VECTOR_ESI0 ^= *buffer &
							   0x7F;
			break;
		case DP_DEVICE_SERVICE_IRQ_VECTOR_ESI1:
			emulator->dpcd_memory.DEVICE_SERVICE_IRQ_VECTOR_ESI1 ^= *buffer &
								   0x1F;
			break;
		case DP_LINK_SERVICE_IRQ_VECTOR_ESI0:
			emulator->dpcd_memory.LINK_SERVICE_IRQ_VECTOR_ESI0 ^= *buffer & 0x1F;
			break;
		case DP_SINK_COUNT_ESI: /* Read only registers, no effect */
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
