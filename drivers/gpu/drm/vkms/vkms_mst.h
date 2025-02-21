#ifndef _VKMS_MST_H_
#define _VKMS_MST_H_

#include <linux/types.h>
#include <drm/display/drm_dp_helper.h>

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
};

struct vkms_mst_emulator;

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
 * struct vkms_mst_emulator - Base structure for all MST device emulators.
 *
 * @dpcd_memory: Representation of the internal DPCD memory. This is a private
 *               field that should not be accessed outside the device itself.
 *               This memory is updated and read by the default implementation
 *               vkms_mst_emulator_transfer_read/write_default
 * @transfer_helpers: helpers called when a dp-aux transfer is requested
 * @name: Name of the device. Mainly used for logging purpose.
 */
struct vkms_mst_emulator {
	struct vkms_dpcd_memory dpcd_memory;

	const struct vkms_mst_transfer_helpers *transfer_helpers;

	const char *name;
};

/**
 * vkms_mst_emulator_init - Initialize an MST emulator device
 * @emulator: Structure to initialize
 * @transfer_helpers: Helpers used to emulate dp-aux transfers. Can be NULL.
 * @name: Name of the device. Used mainly for logging purpose.
 */
void vkms_mst_emulator_init(struct vkms_mst_emulator *emulator,
			    const struct vkms_mst_transfer_helpers *transfer_helpers,
			    const char *name);

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

#endif
