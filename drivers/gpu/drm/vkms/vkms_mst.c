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

void send_next_down_rep(struct vkms_mst_emulator *emulator, u8 port_id)
{
	if (emulator->rep_to_send_content && emulator->rep_to_send_content_len == 0) {
		return;
	}

	memset(&emulator->dpcd_memory.DOWN_REP, 0, sizeof(emulator->dpcd_memory.DOWN_REP));
	int rep_content_len = min(42 - 1, emulator->rep_to_send_content_len);
	int rep_hdr_len = 0;

	if (rep_content_len == emulator->rep_to_send_content_len)
		emulator->rep_to_send_header.eomt = 1;
	else
		emulator->rep_to_send_header.eomt = 0;
	emulator->rep_to_send_header.msg_len = rep_content_len + 1;

	drm_dp_encode_sideband_msg_hdr(&emulator->rep_to_send_header,
				       &emulator->dpcd_memory.DOWN_REP[0], &rep_hdr_len);
	emulator->rep_to_send_header.somt = 0;

	memcpy(&emulator->dpcd_memory.DOWN_REP[rep_hdr_len],
	       emulator->rep_to_send_content, rep_content_len);
	drm_dp_crc_sideband_chunk_req(&emulator->dpcd_memory.DOWN_REP[rep_hdr_len],
				      rep_content_len);

	emulator->rep_to_send_content_len -= rep_content_len;

	memmove(emulator->rep_to_send_content,
		&emulator->rep_to_send_content[rep_content_len],
		emulator->rep_to_send_content_len);

	emulator->dpcd_memory.DEVICE_SERVICE_IRQ_VECTOR_ESI0 |= DP_DOWN_REP_MSG_RDY;

	//vkms_mst_call_irq(emulator, port_id);
}

/**
 * rad_move_left - Rotate the relative adress to left and insert @port_id
 */
static void rad_move_left(struct drm_dp_sideband_msg_hdr *req_hdr, u8 port_id)
{
	int i;
	for (i = 0; i < req_hdr->lcr - 1; i++) {
		if (i % 2 == 0) {
			req_hdr->rad[i / 2] <<= 4;
		} else {
			req_hdr->rad[i / 2] &= 0xF0;
			req_hdr->rad[i / 2] |= req_hdr->rad[i / 2 + 1] >> 4;
		}
	}
	if (i % 2 == 0) {
		req_hdr->rad[i / 2] = port_id << 4;
	} else {
		req_hdr->rad[i / 2] &= 0xF0;
		req_hdr->rad[i / 2] |= port_id;
	}
}

ssize_t vkms_mst_transfer(struct vkms_mst_emulator *emulator, u8 destination_port, struct drm_dp_aux_msg *msg)
{
	struct vkms_mst_emulator *other = emulator->ports[destination_port].to;
	u8 other_port_id = emulator->ports[destination_port].other_port_id;

	if (!other)
		return -ETIMEDOUT;
	if (!other->transfer_helpers)
		return -EPROTO;

	return other->transfer_helpers->transfer(other, other_port_id, msg);
}

static void vkms_mst_emulation_down_req_worker(struct work_struct *work)
{
	struct vkms_mst_emulator *emulator =
		container_of(work, struct vkms_mst_emulator, w_req);

	struct drm_dp_sideband_msg_req_body req = { 0 };
	struct drm_dp_sideband_msg_reply_body rep = { 0 };
	struct drm_dp_sideband_msg_hdr req_hdr = { 0 };
	struct drm_dp_sideband_msg_tx raw_rep = { 0 };
	u8 req_hdr_len = 0;

	bool success = drm_dp_decode_sideband_msg_hdr(
		NULL, &req_hdr, emulator->dpcd_memory.DOWN_REQ,
		sizeof(emulator->dpcd_memory.DOWN_REQ), &req_hdr_len);
	if (!success)
		return;

	// TODO: DPCD sideband request can be splitted, need to support this here

	if (req_hdr.broadcast) {
		if (req_hdr.lct != 1)
			pr_warn("Malformed header for a sideband broadcast message.");
	}

	if (!req_hdr.broadcast && req_hdr.lcr) {
		int new_req_hdr_len = 0;
		u8 dst = (req_hdr.rad[0] & 0xF0) >> 4;

		if (emulator->ports[dst].to) {
			rad_move_left(&req_hdr, emulator->work_current_src);
			req_hdr.lcr--;

			u8 buffer[sizeof(emulator->dpcd_memory.DOWN_REQ)];
			memcpy(&buffer, &emulator->dpcd_memory.DOWN_REQ,
			       sizeof(emulator->dpcd_memory.DOWN_REQ));
			drm_dp_encode_sideband_msg_hdr(&req_hdr, buffer, &new_req_hdr_len);

			struct drm_dp_aux_msg down_req;
			down_req.address = DP_SIDEBAND_MSG_DOWN_REQ_BASE;
			down_req.buffer = buffer;
			down_req.size = sizeof(buffer);
			down_req.request = DP_AUX_NATIVE_WRITE;

			vkms_mst_transfer(emulator, dst, &down_req);
		}
		return;
	}

	drm_dp_decode_sideband_req((void *)emulator->dpcd_memory.DOWN_REQ + req_hdr_len,
				   &req);

	rep.req_type = req.req_type;
	rep.reply_type = DP_SIDEBAND_REPLY_NAK;
	guid_copy(&rep.u.nak.guid, &emulator->dpcd_memory.GUID);
	rep.u.nak.reason = DP_NAK_BAD_PARAM;

	if (emulator->sideband_helpers) {
		switch (req.req_type) {
		case DP_CLEAR_PAYLOAD_ID_TABLE:
			if (emulator->sideband_helpers->clear_payload_id_table)
				emulator->sideband_helpers->clear_payload_id_table(emulator, emulator->work_current_src, &req_hdr, &req, &emulator->rep_to_send_header, &rep);
			break;
		case DP_LINK_ADDRESS:
			if (emulator->sideband_helpers->link_address)
				emulator->sideband_helpers->link_address(emulator, emulator->work_current_src, &req_hdr, &req, &emulator->rep_to_send_header, &rep);
			break;
		default:
			pr_err("Unsupported request %s, ignoring\n", drm_dp_mst_req_type_str(req.req_type));
			break;
		}
	}

	drm_dp_encode_sideband_reply(&rep, &raw_rep);

	emulator->rep_to_send_header.broadcast = req_hdr.broadcast;
	emulator->rep_to_send_header.somt = 1;
	emulator->rep_to_send_header.lcr = req_hdr.lct - 1;
	emulator->rep_to_send_header.lct = req_hdr.lct;
	memcpy(&emulator->rep_to_send_header.rad, &req_hdr.rad,
	       ARRAY_SIZE(req_hdr.rad));
	emulator->rep_to_send_header.seqno = req_hdr.seqno;
	emulator->rep_to_send_header.path_msg = req_hdr.path_msg;
	emulator->rep_to_send_header.msg_len = raw_rep.cur_len + 1;

	emulator->rep_to_send_content_len = raw_rep.cur_len;
	emulator->rep_to_send_content =
		kzalloc(emulator->rep_to_send_content_len, GFP_KERNEL);
	memcpy(emulator->rep_to_send_content, &raw_rep.msg,
	       emulator->rep_to_send_content_len);

	send_next_down_rep(emulator, emulator->work_current_src);
}

void vkms_mst_emulator_init(struct vkms_mst_emulator *emulator,
			    const struct vkms_mst_transfer_helpers *transfer_helpers,
			    const struct vkms_mst_sideband_helpers *sideband_helpers,
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
	emulator->sideband_helpers = sideband_helpers;
	emulator->name = kstrdup_const(name, GFP_KERNEL);
}

void vkms_mst_emulator_destroy(struct vkms_mst_emulator *emulator)
{
	kfree_const(emulator->name);
}

static int vkms_mst_emulator_port_count(struct vkms_mst_emulator *emulator)
{
	int total = 0;
	for (int i = 0; i < VKMS_MST_MAX_PORTS; i++) {
		if (emulator->ports[i].kind != VKMS_MST_PORT_NOT_EXISTS)
			total += 1;
	}
	return total;
}

void vkms_mst_emulator_link_address_default(struct vkms_mst_emulator *emulator, u8 port_id,
					    struct drm_dp_sideband_msg_hdr *req_hdr,
					    struct drm_dp_sideband_msg_req_body *req,
					    struct drm_dp_sideband_msg_hdr *rep_hdr,
					    struct drm_dp_sideband_msg_reply_body *rep)
{
	rep->req_type = DP_LINK_ADDRESS;
	rep->reply_type = DP_SIDEBAND_REPLY_ACK;
	guid_copy(&rep->u.link_addr.guid, &emulator->dpcd_memory.GUID);
	rep->u.link_addr.nports = vkms_mst_emulator_port_count(emulator);

	for (int i = 0; i < rep->u.link_addr.nports; i++) {
		rep->u.link_addr.ports[i].input_port = false;
		rep->u.link_addr.ports[i].port_number = i;

		if (emulator->ports[i].to) {
			u8 buf[16];
			struct drm_dp_aux_msg msg;

			msg.request = DP_AUX_NATIVE_READ;
			msg.address = DP_MSTM_CAP;
			msg.buffer = buf;
			msg.size = 1;
			vkms_mst_transfer(emulator, i, &msg);
			// TODO: Why this is not used?

			msg.address = DP_DPCD_REV;
			msg.size = 1;
			vkms_mst_transfer(emulator, i, &msg);
			rep->u.link_addr.ports[i].dpcd_revision = buf[0];

			msg.address = DP_GUID;
			msg.size = 16;
			vkms_mst_transfer(emulator, i, &msg);
			memcpy(&rep->u.link_addr.ports[i].peer_guid, msg.buffer, 16);

			// TODO: Need to request DFP those informations
			rep->u.link_addr.ports[i].num_sdp_streams = 0;
			rep->u.link_addr.ports[i].num_sdp_stream_sinks = 0;
			rep->u.link_addr.ports[i].legacy_device_plug_status = 1;

			rep->u.link_addr.ports[i].ddps = 1;
			if (emulator->ports[i].kind == VKMS_MST_PORT_UFP) {
				rep->u.link_addr.ports[i].input_port = true;
				rep->u.link_addr.ports[i].peer_device_type =
					DP_PEER_DEVICE_SOURCE_OR_SST;
				rep->u.link_addr.ports[i].port_number = 0;
				rep->u.link_addr.ports[i].mcs = 1;
			} else if (emulator->ports[i].kind == VKMS_MST_PORT_DFP) {
				u8 dfp_present = 0;
				u8 mstm_cap = 0;

				msg.address = DP_DOWNSTREAMPORT_PRESENT;
				msg.size = 1;
				vkms_mst_transfer(emulator, i, &msg);
				dfp_present = ((u8 *)msg.buffer)[0];


				msg.address = DP_MSTM_CAP;
				msg.size = 1;
				vkms_mst_transfer(emulator, i, &msg);
				mstm_cap = ((u8 *)msg.buffer)[0];

				if (dfp_present & DP_DWN_STRM_PORT_PRESENT) {
					switch (dfp_present & DP_DWN_STRM_PORT_TYPE_MASK) {
					case DP_DWN_STRM_PORT_TYPE_DP:
						if (mstm_cap & DP_MST_CAP) {
							rep->u.link_addr.ports[i].mcs = 1;
						} else {
							rep->u.link_addr.ports[i].mcs = 0;
						}
						rep->u.link_addr.ports[i].peer_device_type = DP_PEER_DEVICE_MST_BRANCHING;
						break;
					case DP_DWN_STRM_PORT_TYPE_ANALOG:
					case DP_DWN_STRM_PORT_TYPE_TMDS:
						rep->u.link_addr.ports[i].mcs = 0;
						rep->u.link_addr.ports[i].peer_device_type = DP_PEER_DEVICE_DP_LEGACY_CONV;
						break;
					case DP_DWN_STRM_PORT_TYPE_OTHER:
						rep->u.link_addr.ports[i].mcs = 1;
						rep->u.link_addr.ports[i].peer_device_type = DP_PEER_DEVICE_DP_WIRELESS_CONV;
						break;
					default:
						BUG();
					}
				} else if ((mstm_cap & DP_MST_CAP) == 0) {
					rep->u.link_addr.ports[i].mcs = 0;
					rep->u.link_addr.ports[i].peer_device_type = DP_PEER_DEVICE_SST_SINK;
				}
			}
		}
	}
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

		if (curr_offset >= DP_GUID && curr_offset <= DP_GUID + 0x10) {
			int msg_end =
				min(DP_SIDEBAND_MSG_DOWN_REP_BASE + 0x10, msg->address + msg->size);
			int msg_size = msg_end - curr_offset;

			export_guid(buffer, &emulator->dpcd_memory.GUID);

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
			emulator->work_current_src = port_id;

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
			if (*buffer & DP_DOWN_REP_MSG_RDY)
				send_next_down_rep(emulator, port_id);
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
