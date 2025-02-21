#include "vkms_mst_root.h"
#include "vkms_mst.h"
#include "vkms_connector.h"

static void vkms_mst_root_irq_handler(struct vkms_mst_emulator *emulator, u8 port_id)
{
	struct vkms_mst_emulator_root *emulator_root = container_of(emulator, struct vkms_mst_emulator_root, base);
	struct vkms_connector *connector = container_of(emulator_root, struct vkms_connector, vkms_mst_emulator_root);

	struct drm_dp_aux_msg esi_msg = { 0 };
	struct drm_dp_aux_msg ack_msg = { 0 };
	bool handled = true;

	while (handled) {
		u8 esi[DP_PSR_ERROR_STATUS - DP_SINK_COUNT_ESI] = { 0 };
		u8 ack[DP_PSR_ERROR_STATUS - DP_SINK_COUNT_ESI] = { 0 };

		esi_msg.address = DP_SINK_COUNT_ESI;
		esi_msg.buffer = esi;
		esi_msg.size = ARRAY_SIZE(esi);
		esi_msg.request = DP_AUX_NATIVE_READ;

		vkms_mst_emulator_root_transfer(emulator_root, &esi_msg);

		drm_dp_mst_hpd_irq_handle_event(&connector->mst_mgr, esi, ack, &handled);

		ack_msg.address = DP_SINK_COUNT_ESI;
		ack_msg.buffer = ack;
		ack_msg.size = ARRAY_SIZE(ack);
		ack_msg.request = DP_AUX_NATIVE_WRITE;
		vkms_mst_emulator_root_transfer(emulator_root, &ack_msg);
	}
	drm_dp_mst_hpd_irq_send_new_request(&connector->mst_mgr);
}

static const struct vkms_mst_emulator_helpers vkms_mst_root_helpers = {
	.destroy = vkms_mst_emulator_destroy,
	.irq_handler = vkms_mst_root_irq_handler
};

void vkms_mst_emulator_root_init(struct vkms_mst_emulator_root *emulator, const char *name)
{
	enum vkms_mst_port_kind port_kind[16] = {
		VKMS_MST_PORT_DFP, VKMS_MST_PORT_NOT_EXISTS,
		VKMS_MST_PORT_NOT_EXISTS, VKMS_MST_PORT_NOT_EXISTS,
		VKMS_MST_PORT_NOT_EXISTS, VKMS_MST_PORT_NOT_EXISTS,
		VKMS_MST_PORT_NOT_EXISTS, VKMS_MST_PORT_NOT_EXISTS,
		VKMS_MST_PORT_NOT_EXISTS, VKMS_MST_PORT_NOT_EXISTS,
		VKMS_MST_PORT_NOT_EXISTS, VKMS_MST_PORT_NOT_EXISTS,
		VKMS_MST_PORT_NOT_EXISTS, VKMS_MST_PORT_NOT_EXISTS,
		VKMS_MST_PORT_NOT_EXISTS, VKMS_MST_PORT_NOT_EXISTS
	};

	vkms_mst_emulator_init(&emulator->base, NULL, NULL, &vkms_mst_root_helpers, port_kind, name);

	emulator->base.dpcd_memory.DPCD_REV = 0x14;
}

ssize_t vkms_mst_emulator_root_transfer(struct vkms_mst_emulator_root *emulator, struct drm_dp_aux_msg *msg) {
	return vkms_mst_transfer(&emulator->base, 0, msg);
}