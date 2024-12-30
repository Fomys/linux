#include "vkms_mst_hub.h"
#include "drm/display/drm_dp_mst_helper.h"
#include "linux/gfp_types.h"
#include "linux/slab.h"
#include "vkms_mst.h"
#include <drm/drm_print.h>

static void vkms_mst_hub_emulator_init_memory(struct vkms_dpcd_memory *dpcd_memory)
{
	memset(dpcd_memory, 0, sizeof(*dpcd_memory));

	dpcd_memory->DPCD_REV = DP_DPCD_REV_14;
	dpcd_memory->MAX_LINK_RATE = DP_LINK_BW_1_62;
	dpcd_memory->MAX_LANE_COUNT = DP_MAX_LANE_COUNT_MASK;
	dpcd_memory->MAX_DOWNSPREAD = 0x00;
	dpcd_memory->NORP = 0x01;
	dpcd_memory->DOWNSTREAMPORT_PRESENT = DP_DWN_STRM_PORT_PRESENT;
	dpcd_memory->MAIN_LINK_CHANNEL_CODING = DP_CAP_ANSI_8B10B;
	dpcd_memory->RECEIVE_PORT_0_CAP_0 = 0;
	dpcd_memory->MSTM_CAP = DP_MST_CAP;
	guid_gen(&dpcd_memory->GUID);
}

static void interpret_recv(struct vkms_mst_emulator *emulator, u8 src_port)
{
	struct drm_dp_sideband_msg_hdr *rep_hdr = &emulator->rep_pending_header;
	u8 dst = (rep_hdr->rad[0] & 0xF0) >> 4; // TODO use this instead of only one callback

	int i = 0;

	for (; i < rep_hdr->lcr - 1; i++) {
		if (i % 2 == 0) {
			rep_hdr->rad[i / 2] <<= 4;
		} else {
			rep_hdr->rad[i / 2] &= 0xF0;
			rep_hdr->rad[i / 2] |= rep_hdr->rad[i / 2 + 1] >> 4;
		}
	}

	// TODO: Check if shift_rad_left works as expected
	if (i % 2 == 0) {
		rep_hdr->rad[i / 2] &= 0x0F;
		rep_hdr->rad[i / 2] |= src_port << 4;
	} else {
		rep_hdr->rad[i / 2] &= 0xF0;
		rep_hdr->rad[i / 2] |= src_port;
	}

	if (rep_hdr->lcr) {
		rep_hdr->lcr--;
		memcpy(&emulator->rep_to_send_header, rep_hdr, sizeof(*rep_hdr));
		emulator->rep_to_send_content = krealloc(emulator->rep_to_send_content, emulator->rep_pending_content_len, GFP_KERNEL);
		if (!emulator->rep_to_send_content)
			pr_err("PANIC\n");
		memcpy(emulator->rep_to_send_content, emulator->rep_pending_content, rep_hdr->msg_len);
		emulator->rep_to_send_content_len = rep_hdr->msg_len;

		send_next_down_rep(emulator, dst);
	} else {
		pr_err("STRANGE\n");
	}
}

struct vkms_mst_transfer_helpers vkms_mst_transfer_helpers_hub = {
	.transfer = vkms_mst_emulator_transfer_default,
	.transfer_read = vkms_mst_emulator_transfer_read_default,
	.transfer_write = vkms_mst_emulator_transfer_write_default,
};

static void vkms_mst_hub_clear_payload_id_table(struct vkms_mst_emulator *emulator,
						u8 port_id,
						const struct drm_dp_sideband_msg_hdr *req_hdr,
						const struct drm_dp_sideband_msg_req_body *req,
						struct drm_dp_sideband_msg_hdr *rep_hdr,
						struct drm_dp_sideband_msg_reply_body *rep)
{
	rep->req_type = DP_CLEAR_PAYLOAD_ID_TABLE;
	rep->reply_type = DP_SIDEBAND_REPLY_ACK;
}

static void vkms_mst_hub_enum_path_ressources(struct vkms_mst_emulator *emulator,
						  u8 port_id,
					      const struct drm_dp_sideband_msg_hdr *req_hdr,
					      const struct drm_dp_sideband_msg_req_body *req,
					      struct drm_dp_sideband_msg_hdr *rep_hdr,
					      struct drm_dp_sideband_msg_reply_body *rep)
{
	rep->reply_type = DP_SIDEBAND_REPLY_ACK;
	rep->req_type = DP_ENUM_PATH_RESOURCES;
	rep->u.path_resources.port_number = req->u.port_num.port_number;
	rep->u.path_resources.fec_capable = false;
	rep->u.path_resources.full_payload_bw_number = 1;
	rep->u.path_resources.avail_payload_bw_number = 1;
}

static void vkms_mst_hub_remote_i2c_read(struct vkms_mst_emulator *emulator, u8 port_id,
					 const struct drm_dp_sideband_msg_hdr *req_hdr,
					 const struct drm_dp_sideband_msg_req_body *req,
					 struct drm_dp_sideband_msg_hdr *rep_hdr,
					 struct drm_dp_sideband_msg_reply_body *rep)
{
	const struct drm_dp_remote_i2c_read *i2c_read = &req->u.i2c_read;
	if (!emulator->ports[i2c_read->port_number].to)
		return; // TODO: invalid i2c read


	for (int i = 0; i < i2c_read->num_transactions; i++) {
		struct drm_dp_aux_msg msg = { .address = i2c_read->transactions[i].i2c_dev_id,
					      .buffer = i2c_read->transactions[i].bytes,
					      .request = DP_AUX_I2C_WRITE,
					      .size = i2c_read->transactions[i].num_bytes };
		vkms_mst_transfer(emulator, i2c_read->port_number, &msg);
	}
	u8 *buffer = kzalloc(i2c_read->num_bytes_read * sizeof(*buffer), GFP_KERNEL);
	struct drm_dp_aux_msg msg = { .address = i2c_read->read_i2c_device_id,
				      .buffer = buffer,
				      .request = DP_AUX_I2C_READ,
				      .size = i2c_read->num_bytes_read };
	int ret = vkms_mst_transfer(emulator, i2c_read->port_number, &msg);

	rep->reply_type = DP_SIDEBAND_REPLY_ACK;
	rep->req_type = DP_REMOTE_I2C_READ;
	struct drm_dp_remote_i2c_read_ack_reply *ack = &rep->u.remote_i2c_read_ack;
	memcpy(&ack->bytes, buffer,
	       min(i2c_read->num_bytes_read * sizeof(*buffer), sizeof(ack->bytes)));
	kfree(buffer);
	ack->num_bytes = ret;
	ack->port_number = i2c_read->port_number;
}

const struct vkms_mst_sideband_helpers vkms_mst_hub_sideband_helpers = {
	.clear_payload_id_table = vkms_mst_hub_clear_payload_id_table,
	.link_address = vkms_mst_emulator_link_address_default,
	.enum_path_ressources = vkms_mst_hub_enum_path_ressources,
	.remote_i2c_read = vkms_mst_hub_remote_i2c_read
};

static void vkms_mst_hub_emulator_irq_handler(struct vkms_mst_emulator *emulator, u8 port_id)
{
	u8 esi;
	struct drm_dp_aux_msg esi_msg;
	esi_msg.request = DP_AUX_NATIVE_READ;
	esi_msg.buffer = &esi;
	esi_msg.address = DP_DEVICE_SERVICE_IRQ_VECTOR;
	esi_msg.size = sizeof(esi);

	vkms_mst_transfer(emulator, port_id, &esi_msg);

	if (esi & DP_DOWN_REP_MSG_RDY) {
		u8 *down_rep = kzalloc(sizeof(emulator->dpcd_memory.DOWN_REP), GFP_KERNEL);
		struct drm_dp_aux_msg down_rep_msg;

		struct drm_dp_sideband_msg_hdr req_hdr = { 0 };

		u8 req_hdr_len = 0;

		down_rep_msg.request = DP_AUX_NATIVE_READ;
		down_rep_msg.buffer = down_rep;
		down_rep_msg.address = DP_SIDEBAND_MSG_DOWN_REP_BASE;
		down_rep_msg.size =sizeof(emulator->dpcd_memory.DOWN_REP) ;
		vkms_mst_transfer(emulator, port_id, &down_rep_msg);

		drm_dp_decode_sideband_msg_hdr(NULL, &req_hdr, down_rep, sizeof(down_rep),
					       &req_hdr_len);

		if (req_hdr.somt) {
		        memcpy(&emulator->rep_pending_header, &req_hdr, sizeof(req_hdr));
			emulator->rep_pending_content_len = 0;
		}
		emulator->rep_pending_content = krealloc(emulator->rep_pending_content, req_hdr.msg_len + emulator->rep_pending_content_len, GFP_KERNEL);
		memcpy(emulator->rep_pending_content + emulator->rep_pending_content_len, &down_rep[req_hdr_len], req_hdr.msg_len);
		emulator->rep_pending_content_len += req_hdr.msg_len;


		if (req_hdr.eomt) {
		        interpret_recv(emulator, port_id);
		}

		u8 ack = DP_DOWN_REP_MSG_RDY;

		struct drm_dp_aux_msg ack_msg;
		ack_msg.request = DP_AUX_NATIVE_WRITE;
		ack_msg.buffer = &ack;
		ack_msg.address = DP_DEVICE_SERVICE_IRQ_VECTOR;
		ack_msg.size = sizeof(ack);
		vkms_mst_transfer(emulator, port_id, &ack_msg);
		kfree(down_rep);
	}
}


static const struct vkms_mst_emulator_helpers vkms_mst_hub_emulator_helpers = {
	.destroy = vkms_mst_emulator_destroy,
	.irq_handler = vkms_mst_hub_emulator_irq_handler,
};

struct vkms_mst_hub_emulator *vkms_mst_hub_emulator_alloc(unsigned int children_count, const char* name)
{
	struct vkms_mst_hub_emulator *emulator = kzalloc(sizeof(*emulator), GFP_KERNEL);
	if (emulator)
		vkms_mst_hub_emulator_init(emulator, children_count, name);
	return emulator;
}

void vkms_mst_hub_emulator_init(struct vkms_mst_hub_emulator *vkms_mst_hub_emulator,
				unsigned int children_count, const char *name)
{
	enum vkms_mst_port_kind port_kind[16] = { VKMS_MST_PORT_UFP,	VKMS_MST_PORT_NOT_EXISTS,
					      VKMS_MST_PORT_NOT_EXISTS, VKMS_MST_PORT_NOT_EXISTS,
					      VKMS_MST_PORT_NOT_EXISTS, VKMS_MST_PORT_NOT_EXISTS,
					      VKMS_MST_PORT_NOT_EXISTS, VKMS_MST_PORT_NOT_EXISTS,
					      VKMS_MST_PORT_NOT_EXISTS, VKMS_MST_PORT_NOT_EXISTS,
					      VKMS_MST_PORT_NOT_EXISTS, VKMS_MST_PORT_NOT_EXISTS,
					      VKMS_MST_PORT_NOT_EXISTS, VKMS_MST_PORT_NOT_EXISTS,
					      VKMS_MST_PORT_NOT_EXISTS, VKMS_MST_PORT_NOT_EXISTS };
	for (int i = 1; i <= children_count; i++) {
		port_kind[i] = VKMS_MST_PORT_DFP;
	}
	vkms_mst_emulator_init(&vkms_mst_hub_emulator->base,
			       &vkms_mst_transfer_helpers_hub, &vkms_mst_hub_sideband_helpers,
			       &vkms_mst_hub_emulator_helpers,
			       port_kind, name);
	vkms_mst_hub_emulator_init_memory(&vkms_mst_hub_emulator->base.dpcd_memory);
}