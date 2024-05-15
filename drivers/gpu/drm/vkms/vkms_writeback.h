/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef _VKMS_WRITEBACK_H
#define _VKMS_WRITEBACK_H

#include "vkms_drv.h"
#include "vkms_formats.h"

struct vkms_crtc;

struct vkms_writeback_job {
	struct iosys_map data[DRM_FORMAT_MAX_PLANES];
	struct vkms_frame_info wb_frame_info;
	pixel_write_line_t pixel_write;
};

/* Writeback */
int vkms_enable_writeback_connector(struct vkms_device *vkmsdev);

#endif //_VKMS_WRITEBACK_H
