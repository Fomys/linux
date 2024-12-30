#ifndef _VKMS_MST_H_
#define _VKMS_MST_H_

#include <linux/types.h>
#include <drm/display/drm_dp_helper.h>
#include <drm/display/drm_dp_mst_helper.h>

#define VKMS_MST_MAX_PORTS 16

/**
 * struct vkms_dpcd_memory - Representation of the DPCD memory of a device
 * For exact meaning of each field, refer to the DisplayPort specification.
 */
struct vkms_dpcd_memory {
	u8 DPCD_REV;
	u8 MAX_LINK_RATE;
	u8 MAX_LANE_COUNT;
	u8 MAX_DOWNSPREAD;
	u8 NORP;
	u8 DOWNSTREAMPORT_PRESENT;
	u8 MAIN_LINK_CHANNEL_CODING;
	u8 DOWNSTREAM_PORT_COUNT;
	u8 RECEIVE_PORT_0_CAP_0;
	u8 RECEIVE_PORT_0_BUFFER_SIZE;
	u8 RECEIVE_PORT_1_CAP_0;
	u8 RECEIVE_PORT_1_BUFFER_SIZE;
	u8 I2C_SPEED_CAP;
	u8 EDP_CONFIGURATION_CAP;
	u8 TRAINING_AUX_RD_INTERVAL;
	u8 MSTM_CAP;
	u8 PAYLOAD_TABLE_UPDATE_STATUS;
	u8 MSTM_CTRL;
	u8 DOWN_REQ[0x200];
	u8 DOWN_REP[0x200];
	u8 PAYLOAD_ALLOCATE_SET;
	u8 PAYLOAD_ALLOCATE_START_TIME_SLOT;
	u8 PAYLOAD_ALLOCATE_TIME_SLOT_COUNT;
	guid_t GUID;
	u8 DEVICE_SERVICE_IRQ_VECTOR_ESI0;
	u8 SINK_COUNT_ESI;
	u8 LINK_SERVICE_IRQ_VECTOR_ESI0;
	u8 DEVICE_SERVICE_IRQ_VECTOR_ESI1;
};

struct vkms_mst_emulator;

struct vkms_mst_sideband_helpers {
	void (*link_address)(struct vkms_mst_emulator *emulator, u8 port_id,
			     struct drm_dp_sideband_msg_hdr *req_hdr,
			     struct drm_dp_sideband_msg_req_body *req,
			     struct drm_dp_sideband_msg_hdr *rep_hdr,
			     struct drm_dp_sideband_msg_reply_body *rep);
	void (*clear_payload_id_table)(struct vkms_mst_emulator *emulator, u8 port_id,
				       const struct drm_dp_sideband_msg_hdr *req_hdr,
				       const struct drm_dp_sideband_msg_req_body *req,
				       struct drm_dp_sideband_msg_hdr *rep_hdr,
				       struct drm_dp_sideband_msg_reply_body *rep);
	void (*enum_path_ressources)(struct vkms_mst_emulator *emulator, u8 port_id,
						         const struct drm_dp_sideband_msg_hdr *req_hdr,
						         const struct drm_dp_sideband_msg_req_body *req,
						         struct drm_dp_sideband_msg_hdr *rep_hdr,
						         struct drm_dp_sideband_msg_reply_body *rep);
	void (*remote_i2c_read)(struct vkms_mst_emulator *emulator, u8 port_id,
						    const struct drm_dp_sideband_msg_hdr *req_hdr,
						    const struct drm_dp_sideband_msg_req_body *req,
						    struct drm_dp_sideband_msg_hdr *rep_hdr,
						    struct drm_dp_sideband_msg_reply_body *rep);
};

/**
 * struct vkms_mst_transfer_helpers - Helpers to emulate a memory transfer over
 * dp-aux channel.
 *
 * You can find default implementation for those functions at vkms_mst.h.
 *
 * @transfer: Called when a transfer is received on @port_id.
 *	      Default implementation: vkms_mst_emulator_transfer_default
 * @transfer_read: If using vkms_mst_emulator_transfer_default,
 * 		   this is called for each native read request received
 *		   on @port_id.
 *		   Default implementation: vkms_mst_emulator_transfer_write_default
 * @transfer_write: If using vkms_mst_emulator_transfer_default,
 * 		    this is called for each native write request received
 *		    on @port_id.
 *		    Default implementation: vkms_mst_emulator_transfer_read_default
 * @transfer_i2c_write: If using vkms_mst_emulator_transfer_default,
 * 		        this is called for each i2c write request
 *		        received on @port_id.
 *		        Default implementation: vkms_mst_emulator_transfer_i2c_write_default
 * @transfer_i2c_read: If using vkms_mst_emulator_transfer_default,
 * 		       this is called for each i2c read request
 *		       received on @port_id.
 *		       Default implementation: vkms_mst_emulator_transfer_i2c_read_default
 */
struct vkms_mst_transfer_helpers {
	ssize_t (*transfer)(struct vkms_mst_emulator *emulator,
				   u8 port_id, struct drm_dp_aux_msg *msg);
	ssize_t (*transfer_read)(struct vkms_mst_emulator *emulator,
					u8 port_id, struct drm_dp_aux_msg *msg);
	ssize_t (*transfer_write)(struct vkms_mst_emulator *emulator,
					 u8 port_id, struct drm_dp_aux_msg *msg);
	ssize_t (*transfer_i2c_write)(struct vkms_mst_emulator *emulator,
					     u8 port_id,
					     struct drm_dp_aux_msg *msg);
	ssize_t (*transfer_i2c_read)(struct vkms_mst_emulator *emulator,
					    u8 port_id,
					    struct drm_dp_aux_msg *msg);
};

/**
 * struct vkms_mst_emulator_helpers - Helper to manage the emulator itself
 *
 * @destroy: Destroy function that must free all the memory allocated by the helper. It must call vkms_mst_emulator_destroy.
 */
struct vkms_mst_emulator_helpers {
	void (*destroy)(struct vkms_mst_emulator *emulator);
	void (*irq_handler)(struct vkms_mst_emulator *emulator, u8 port_id);
};

/**
 * enum vkms_mst_port_kind - The different kind of ports a MST device can have
 * @VKMS_MST_PORT_NOT_EXISTS: Default value, means that the device don't have a port
 * @VKMS_MST_PORT_UFP: Up facing port, for example a video input for a hub
 * @VKMS_MST_PORT_DFP: Down facing port, for example a video output for a hub
 */
enum vkms_mst_port_kind {
	VKMS_MST_PORT_NOT_EXISTS,
	VKMS_MST_PORT_UFP,
	VKMS_MST_PORT_DFP
};

/**
 * struct vkms_mst_emulator_port - Represents a connection between two devices
 * @kind: Kind of connection, initialized in vkms_mst_emulator_init
 * @to: Connected device, can be NULL.
 * @other_port_id: Required to identify on which port of the other device the
 *		   connection is done.
 */
struct vkms_mst_emulator_port {
	enum vkms_mst_port_kind kind;
	struct vkms_mst_emulator *to;
	u8 other_port_id;
};

/**
 * struct vkms_mst_emulator - Base structure for all MST device emulators.
 *
 * @dpcd_memory: Representation of the internal DPCD memory. This is a private
 *               field that should not be accessed outside the device itself.
 *               This memory is updated and read by the default implementation
 *               vkms_mst_emulator_transfer_read/write_default
 * @wq_req: Workqueue used when new requests are received on DOWN_REQ
 * @w_req: Work used for new request received on DOWN_REQ
 * @work_current_src: Used by @w_req to know where the request come from
 * @transfer_helpers: helpers called when a dp-aux transfer is requested
 * @rep_to_send_header: Header of the pending message that need to be send
 * @rep_to_send_content_len: Len of the sideband message to be send
 * @rep_to_send_content: Sideband message content to send
 * @transfer_helpers: Helpers called when a transfer request occurs
 * @ports: List of the ports and connected devices
 * @name: Name of the device. Mainly used for logging purpose.
 *
 * With the functions vkms_mst_emulator_connect and vkms_mst_emulator_disconnect,
 * you have the garantee that forall 0 <= i < VKMS_MST_MAX_PORTS:
 * 	ports[i].to = other device
 *	ports[i].to.ports[ports[i].other_port_id].to = this device
 *	ports[i].to.ports[ports[i].other_port_id].other_port_id = i
 */
struct vkms_mst_emulator {
	struct vkms_dpcd_memory dpcd_memory;

	// TODO: Transform this into a real queue, we can technically have
	//  multiple request at the same time from multiple devices.
	//  We may also need to change the dpcd_memory.DOWN_REQ/DOWN_REP
	//  behavior so it can work with multiple devices
	struct workqueue_struct *wq_req;
	struct work_struct w_req;
	u8 work_current_src;

	struct workqueue_struct *wq_irq;
	u8 irq_dst;

	struct drm_dp_sideband_msg_hdr rep_to_send_header;
	u8 rep_to_send_content_len;
	// TODO: Est ce que ça fait encore du jardinage
	u8 *rep_to_send_content;

	u8 *rep_pending_content;
	u8 rep_pending_content_len;
	struct drm_dp_sideband_msg_hdr rep_pending_header;

	const struct vkms_mst_transfer_helpers *transfer_helpers;
	const struct vkms_mst_sideband_helpers *sideband_helpers;
	const struct vkms_mst_emulator_helpers *helpers;

	struct vkms_mst_emulator_port ports[VKMS_MST_MAX_PORTS];
	const char *name;
};

void vkms_mst_call_irq(struct vkms_mst_emulator *emulator, u8 dst_port);
void send_next_down_rep(struct vkms_mst_emulator *emulator, u8 port_id);
ssize_t vkms_mst_transfer(struct vkms_mst_emulator *emulator, u8 destination_port, struct drm_dp_aux_msg *msg);

/**
 * vkms_mst_emulator_init - Initialize an MST emulator device
 * @emulator: Structure to initialize
 * @transfer_helpers: Helpers used to emulate dp-aux transfers. Can be NULL.
 * @sideband_helpers: Helpers used to emulate sideband transfers. Can be NULL.
 * @port_kinds: List of ports to configure on this device
 * @name: Name of the device. Used mainly for logging purpose.
 */
void vkms_mst_emulator_init(struct vkms_mst_emulator *emulator,
			    const struct vkms_mst_transfer_helpers *transfer_helpers,
			    const struct vkms_mst_sideband_helpers *sideband_helpers,
			    const struct vkms_mst_emulator_helpers *helpers,
			    const enum vkms_mst_port_kind port_kinds[VKMS_MST_MAX_PORTS],
			    const char *name);

/**
 * vkms_mst_emulator_connect - Connect two devices.
 * This function garantee that the field port is properly filled, i.e:
 * 	emulator_1.ports[emulator_1_port].to = emulator_2
 *	emulator_1.ports[emulator_1_port].other_port_id = emulator_2_port
 *	emulator_2.ports[emulator_2_port].to = emulator_1
 * 	emulator_2.ports[emulator_2_port].other_port_id = emulator_1_port
 */
int vkms_mst_emulator_connect(
	struct vkms_mst_emulator *emulator_1,
	unsigned int emulator_1_port,
	struct vkms_mst_emulator *emulator_2,
	unsigned int emulator_2_port);

/**
 * vkms_mst_emulator_disconnect - Disconnect two devices, ensuring that the
 * other end of the connexion is properly disconnected
 * @emulator: emulator to work on
 * @port: id of the port to be disconnected
 */
int vkms_mst_emulator_disconnect(struct vkms_mst_emulator *emulator, u8 port);

/**
 * vkms_mst_emulator_destroy: Destroy all resources allocated for an emulator
 * @emulator: Device to destroy
 */
void vkms_mst_emulator_destroy(struct vkms_mst_emulator *emulator);

/**
 * vkms_mst_emulator_transfer_default - Default implementation to handle a
 * dp-aux transfer.
 *
 * This implementation automatically call the correct callback in
 * @emulator->transfer_helper.
 */
ssize_t vkms_mst_emulator_transfer_default(struct vkms_mst_emulator *emulator,
					   u8 port_id, struct drm_dp_aux_msg *msg);

/**
 * vkms_mst_emulator_transfer_read_default - Default implementation to hand
 * native read on dp-aux
 * @emulator: Destination for the transfer
 * @port_id: Port which received the transfer
 * @msg: Requested transfer
 *
 * This implementation simply read the requested values from the correct
 * register.
 * Not all the registers are readable.
 */
ssize_t vkms_mst_emulator_transfer_read_default(struct vkms_mst_emulator *emulator, u8 port_id, struct drm_dp_aux_msg *msg);

/**
 * vkms_mst_emulator_transfer_write_default - Default implementation to hand
 * native write on dp-aux
 * @emulator: Destination for the transfer
 * @port_id: Port which received the transfer
 * @msg: Requested transfer
 *
 * This implementation simply write the requested values in the correct
 * register.
 * Not all the registers are writable.
 */
ssize_t vkms_mst_emulator_transfer_write_default(struct vkms_mst_emulator *emulator, u8 port_id, struct drm_dp_aux_msg *msg);

/**
 * vkms_mst_emulator_transfer_i2c_write_default - Default implementation to
 * handle i2c write over dp-aux
 * @emulator: Destination for the transfer
 * @port_id: Port which received the transfer
 * @msg: Requested transfer
 *
 * This implementation simply reply with a NACK.
 */
ssize_t vkms_mst_emulator_transfer_i2c_write_default(struct vkms_mst_emulator *emulator, u8 port_id, struct drm_dp_aux_msg *msg);

/**
 * vkms_mst_emulator_transfer_i2c_read_default - Default implementation to
 * handle i2c read over dp-aux
 * @emulator: Destination for the transfer
 * @port_id: Port which received the transfer
 * @msg: Requested transfer
 *
 * This implementation simply reply with a NACK.
 */
ssize_t vkms_mst_emulator_transfer_i2c_read_default(struct vkms_mst_emulator *emulator, u8 port_id, struct drm_dp_aux_msg *msg);

void vkms_mst_emulator_link_address_default(struct vkms_mst_emulator *emulator, u8 port_id,
				   struct drm_dp_sideband_msg_hdr *req_hdr,
				   struct drm_dp_sideband_msg_req_body *req,
				   struct drm_dp_sideband_msg_hdr *rep_hdr,
				   struct drm_dp_sideband_msg_reply_body *rep);

#endif
