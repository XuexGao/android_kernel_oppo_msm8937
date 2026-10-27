// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung EA8064 (EA8064T) 1080x1920 AMOLED panel, CMD mode, as fitted to the
 * OPPO R9s (msm8953, board id 16017/16027).
 *
 * Everything here is transcribed from the downstream vendor tree:
 *   arch/arm64/boot/dts/qcom/oppo-msm8953/16017/
 *       dsi-panel-oppo16017samsung_ea8064_1080p_cmd.dtsi
 * The vendor packs its init stream as, per DCS packet:
 *   [0] packet type  0x05 = DCS short write, no parameter
 *                    0x15 = DCS short write, one parameter
 *                    0x39 = generic long write
 *   [1] flags  [2] virtual channel  [3] stream
 *   [4..5] delay in ms, little endian
 *   [6] payload length, followed by that many payload bytes
 * Only type/delay/payload matter for a mainline driver and are what is kept
 * below; the byte values are deliberately left as-is, so do not "clean up" the
 * F0/C3/B0 page-select dance - it is what the controller expects.
 *
 * Bring-up shortcut: brightness is fixed by the 0x51/0x53 values in the init
 * stream instead of being exported as a backlight device, so that there is
 * something on the screen while the rest of the display path gets debugged.
 */

#include <drm/drm_connector.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/regulator/consumer.h>

#include <video/mipi_display.h>

enum ea8064_pkt {
	PKT_DCS0,	/* DCS short write, no args  (vendor type 0x05) */
	PKT_DCS1,	/* DCS short write, 1 param  (vendor type 0x15) */
	PKT_GEN,	/* generic long write        (vendor type 0x39) */
};

struct ea8064_seq {
	enum ea8064_pkt pkt;
	u16 delay_ms;
	u8 len;
	u8 data[22];
};

static const struct ea8064_seq ea8064_on_seq[] = {
	{ PKT_DCS0, 120, 1, { MIPI_DCS_EXIT_SLEEP_MODE } },
	{ PKT_GEN,    1, 3, { 0xF0, 0x5A, 0x5A } },
	{ PKT_GEN,    1, 2, { 0xC3, 0xA0 } },
	{ PKT_GEN,    1, 2, { 0xB0, 0x01 } },
	{ PKT_GEN,    1, 2, { 0xC3, 0x49 } },
	{ PKT_GEN,    1, 2, { 0xB0, 0x0D } },
	{ PKT_GEN,    1, 2, { 0xC3, 0x14 } },
	{ PKT_GEN,    1, 2, { 0xB0, 0x0E } },
	{ PKT_GEN,    1, 2, { 0xC3, 0x25 } },
	{ PKT_GEN,    1, 2, { 0xB0, 0x18 } },
	{ PKT_GEN,    1, 2, { 0xC3, 0x00 } },
	{ PKT_GEN,    1, 2, { 0xB0, 0x19 } },
	{ PKT_GEN,    1, 22, { 0xC3, 0xDE, 0x00, 0x00, 0x16, 0xFF, 0x00, 0x00,
			       0x00, 0xFF, 0x00, 0xFF, 0xFF, 0xFF, 0x00, 0xFF,
			       0xF2, 0xFB, 0x04, 0xFF, 0xFF, 0xFF } },
	{ PKT_GEN,    1, 3, { 0xF0, 0xA5, 0xA5 } },
	{ PKT_DCS1,   1, 2, { MIPI_DCS_SET_TEAR_ON, 0x00 } },
	/* 0x53 = WCTD (write control display), the vendor's "53 20" */
	{ PKT_DCS1,   1, 2, { MIPI_DCS_WRITE_CONTROL_DISPLAY, 0x20 } },
	/* 0x51 = SDB (set display brightness). The vendor stream sends 0x00 and
	 * lets the display driver raise it later; a fixed mid value here keeps a
	 * working init observable, see the note at the top of the file. */
	{ PKT_DCS1,  10, 2, { MIPI_DCS_SET_DISPLAY_BRIGHTNESS, 0x80 } },
	{ PKT_DCS0,   1, 1, { MIPI_DCS_SET_DISPLAY_ON } },
};

static const struct ea8064_seq ea8064_off_seq[] = {
	{ PKT_DCS0,  40, 1, { MIPI_DCS_SET_DISPLAY_OFF } },
	{ PKT_DCS0, 120, 1, { MIPI_DCS_ENTER_SLEEP_MODE } },
};

struct ea8064 {
	struct drm_panel panel;
	struct mipi_dsi_device *dsi;
	struct regulator *vci;
	struct regulator *avdd;
	struct regulator *vneg;
	struct gpio_desc *reset;
	bool prepared;
};

static inline struct ea8064 *to_ea8064(struct drm_panel *panel)
{
	return container_of(panel, struct ea8064, panel);
}

static int ea8064_write(struct ea8064 *ctx, const struct ea8064_seq *s)
{
	struct mipi_dsi_device *dsi = ctx->dsi;
	ssize_t ret;

	switch (s->pkt) {
	case PKT_DCS0:
		ret = mipi_dsi_dcs_write(dsi, s->data[0], NULL, 0);
		break;
	case PKT_DCS1:
		ret = mipi_dsi_dcs_write(dsi, s->data[0], &s->data[1], 1);
		break;
	case PKT_GEN:
		ret = mipi_dsi_generic_write(dsi, s->data, s->len);
		break;
	default:
		return -EINVAL;
	}

	if (ret < 0) {
		dev_err(&dsi->dev, "panel write failed: %zd\n", ret);
		return ret;
	}

	if (s->delay_ms)
		msleep(s->delay_ms);

	return 0;
}

static int ea8064_power_on(struct ea8064 *ctx)
{
	int ret;

	ret = regulator_enable(ctx->avdd);
	if (ret)
		return ret;

	ret = regulator_enable(ctx->vci);
	if (ret)
		goto err_avdd;

	/* VNEG(ibb) 是可选的负压轨，FIXME(unverified)：dts 给了 vneg-supply 才使能，
	 * 给不到时行为与之前完全一致，避免无证据接入负压轨伤屏。
	 * 上电顺序：正压(avdd)/逻辑(vci) 先稳，再上 vneg。 */
	if (ctx->vneg) {
		ret = regulator_enable(ctx->vneg);
		if (ret)
			goto err_vci;
	}

	/* Vendor reset-sequence <1 5>, <0 2>, <1 12>: hold reset, release, wait */
	gpiod_set_value_cansleep(ctx->reset, 1);
	usleep_range(5000, 6000);
	gpiod_set_value_cansleep(ctx->reset, 0);
	msleep(20);

	return 0;

err_vci:
	regulator_disable(ctx->vci);
err_avdd:
	regulator_disable(ctx->avdd);
	return ret;
}

static int ea8064_power_off(struct ea8064 *ctx)
{
	gpiod_set_value_cansleep(ctx->reset, 1);
	usleep_range(5000, 6000);

	if (ctx->vneg)
		regulator_disable(ctx->vneg);
	regulator_disable(ctx->vci);
	regulator_disable(ctx->avdd);

	return 0;
}

static int ea8064_prepare(struct drm_panel *panel)
{
	struct ea8064 *ctx = to_ea8064(panel);
	unsigned int i;
	int ret;

	if (ctx->prepared)
		return 0;

	ret = ea8064_power_on(ctx);
	if (ret < 0)
		return ret;

	/* qcom,mdss-dsi-lp11-init: the host is already in LP-11 after reset */
	for (i = 0; i < ARRAY_SIZE(ea8064_on_seq); i++) {
		ret = ea8064_write(ctx, &ea8064_on_seq[i]);
		if (ret < 0) {
			ea8064_power_off(ctx);
			return ret;
		}
	}

	ctx->prepared = true;
	return 0;
}

static int ea8064_unprepare(struct drm_panel *panel)
{
	struct ea8064 *ctx = to_ea8064(panel);
	unsigned int i;

	if (!ctx->prepared)
		return 0;

	for (i = 0; i < ARRAY_SIZE(ea8064_off_seq); i++)
		ea8064_write(ctx, &ea8064_off_seq[i]);

	ea8064_power_off(ctx);
	ctx->prepared = false;

	return 0;
}

/*
 * CMD-mode panel with an internal RAM refresh: the init stream already ends in
 * Display On, so enable/disable only exist to keep the panel state machine
 * honest.
 */
static int ea8064_enable(struct drm_panel *panel)
{
	return 0;
}

static int ea8064_disable(struct drm_panel *panel)
{
	return 0;
}

static const struct drm_display_mode ea8064_mode = {
	/* Porches from qcom,mdss-dsi-{h,v}-*-porch / -pulse-width at 60 Hz */
	.clock = 150111,
	.hdisplay = 1080,
	.hsync_start = 1080 + 118,
	.hsync_end = 1080 + 118 + 16,
	.htotal = 1080 + 118 + 16 + 70,
	.vdisplay = 1920,
	.vsync_start = 1920 + 20,
	.vsync_end = 1920 + 20 + 2,
	.vtotal = 1920 + 20 + 2 + 4,
	.width_mm = 74,
	.height_mm = 132,
	.type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED,
};

static int ea8064_get_modes(struct drm_panel *panel,
			    struct drm_connector *connector)
{
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(connector->dev, &ea8064_mode);
	if (!mode)
		return -ENOMEM;

	drm_mode_set_name(mode);
	drm_mode_probed_add(connector, mode);

	connector->display_info.bpc = 8;
	connector->display_info.width_mm = 74;
	connector->display_info.height_mm = 132;

	return 1;
}

static const struct drm_panel_funcs ea8064_funcs = {
	.prepare = ea8064_prepare,
	.unprepare = ea8064_unprepare,
	.enable = ea8064_enable,
	.disable = ea8064_disable,
	.get_modes = ea8064_get_modes,
};

static int ea8064_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct ea8064 *ctx;
	int ret;

	ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->dsi = dsi;
	mipi_dsi_set_drvdata(dsi, ctx);

	ctx->vci = devm_regulator_get(dev, "vci");
	if (IS_ERR(ctx->vci))
		return dev_err_probe(dev, PTR_ERR(ctx->vci), "Failed to get VCI\n");

	ctx->avdd = devm_regulator_get(dev, "avdd");
	if (IS_ERR(ctx->avdd))
		return dev_err_probe(dev, PTR_ERR(ctx->avdd), "Failed to get AVDD\n");

	/* VNEG(ibb) 对 AMOLED 是负压轨，未确认前做成可选：dts 不接 vneg-supply 就跳过，
	 * 避免无证据接入负压轨伤屏（详见 README Blocker 1 / M3）。 */
	ctx->vneg = devm_regulator_get_optional(dev, "vneg");
	if (IS_ERR(ctx->vneg)) {
		if (PTR_ERR(ctx->vneg) == -ENODEV)
			ctx->vneg = NULL;
		else
			return dev_err_probe(dev, PTR_ERR(ctx->vneg), "Failed to get VNEG\n");
	}

	ctx->reset = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset))
		return dev_err_probe(dev, PTR_ERR(ctx->reset), "Failed to get reset GPIO\n");

	/* 4 lanes, RGB888, CMD mode (no MIPI_DSI_MODE_VIDEO), burst */
	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_NO_EOT_PACKET | MIPI_DSI_CLOCK_NON_CONTINUOUS |
			  MIPI_DSI_MODE_LPM;

	drm_panel_init(&ctx->panel, dev, &ea8064_funcs, DRM_MODE_CONNECTOR_DSI);
	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0) {
		dev_err_probe(dev, ret, "Failed to attach to DSI host\n");
		drm_panel_remove(&ctx->panel);
	}

	return ret;
}

static void ea8064_remove(struct mipi_dsi_device *dsi)
{
	struct ea8064 *ctx = mipi_dsi_get_drvdata(dsi);

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);
}

static const struct of_device_id ea8064_of_match[] = {
	{ .compatible = "oppo,ea8064-cmd" },
	{ }
};
MODULE_DEVICE_TABLE(of, ea8064_of_match);

static struct mipi_dsi_driver ea8064_driver = {
	.probe = ea8064_probe,
	.remove = ea8064_remove,
	.driver = {
		.name = "panel-oppo-ea8064",
		.of_match_table = ea8064_of_match,
	},
};
module_mipi_dsi_driver(ea8064_driver);

MODULE_DESCRIPTION("Samsung EA8064 AMOLED panel driver for OPPO R9s");
MODULE_LICENSE("GPL");
