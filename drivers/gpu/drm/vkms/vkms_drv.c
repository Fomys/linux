// SPDX-License-Identifier: GPL-2.0+

/**
 * DOC: vkms (Virtual Kernel Modesetting)
 *
 * VKMS is a software-only model of a KMS driver that is useful for testing
 * and for running X (or similar) on headless machines. VKMS aims to enable
 * a virtual display with no need of a hardware display capability, releasing
 * the GPU in DRM API tests.
 */

#include <linux/device.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>

#include <drm/drm_gem.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_drv.h>
#include <drm/drm_fbdev_shmem.h>
#include <drm/drm_file.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <drm/drm_ioctl.h>
#include <drm/drm_managed.h>
#include <drm/drm_gem_shmem_helper.h>
#include <drm/drm_vblank.h>

#include "vkms_device_drv.h"
#include "vkms_crtc.h"

#include <drm/drm_print.h>
#include <drm/drm_edid.h>

#include "vkms_device_drv.h"

#define DRIVER_NAME	"vkms"
#define DRIVER_DESC	"Virtual Kernel Mode Setting"
#define DRIVER_DATE	"20180514"
#define DRIVER_MAJOR	1
#define DRIVER_MINOR	0

static bool enable_cursor = true;
module_param_named(enable_cursor, enable_cursor, bool, 0444);
MODULE_PARM_DESC(enable_cursor, "Enable/Disable cursor support");

static bool enable_writeback = true;
module_param_named(enable_writeback, enable_writeback, bool, 0444);
MODULE_PARM_DESC(enable_writeback,
		 "Enable/Disable writeback connector support");

static bool enable_overlay;
module_param_named(enable_overlay, enable_overlay, bool, 0444);
MODULE_PARM_DESC(enable_overlay, "Enable/Disable overlay support");

DEFINE_DRM_GEM_FOPS(vkms_driver_fops);

static const struct drm_driver vkms_driver = {
	.driver_features	= DRIVER_MODESET | DRIVER_ATOMIC | DRIVER_GEM,
	.fops			= &vkms_driver_fops,
	DRM_GEM_SHMEM_DRIVER_OPS,

	.name			= DRIVER_NAME,
	.desc			= DRIVER_DESC,
	.date			= DRIVER_DATE,
	.major			= DRIVER_MAJOR,
	.minor			= DRIVER_MINOR,
};

struct vkms_platform_data default_platform_data;

static int vkms_platform_probe(struct platform_device *pdev)
{
	struct vkms_platform_data *pdata = dev_get_platdata(&pdev->dev);
	struct vkms_device *vkms_device;

	vkms_device = devm_drm_dev_alloc(&pdev->dev, &vkms_driver,
					 struct vkms_device, drm);

	if (IS_ERR(vkms_device))
		return PTR_ERR(vkms_device);

	int ret = vkms_configure_device(vkms_device, pdata->config);

	if (ret)
		return ret;

	platform_set_drvdata(pdev, vkms_device);

	return 0;
}

static void vkms_platform_remove(struct platform_device *pdev)
{
	struct vkms_device *vkms_dev = platform_get_drvdata(pdev);

	if (!vkms_dev)
		return;

	drm_dev_unregister(&vkms_dev->drm);
	drm_atomic_helper_shutdown(&vkms_dev->drm);
}

static struct platform_driver vkms_platform_driver = {
	.probe = vkms_platform_probe,
	.remove_new = vkms_platform_remove,
	.driver = {
		.name = DRIVER_NAME
	},
};

static int vkms_create_default_device(void)
{
	struct platform_device *pdev;
	struct vkms_config_plane *plane;
	struct vkms_config_encoder *encoder;
	struct vkms_config_crtc *crtc;
	default_platform_data.config = vkms_config_alloc();

	if (!default_platform_data.config)
		return -ENOMEM;

	crtc = vkms_config_create_crtc(default_platform_data.config);

	if (!crtc)
		goto err_alloc;
	crtc->name = kzalloc(sizeof("Main CRTC"), GFP_KERNEL);
	if (!crtc->name)
		goto err_alloc;
	sprintf(crtc->name, "Main CRTC");
	crtc->enable_writeback = enable_writeback;

	encoder = vkms_config_create_encoder(default_platform_data.config);
	if (!encoder)
		goto err_alloc;
	encoder->name = kzalloc(sizeof("Main Encoder"), GFP_KERNEL);
	if (!encoder->name)
		goto err_alloc;
	sprintf(encoder->name, "Main Encoder");

	if (vkms_config_encoder_attach_crtc(encoder, crtc))
		goto err_alloc;

	default_platform_data.config->writeback = enable_writeback;

	plane = vkms_config_create_plane(default_platform_data.config);
	if (!plane)
		goto err_alloc;

	plane->type = DRM_PLANE_TYPE_PRIMARY;
	plane->name = kzalloc(sizeof("primary"), GFP_KERNEL);
	sprintf(plane->name, "primary");

	if (vkms_config_plane_attach_crtc(plane, crtc))
		goto err_alloc;

	if (enable_overlay) {
		for (int i = 0; i < NUM_OVERLAY_PLANES; i++) {
			plane = vkms_config_create_plane(default_platform_data.config);
			if (!plane)
				goto err_alloc;
			plane->type = DRM_PLANE_TYPE_OVERLAY;
			plane->name = kzalloc(10, GFP_KERNEL);
			snprintf(plane->name, 10, "plane-%d", i);

			if (vkms_config_plane_attach_crtc(plane, crtc))
				goto err_alloc;
		}
	}
	if (enable_cursor) {
		plane = vkms_config_create_plane(default_platform_data.config);
		if (!plane)
			goto err_alloc;
		plane->type = DRM_PLANE_TYPE_CURSOR;
		plane->name = kzalloc(sizeof("cursor"), GFP_KERNEL);
		sprintf(plane->name, "cursor");

		if (vkms_config_plane_attach_crtc(plane, crtc))
			goto err_alloc;
	}

	pdev = vkms_create_device(&default_platform_data);
	if (IS_ERR(pdev))
		return PTR_ERR(pdev);

	return 0;

err_alloc:
	vkms_config_free(default_platform_data.config);
	return -ENOMEM;
}

static int __init vkms_platform_driver_init(void)
{
	int ret = platform_driver_register(&vkms_platform_driver);
	if (ret) {
		DRM_ERROR("Unable to register platform driver\n");
		return ret;
	}

	ret = vkms_create_default_device();
	if (ret) {
		DRM_ERROR("Unable to create default device\n");
		goto fail_device;
	}
	return 0;

fail_device:
	platform_driver_unregister(&vkms_platform_driver);
	return ret;
}

static void __exit vkms_platform_driver_exit(void)
{
	struct device *dev;

	while ((dev = platform_find_device_by_driver(NULL, &vkms_platform_driver.driver))) {
		// platform_find_device_by_driver increments the refcount. Drop
		// it so we don't leak memory.
		put_device(dev);
		platform_device_unregister(to_platform_device(dev));
	}

	platform_driver_unregister(&vkms_platform_driver);

	vkms_config_free(default_platform_data.config);
}

module_init(vkms_platform_driver_init);
module_exit(vkms_platform_driver_exit);

MODULE_AUTHOR("Haneen Mohammed <hamohammed.sa@gmail.com>");
MODULE_AUTHOR("Rodrigo Siqueira <rodrigosiqueiramelo@gmail.com>");
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");
