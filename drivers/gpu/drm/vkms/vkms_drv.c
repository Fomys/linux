// SPDX-License-Identifier: GPL-2.0+

/**
 * DOC: vkms (Virtual Kernel Modesetting)
 *
 * VKMS is a software-only model of a KMS driver that is useful for testing
 * and for running X (or similar) on headless machines. VKMS aims to enable
 * a virtual display with no need of a hardware display capability, releasing
 * the GPU in DRM API tests.
 */

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
#include <drm/drm_probe_helper.h>
#include <drm/drm_gem_shmem_helper.h>
#include <drm/drm_vblank.h>

#include "vkms_drv.h"
#include "vkms_crtc.h"

#include <drm/drm_print.h>
#include <drm/drm_debugfs.h>
#include <drm/drm_edid.h>

#define DRIVER_NAME	"vkms"
#define DRIVER_DESC	"Virtual Kernel Mode Setting"
#define DRIVER_DATE	"20180514"
#define DRIVER_MAJOR	1
#define DRIVER_MINOR	0

static struct vkms_config *default_config;

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

static void vkms_atomic_commit_tail(struct drm_atomic_state *old_state)
{
	struct drm_device *dev = old_state->dev;
	struct drm_crtc *crtc;
	struct drm_crtc_state *old_crtc_state;
	int i;

	drm_atomic_helper_commit_modeset_disables(dev, old_state);

	drm_atomic_helper_commit_planes(dev, old_state, 0);

	drm_atomic_helper_commit_modeset_enables(dev, old_state);

	drm_atomic_helper_fake_vblank(old_state);

	drm_atomic_helper_commit_hw_done(old_state);

	drm_atomic_helper_wait_for_flip_done(dev, old_state);

	for_each_old_crtc_in_state(old_state, crtc, old_crtc_state, i) {
		struct vkms_crtc_state *vkms_state =
			drm_crtc_state_to_vkms_crtc_state(old_crtc_state);

		flush_work(&vkms_state->composer_work);
	}

	drm_atomic_helper_cleanup_planes(dev, old_state);
}

static int vkms_config_show(struct seq_file *m, void *data)
{
	struct drm_debugfs_entry *entry = m->private;
	struct drm_device *dev = entry->dev;
	struct vkms_device *vkmsdev = drm_device_to_vkms_device(dev);

	seq_printf(m, "writeback=%d\n", enable_writeback);
	seq_printf(m, "cursor=%d\n", enable_cursor);
	seq_printf(m, "overlay=%d\n", enable_overlay);

	return 0;
}

static const struct drm_debugfs_info vkms_config_debugfs_list[] = {
	{ "vkms_config", vkms_config_show, 0 },
};

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

static int vkms_atomic_check(struct drm_device *dev, struct drm_atomic_state *state)
{
	struct drm_crtc *crtc;
	struct drm_crtc_state *new_crtc_state;
	int i;

	for_each_new_crtc_in_state(state, crtc, new_crtc_state, i) {
		if (!new_crtc_state->gamma_lut || !new_crtc_state->color_mgmt_changed)
			continue;

		if (new_crtc_state->gamma_lut->length / sizeof(struct drm_color_lut *)
		    > VKMS_LUT_SIZE)
			return -EINVAL;
	}

	return drm_atomic_helper_check(dev, state);
}

static const struct drm_mode_config_funcs vkms_mode_funcs = {
	.fb_create = drm_gem_fb_create,
	.atomic_check = vkms_atomic_check,
	.atomic_commit = drm_atomic_helper_commit,
};

static const struct drm_mode_config_helper_funcs vkms_mode_config_helpers = {
	.atomic_commit_tail = vkms_atomic_commit_tail,
};

static const struct drm_connector_funcs vkms_connector_funcs = {
	.fill_modes = drm_helper_probe_single_connector_modes,
	.reset = drm_atomic_helper_connector_reset,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
};

static const struct drm_encoder_funcs vkms_encoder_funcs = {};

static int vkms_conn_get_modes(struct drm_connector *connector)
{
	int count;

	/* Use the default modes list from drm */
	count = drm_add_modes_noedid(connector, XRES_MAX, YRES_MAX);
	drm_set_preferred_mode(connector, XRES_DEF, YRES_DEF);

	return count;
}

static const struct drm_connector_helper_funcs vkms_conn_helper_funcs = {
	.get_modes = vkms_conn_get_modes,
};

static int vkms_output_init(struct vkms_device *vkmsdev, int possible_crtc,
			    struct vkms_config *vkms_config)
{
	struct drm_device *dev = &vkmsdev->drm;
	struct drm_connector *connector;
	struct drm_encoder *encoder;
	struct vkms_crtc *crtc;
	struct drm_plane *plane;
	struct vkms_plane *primary, *cursor, *overlay = NULL;
	int ret;
	int writeback;
	unsigned int n;

	/*
	 * Initialize used plane. One primary plane is required to perform the composition.
	 *
	 * The overlay and cursor planes are not mandatory, but can be used to perform complex
	 * composition.
	 */
	primary = vkms_plane_init(vkmsdev, DRM_PLANE_TYPE_PRIMARY, possible_crtc);
	if (IS_ERR(primary))
		return PTR_ERR(primary);

	if (vkms_config->overlay) {
		for (n = 0; n < NUM_OVERLAY_PLANES; n++) {
			overlay = vkms_plane_init(vkmsdev, DRM_PLANE_TYPE_OVERLAY, possible_crtc);
			if (IS_ERR(overlay))
				return PTR_ERR(overlay);
		}
	}

	if (vkms_config->cursor) {
		cursor = vkms_plane_init(vkmsdev, DRM_PLANE_TYPE_CURSOR, possible_crtc);
		if (IS_ERR(cursor))
			return PTR_ERR(cursor);
	}

	/* [1]: Initialize the crtc component */
	crtc = vkms_crtc_init(vkmsdev, &primary->base,
			      cursor ? &cursor->base : NULL);
	if (IS_ERR(crtc))
		return PTR_ERR(crtc);

	/* Enable the output's CRTC for all the planes */
	drm_for_each_plane(plane, &vkmsdev->drm) {
		plane->possible_crtcs |= drm_crtc_mask(&crtc->base);
	}

	/* Initialize the connector component */
	connector = drmm_kzalloc(&vkmsdev->drm, sizeof(*connector), GFP_KERNEL);

	ret = drmm_connector_init(dev, connector, &vkms_connector_funcs,
				  DRM_MODE_CONNECTOR_VIRTUAL, NULL);
	if (ret) {
		DRM_ERROR("Failed to init connector\n");
		return ret;
	}

	drm_connector_helper_add(connector, &vkms_conn_helper_funcs);

	/* Initialize the encoder component */
	encoder = drmm_kzalloc(&vkmsdev->drm, sizeof(*encoder), GFP_KERNEL);

	ret = drmm_encoder_init(dev, encoder, &vkms_encoder_funcs,
				DRM_MODE_ENCODER_VIRTUAL, NULL);
	if (ret) {
		DRM_ERROR("Failed to init encoder\n");
		return ret;
	}

	encoder->possible_crtcs = drm_crtc_mask(&crtc->base);

	/* Attach the encoder and the connector */
	ret = drm_connector_attach_encoder(connector, encoder);
	if (ret) {
		DRM_ERROR("Failed to attach connector to encoder\n");
		return ret;
	}

	/* Initialize the writeback component */
	if (vkms_config->writeback) {
		writeback = vkms_enable_writeback_connector(vkmsdev, crtc);
		if (writeback)
			DRM_ERROR("Failed to init writeback connector\n");
	}

	drm_mode_config_reset(dev);

	return 0;
}

static int vkms_modeset_init(struct vkms_device *vkmsdev, struct vkms_config *vkms_config)
{
	struct drm_device *dev = &vkmsdev->drm;
	int ret;

	ret = drmm_mode_config_init(dev);
	if (ret)
		return ret;

	dev->mode_config.funcs = &vkms_mode_funcs;
	dev->mode_config.min_width = XRES_MIN;
	dev->mode_config.min_height = YRES_MIN;
	dev->mode_config.max_width = XRES_MAX;
	dev->mode_config.max_height = YRES_MAX;
	dev->mode_config.cursor_width = 512;
	dev->mode_config.cursor_height = 512;
	/*
	 * FIXME: There's a confusion between bpp and depth between this and
	 * fbdev helpers. We have to go with 0, meaning "pick the default",
	 * which is XRGB8888 in all cases.
	 */
	dev->mode_config.preferred_depth = 0;
	dev->mode_config.helper_private = &vkms_mode_config_helpers;

	return vkms_output_init(vkmsdev, 0, vkms_config);
}

static int vkms_create(struct vkms_config *config)
{
	int ret;
	struct platform_device *pdev;
	struct vkms_device *vkms_device;

	pdev = platform_device_register_simple(DRIVER_NAME, -1, NULL, 0);
	if (IS_ERR(pdev))
		return PTR_ERR(pdev);

	vkms_device = devm_drm_dev_alloc(&pdev->dev, &vkms_driver,
					 struct vkms_device, drm);
	if (IS_ERR(vkms_device)) {
		ret = PTR_ERR(vkms_device);
		goto out_unregister;
	}
	vkms_device->platform = pdev;
	config->dev = vkms_device;

	ret = dma_coerce_mask_and_coherent(vkms_device->drm.dev,
					   DMA_BIT_MASK(64));

	if (ret) {
		DRM_ERROR("Could not initialize DMA support\n");
		goto out_unregister;
	}

	ret = drm_vblank_init(&vkms_device->drm, 1);
	if (ret) {
		DRM_ERROR("Failed to vblank\n");
		goto out_unregister;
	}

	ret = vkms_modeset_init(vkms_device, config);
	if (ret)
		goto out_unregister;

	drm_debugfs_add_files(&vkms_device->drm, vkms_config_debugfs_list,
			      ARRAY_SIZE(vkms_config_debugfs_list));

	ret = drm_dev_register(&vkms_device->drm, 0);
	if (ret)
		goto out_unregister;

	drm_fbdev_shmem_setup(&vkms_device->drm, 0);

	return 0;

out_unregister:
	platform_device_unregister(pdev);
	return ret;
}

static int __init vkms_init(void)
{
	int ret;
	struct vkms_config *config;

	config = kmalloc(sizeof(*config), GFP_KERNEL);
	if (!config)
		return -ENOMEM;

	default_config = config;

	config->cursor = enable_cursor;
	config->writeback = enable_writeback;
	config->overlay = enable_overlay;

	ret = vkms_create(config);
	if (ret)
		kfree(config);

	return ret;
}

static void vkms_destroy(struct vkms_config *config)
{
	struct platform_device *pdev;

	if (!config->dev) {
		DRM_INFO("vkms_device is NULL.\n");
		return;
	}

	pdev = config->dev->platform;

	drm_dev_unregister(&config->dev->drm);
	drm_atomic_helper_shutdown(&config->dev->drm);
	platform_device_unregister(pdev);

	config->dev = NULL;
}

static void __exit vkms_exit(void)
{
	if (default_config->dev)
		vkms_destroy(default_config);

	kfree(default_config);
}

module_init(vkms_init);
module_exit(vkms_exit);

MODULE_AUTHOR("Haneen Mohammed <hamohammed.sa@gmail.com>");
MODULE_AUTHOR("Rodrigo Siqueira <rodrigosiqueiramelo@gmail.com>");
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");
