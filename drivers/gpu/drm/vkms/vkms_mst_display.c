#include "vkms_mst_display.h"

static void vkms_mst_display_clear_payload_id_table(struct vkms_mst_emulator *emulator, u8 port_id,
			       const struct drm_dp_sideband_msg_hdr *req_hdr,
			       const struct drm_dp_sideband_msg_req_body *req,
			       struct drm_dp_sideband_msg_hdr *rep_hdr,
			       struct drm_dp_sideband_msg_reply_body *rep)
{
	rep->req_type = DP_CLEAR_PAYLOAD_ID_TABLE;
	rep->reply_type = DP_SIDEBAND_REPLY_ACK;
}

const struct vkms_mst_sideband_helpers vkms_mst_display_sideband_helpers = {
	.clear_payload_id_table = vkms_mst_display_clear_payload_id_table,
	.link_address = vkms_mst_emulator_link_address_default
};

static ssize_t vkms_mst_display_transfer_i2c_write(struct vkms_mst_emulator *emulator,
						   u8 port_id,
						   struct drm_dp_aux_msg *msg)
{
	struct vkms_mst_display_emulator *display_emulator = container_of(emulator, struct vkms_mst_display_emulator, base);
	if ((msg->request & ~DP_AUX_I2C_MOT) != DP_AUX_I2C_WRITE) {
		pr_err("Wrong request type, the caller must only call this with DP_AUX_I2C_WRITE requests.\n");
		return 0;
	}

	if (msg->size == 1) {
		display_emulator->current_edid_offset = ((u8 *)msg->buffer)[0];
		msg->reply = DP_AUX_I2C_REPLY_ACK;
		return 1;
	}

	msg->reply = DP_AUX_I2C_REPLY_NACK;

	return 0;
}

static ssize_t vkms_mst_display_transfer_i2c_read(struct vkms_mst_emulator *emulator,u8 port_id, struct drm_dp_aux_msg *msg)
{
	struct vkms_mst_display_emulator *display_emulator = container_of(emulator, struct vkms_mst_display_emulator, base);

	if ((msg->request & ~DP_AUX_I2C_MOT) != DP_AUX_I2C_READ) {
		pr_err("Wrong request type, the caller must only call this with DP_AUX_I2C_READ requests.\n");
		return 0;
	}
	msg->reply = DP_AUX_I2C_REPLY_ACK;

	memcpy(msg->buffer,
	       &display_emulator->edid[display_emulator->current_edid_offset],
	       msg->size);
	display_emulator->current_edid_offset += msg->size;

	return msg->size;
}

static struct vkms_mst_transfer_helpers vkms_mst_transfer_helpers_display = {
	.transfer = vkms_mst_emulator_transfer_default,
	.transfer_read = vkms_mst_emulator_transfer_read_default,
	.transfer_write = vkms_mst_emulator_transfer_write_default,
	.transfer_i2c_write = vkms_mst_display_transfer_i2c_write,
	.transfer_i2c_read = vkms_mst_display_transfer_i2c_read,
};

static void vkms_mst_display_irq_hanlder(struct vkms_mst_emulator *emulator, u8 port_id)
{
	pr_err("not implemented");
}

const struct vkms_mst_emulator_helpers vkms_mst_display_helpers = {
	.destroy = vkms_mst_emulator_destroy,
	.irq_handler = vkms_mst_display_irq_hanlder
};

void vkms_mst_display_emulator_init(struct vkms_mst_display_emulator *emulator, const char* name)
{
	enum vkms_mst_port_kind port_kind[16] = {
		VKMS_MST_PORT_UFP, VKMS_MST_PORT_NOT_EXISTS,
		VKMS_MST_PORT_NOT_EXISTS, VKMS_MST_PORT_NOT_EXISTS,
		VKMS_MST_PORT_NOT_EXISTS, VKMS_MST_PORT_NOT_EXISTS,
		VKMS_MST_PORT_NOT_EXISTS, VKMS_MST_PORT_NOT_EXISTS,
		VKMS_MST_PORT_NOT_EXISTS, VKMS_MST_PORT_NOT_EXISTS,
		VKMS_MST_PORT_NOT_EXISTS, VKMS_MST_PORT_NOT_EXISTS,
		VKMS_MST_PORT_NOT_EXISTS, VKMS_MST_PORT_NOT_EXISTS,
		VKMS_MST_PORT_NOT_EXISTS, VKMS_MST_PORT_NOT_EXISTS
	};
	vkms_mst_emulator_init(&emulator->base,
			       &vkms_mst_transfer_helpers_display,
			       &vkms_mst_display_sideband_helpers,
			       &vkms_mst_display_helpers,
			       port_kind, name);
	const u8 edid[256] = {
		0,   255, 255, 255, 255, 255, 255, 0,	5,  227, 1,   0,  199,
		4,   0,	  0,   38,  25,	 1,   3,   128, 53, 30,	 120, 42, 246,
		229, 167, 83,  77,  153, 36,  20,  80,	84, 191, 239, 0,  209,
		192, 179, 0,   149, 0,	 129, 128, 129, 64, 129, 192, 1,  1,
		1,   1,	  2,   58,  128, 24,  113, 56,	45, 64,	 88,  44, 69,
		0,   19,  43,  33,  0,	 0,   30,  0,	0,  0,	 253, 0,  50,
		76,  30,  83,  17,  0,	 10,  32,  32,	32, 32,	 32,  32, 0,
		0,   0,	  252, 0,   50,	 52,  54,  48,	71, 53,	 10,  32, 32,
		32,  32,  32,  32,  0,	 0,   0,   255, 0,  70,	 48,  55, 70,
		57,  66,  65,  48,  48,	 49,  50,  50,	51, 1,	 153, 2,  3,
		30,  241, 75,  16,  31,	 5,   20,  4,	19, 3,	 18,  2,  17,
		1,   35,  9,   7,   7,	 131, 1,   0,	0,  101, 3,   12, 0,
		16,  0,	  140, 10,  208, 138, 32,  224, 45, 16,	 16,  62, 150,
		0,   19,  43,  33,  0,	 0,   24,  1,	29, 0,	 114, 81, 208,
		30,  32,  110, 40,  85,	 0,   19,  43,	33, 0,	 0,   30, 140,
		10,  208, 138, 32,  224, 45,  16,  16,	62, 150, 0,   19, 43,
		33,  0,	  0,   24,  140, 10,  208, 144, 32, 64,	 49,  32, 12,
		64,  85,  0,   19,  43,	 33,  0,   0,	24, 0,	 0,   0,  0,
		0,   0,	  0,   0,   0,	 0,   0,   0,	0,  0,	 0,   0,  0,
		0,   0,	  0,   0,   0,	 0,   0,   0,	177
	};
	memcpy(&emulator->edid, &edid, sizeof(edid));
	emulator->current_edid_offset = 0;

	emulator->base.dpcd_memory.MAX_LINK_RATE = 0x06;
	emulator->base.dpcd_memory.MAX_LANE_COUNT = 0x01;
	emulator->base.dpcd_memory.RECEIVE_PORT_0_CAP_0 = DP_LOCAL_EDID_PRESENT;
	emulator->base.dpcd_memory.DOWNSTREAMPORT_PRESENT = DP_DWN_STRM_PORT_TYPE_DP;
}
