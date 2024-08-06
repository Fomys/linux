/* SPDX-License-Identifier: GPL-2.0+ */

#include <kunit/test.h>

#include "../vkms_device_drv.h"

static void vkms_config_test_basic_allocation(struct kunit *test)
{
	struct vkms_config *config = vkms_config_alloc();

	KUNIT_EXPECT_TRUE_MSG(test, list_empty(&config->encoders),
			      "Encoder list is not empty after allocation");
	KUNIT_EXPECT_TRUE_MSG(test, list_empty(&config->crtcs),
			      "CRTC list is not empty after allocation");
	KUNIT_EXPECT_TRUE_MSG(test, list_empty(&config->planes),
			      "Plane list is not empty after allocation");

	vkms_config_free(config);
}

static void vkms_config_test_simple_config(struct kunit *test)
{
	struct vkms_config *config = vkms_config_alloc();

	struct vkms_config_plane *plane_1 = vkms_config_create_plane(config);
	struct vkms_config_plane *plane_2 = vkms_config_create_plane(config);
	struct vkms_config_crtc *crtc = vkms_config_create_crtc(config);
	struct vkms_config_encoder *encoder = vkms_config_create_encoder(config);

	KUNIT_EXPECT_FALSE(test, vkms_config_is_valid(config));
	KUNIT_EXPECT_EQ(test, list_count_nodes(&config->planes), 2);
	KUNIT_EXPECT_EQ(test, list_count_nodes(&config->crtcs), 1);
	KUNIT_EXPECT_EQ(test, list_count_nodes(&config->encoders), 1);

	plane_1->type = DRM_PLANE_TYPE_PRIMARY;
	plane_2->type = DRM_PLANE_TYPE_CURSOR;

	KUNIT_EXPECT_EQ(test, vkms_config_plane_attach_crtc(plane_1, crtc), 0);
	KUNIT_EXPECT_EQ(test, vkms_config_plane_attach_crtc(plane_2, crtc), 0);
	KUNIT_EXPECT_EQ(test, vkms_config_encoder_attach_crtc(encoder, crtc), 0);

	KUNIT_EXPECT_TRUE(test, vkms_config_is_valid(config));

	vkms_config_delete_plane(plane_1, config);
	KUNIT_EXPECT_EQ(test, list_count_nodes(&config->planes), 1);
	KUNIT_EXPECT_EQ(test, list_count_nodes(&config->crtcs), 1);
	KUNIT_EXPECT_EQ(test, list_count_nodes(&config->encoders), 1);

	KUNIT_EXPECT_FALSE(test, vkms_config_is_valid(config));

	plane_2->type = DRM_PLANE_TYPE_PRIMARY;

	KUNIT_EXPECT_TRUE(test, vkms_config_is_valid(config));

	vkms_config_free(config);
}

static void vkms_config_test_complex_config(struct kunit *test)
{
	struct vkms_config *config = vkms_config_alloc();

	struct vkms_config_plane *plane_1 = vkms_config_create_plane(config);
	struct vkms_config_plane *plane_2 = vkms_config_create_plane(config);
	struct vkms_config_plane *plane_3 = vkms_config_create_plane(config);
	struct vkms_config_plane *plane_4 = vkms_config_create_plane(config);
	struct vkms_config_plane *plane_5 = vkms_config_create_plane(config);
	struct vkms_config_plane *plane_6 = vkms_config_create_plane(config);
	struct vkms_config_plane *plane_7 = vkms_config_create_plane(config);
	struct vkms_config_plane *plane_8 = vkms_config_create_plane(config);
	struct vkms_config_crtc *crtc_1 = vkms_config_create_crtc(config);
	struct vkms_config_crtc *crtc_2 = vkms_config_create_crtc(config);
	struct vkms_config_encoder *encoder_1 = vkms_config_create_encoder(config);
	struct vkms_config_encoder *encoder_2 = vkms_config_create_encoder(config);
	struct vkms_config_encoder *encoder_3 = vkms_config_create_encoder(config);
	struct vkms_config_encoder *encoder_4 = vkms_config_create_encoder(config);

	KUNIT_EXPECT_FALSE(test, vkms_config_is_valid(config));
	KUNIT_EXPECT_EQ(test, list_count_nodes(&config->planes), 8);
	KUNIT_EXPECT_EQ(test, list_count_nodes(&config->crtcs), 2);
	KUNIT_EXPECT_EQ(test, list_count_nodes(&config->encoders), 4);

	plane_1->type = DRM_PLANE_TYPE_PRIMARY;
	plane_2->type = DRM_PLANE_TYPE_CURSOR;
	plane_3->type = DRM_PLANE_TYPE_OVERLAY;
	plane_4->type = DRM_PLANE_TYPE_OVERLAY;
	plane_5->type = DRM_PLANE_TYPE_PRIMARY;
	plane_6->type = DRM_PLANE_TYPE_CURSOR;
	plane_7->type = DRM_PLANE_TYPE_OVERLAY;
	plane_8->type = DRM_PLANE_TYPE_OVERLAY;

	KUNIT_EXPECT_EQ(test, vkms_config_plane_attach_crtc(plane_1, crtc_1), 0);
	KUNIT_EXPECT_EQ(test, vkms_config_plane_attach_crtc(plane_2, crtc_1), 0);
	KUNIT_EXPECT_EQ(test, vkms_config_plane_attach_crtc(plane_3, crtc_1), 0);
	KUNIT_EXPECT_EQ(test, vkms_config_plane_attach_crtc(plane_4, crtc_1), 0);
	KUNIT_EXPECT_EQ(test, vkms_config_plane_attach_crtc(plane_5, crtc_2), 0);
	KUNIT_EXPECT_EQ(test, vkms_config_plane_attach_crtc(plane_6, crtc_2), 0);
	KUNIT_EXPECT_EQ(test, vkms_config_plane_attach_crtc(plane_7, crtc_2), 0);
	KUNIT_EXPECT_EQ(test, vkms_config_plane_attach_crtc(plane_8, crtc_2), 0);
	KUNIT_EXPECT_EQ(test, vkms_config_plane_attach_crtc(plane_3, crtc_2), 0);
	KUNIT_EXPECT_EQ(test, vkms_config_plane_attach_crtc(plane_4, crtc_2), 0);

	KUNIT_EXPECT_EQ(test, vkms_config_encoder_attach_crtc(encoder_1, crtc_1), 0);
	KUNIT_EXPECT_EQ(test, vkms_config_encoder_attach_crtc(encoder_2, crtc_1), 0);
	KUNIT_EXPECT_EQ(test, vkms_config_encoder_attach_crtc(encoder_3, crtc_1), 0);
	KUNIT_EXPECT_EQ(test, vkms_config_encoder_attach_crtc(encoder_3, crtc_2), 0);
	KUNIT_EXPECT_EQ(test, vkms_config_encoder_attach_crtc(encoder_4, crtc_2), 0);

	KUNIT_EXPECT_TRUE(test, vkms_config_is_valid(config));

	vkms_config_delete_plane(plane_4, config);
	KUNIT_EXPECT_EQ(test, list_count_nodes(&config->planes), 7);
	KUNIT_EXPECT_EQ(test, list_count_nodes(&config->crtcs), 2);
	KUNIT_EXPECT_EQ(test, list_count_nodes(&config->encoders), 4);

	KUNIT_EXPECT_TRUE(test, vkms_config_is_valid(config));

	vkms_config_free(config);
}

static struct kunit_case vkms_config_test_cases[] = {
	KUNIT_CASE(vkms_config_test_basic_allocation),
	KUNIT_CASE(vkms_config_test_simple_config),
	KUNIT_CASE(vkms_config_test_complex_config),
	{}
};

static struct kunit_suite vkms_config_test_suite = {
	.name = "vkms-config",
	.test_cases = vkms_config_test_cases,
};

kunit_test_suite(vkms_config_test_suite);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Kunit test for vkms config utility");
