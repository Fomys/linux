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

static struct vkms_mst_transfer_helpers vkms_mst_transfer_helpers_display = {
	.transfer = vkms_mst_emulator_transfer_default,
	.transfer_read = vkms_mst_emulator_transfer_read_default,
	.transfer_write = vkms_mst_emulator_transfer_write_default,
};

const struct vkms_mst_sideband_helpers vkms_mst_display_sideband_helpers = {
	.clear_payload_id_table = vkms_mst_display_clear_payload_id_table,
	.link_address = vkms_mst_emulator_link_address_default
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

	emulator->base.dpcd_memory.MAX_LINK_RATE = 0x06;
	emulator->base.dpcd_memory.MAX_LANE_COUNT = 0x01;
	emulator->base.dpcd_memory.RECEIVE_PORT_0_CAP_0 = DP_LOCAL_EDID_PRESENT;
	emulator->base.dpcd_memory.DOWNSTREAMPORT_PRESENT = DP_DWN_STRM_PORT_TYPE_DP;
}
