// SPDX-License-Identifier: GPL-2.0
/*
 * JDI R63452 1080x1920 command-mode display panel, as fitted to the OPPO R9s
 * (msm8953, board id 16027). Transcribed from the downstream vendor tree:
 *   arch/arm64/boot/dts/qcom/oppo-msm8953/16027/
 *       dsi-panel-oppo16027jdi_r63452_1080p_cmd.dtsi
 *
 * The panel is a JDI command-mode TFT/LCD (the pmi8950 LAB/IBB are driven in
 * "lcd" mode for it; there is no docked AMOLED register dance). Contrast with
 * the 16017 variant which is a Samsung EA8064 AMOLED and has its own driver.
 *
 * Command format is the standard QCOM header: <type> <flags> <vc> <stream>
 * <delay_ms lo> <delay_ms hi> <count> <payload...>.
 *   type 0x05 = DCS short write (no param); 0x15 = DCS short write +1 param;
 *   0x39 = generic long write. Only type/delay/payload are kept here; the
 *   byte values are left exactly as in the vendor dtsi.
 *
 * Bring-up notes:
 *  - The on/off sequences set display brightness/contrast themselves; the
 *    LM3697 backlight controller (i2c_2 0x36) is NOT wired yet, so a working
 *    panel will still look dark until that is added. See README.
 *  - vdd / vddio supplies: the downstream board never binds mdss_dsi0 to
 *    explicit PMIC regulators for these (it uses a name-based panel-supply
 *    list). We keep them optional; the real rails are FIXME(unverified).
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

enum r63452_pkt {
	PKT_DCS0,	/* DCS short write, no args  (vendor type 0x05) */
	PKT_DCS1,	/* DCS short write, 1 param  (vendor type 0x15) */
	PKT_GEN,	/* generic long write        (vendor type 0x39) */
};

struct r63452_seq {
	enum r63452_pkt pkt;
	u16 delay_ms;
	u8 len;
	u8 data[26];
};

/* Transcribed from dsi-panel-oppo16027jdi_r63452_1080p_cmd.dtsi's
 * qcom,mdss-dsi-on-command. All are 0x39 (long) writes; keep byte-for-byte. */
static const struct r63452_seq r63452_on_seq[] = {
	{ PKT_GEN,   0,  2, { 0x35, 0x00 } },
	{ PKT_GEN,   0,  2, { 0xB0, 0x00 } },
	{ PKT_GEN,   0,  4, { 0xE9, 0x40, 0x00, 0x00 } },
	{ PKT_GEN,   0,  2, { 0xD6, 0x01 } },
	{ PKT_GEN,   0,  2, { 0x51, 0xFF } },
	{ PKT_GEN,   0,  2, { 0x53, 0x24 } },
	{ PKT_GEN,   0,  2, { 0x55, 0x02 } },
	{ PKT_GEN,   0,  2, { 0x5E, 0x00 } },
	{ PKT_GEN,   0,  8, { 0xB9, 0x8F, 0x4D, 0x13, 0x20, 0x02, 0x30, 0x30 } },
	{ PKT_GEN,   0, 26, { 0xCE, 0x11, 0x40, 0x49, 0x53, 0x59, 0x5E, 0x63,
			      0x68, 0x6E, 0x74, 0x7E, 0x8A, 0x98, 0xA8, 0xBB,
			      0xD0, 0xFF, 0x04, 0x00, 0x04, 0x04, 0x42, 0x00,
			      0x69, 0x5A } },
	{ PKT_GEN, 120,  2, { MIPI_DCS_EXIT_SLEEP_MODE, 0x00 } },
	{ PKT_GEN,  20,  2, { MIPI_DCS_SET_DISPLAY_ON, 0x00 } },
};

static const struct r63452_seq r63452_off_seq[] = {
	{ PKT_DCS0,  20, 1, { MIPI_DCS_SET_DISPLAY_OFF } },
	{ PKT_DCS0, 120, 1, { MIPI_DCS_ENTER_SLEEP_MODE } },
};

struct r63452 {
	struct drm_panel panel;
	struct mipi_dsi_device *dsi;
	struct regulator *vdd;
	struct regulator *vddio;
	struct regulator *lab;
	struct regulator *ibb;
	struct gpio_desc *reset;
	struct gpio_desc *avdd_en;	/* "enable" rail; optional */
	bool prepared;
};

static inline struct r63452 *to_r63452(struct drm_panel *panel)
{
	return container_of(panel, struct r63452, panel);
}

static int r63452_write(struct r63452 *ctx, const struct r63452_seq *s)
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

static int r63452_power_on(struct r63452 *ctx)
{
	int ret;

	/* Supply order follows the vendor dsi_panel_pwr_supply entry list:
	 * vdd -> vddio -> lab -> ibb(node, post-on-sleep 10ms). Each rail is
	 * optional so that an unmapped/unknown rail (vdd/vddio are FIXME) does
	 * not fail panel probe entirely. */
	if (ctx->vdd) {
		ret = regulator_enable(ctx->vdd);
		if (ret)
			return ret;
	}
	if (ctx->vddio) {
		ret = regulator_enable(ctx->vddio);
		if (ret)
			goto err_vdd;
	}
	if (ctx->lab) {
		ret = regulator_enable(ctx->lab);
		if (ret)
			goto err_vddio;
	}
	if (ctx->ibb) {
		ret = regulator_enable(ctx->ibb);
		if (ret)
			goto err_lab;
		msleep(10);
	}

	if (ctx->avdd_en) {
		/*
		 * gpiod_set_value_cansleep() 在 mainline（含 6.12）返回 void，
		 * 不像某些 downstream 内核返回 int。原写法按 7.x 移植时
		 * 也许还能编，但在 6.12 会报 "void value not ignored"。
		 */
		gpiod_set_value_cansleep(ctx->avdd_en, 1);
	}

	/* Vendor reset sequence <1 15>, <0 2>, <1 15>; reset is active-high. */
	gpiod_set_value_cansleep(ctx->reset, 1);
	msleep(15);
	gpiod_set_value_cansleep(ctx->reset, 0);
	msleep(2);
	gpiod_set_value_cansleep(ctx->reset, 1);
	msleep(15);

	return 0;

	/*
	 * 注意：这里没有 err_ibb 标签。
	 * 原驱动在 avdd_en 的 gpiod 调用失败时 goto err_ibb 去关 IBB，
	 * 但 mainline 的 gpiod_set_value_cansleep() 返回 void、不会失败；
	 * 而 IBB 使能之后 power_on 里再没有可失败的步骤，所以 err_ibb
	 * 变成不可达标签，clang/gcc 都会报 -Werror=unused-label。
	 * 若将来在 IBB 之后加入可失败操作，记得恢复 err_ibb 分支。
	 */
err_lab:
	if (ctx->lab)
		regulator_disable(ctx->lab);
err_vddio:
	if (ctx->vddio)
		regulator_disable(ctx->vddio);
err_vdd:
	if (ctx->vdd)
		regulator_disable(ctx->vdd);
	return ret;
}

static void r63452_power_off(struct r63452 *ctx)
{
	gpiod_set_value_cansleep(ctx->reset, 0);

	if (ctx->avdd_en)
		gpiod_set_value_cansleep(ctx->avdd_en, 0);

	if (ctx->ibb)
		regulator_disable(ctx->ibb);
	if (ctx->lab)
		regulator_disable(ctx->lab);
	if (ctx->vddio)
		regulator_disable(ctx->vddio);
	if (ctx->vdd)
		regulator_disable(ctx->vdd);
}

static int r63452_prepare(struct drm_panel *panel)
{
	struct r63452 *ctx = to_r63452(panel);
	int i;
	int ret;

	if (ctx->prepared)
		return 0;

	/* qcom,mdss-dsi-init-delay-us = 5000 */
	msleep(5);

	ret = r63452_power_on(ctx);
	if (ret < 0)
		return ret;

	for (i = 0; i < ARRAY_SIZE(r63452_on_seq); i++) {
		ret = r63452_write(ctx, &r63452_on_seq[i]);
		if (ret < 0) {
			r63452_power_off(ctx);
			return ret;
		}
	}

	ctx->prepared = true;
	return 0;
}

static int r63452_unprepare(struct drm_panel *panel)
{
	struct r63452 *ctx = to_r63452(panel);
	int i;

	if (!ctx->prepared)
		return 0;

	for (i = 0; i < ARRAY_SIZE(r63452_off_seq); i++)
		r63452_write(ctx, &r63452_off_seq[i]);

	r63452_power_off(ctx);
	ctx->prepared = false;

	return 0;
}

static int r63452_enable(struct drm_panel *panel)
{
	return 0;
}

static int r63452_disable(struct drm_panel *panel)
{
	return 0;
}

static const struct drm_display_mode r63452_mode = {
	/* h_total 1276 (1080+100+2+94), v_total 1952 (1920+8+4+20) @60 Hz */
	.clock = 149445,
	.hdisplay = 1080,
	.hsync_start = 1080 + 100,
	.hsync_end = 1080 + 100 + 2,
	.htotal = 1080 + 100 + 2 + 94,
	.vdisplay = 1920,
	.vsync_start = 1920 + 8,
	.vsync_end = 1920 + 8 + 4,
	.vtotal = 1920 + 8 + 4 + 20,
	.width_mm = 68,
	.height_mm = 122,
	.type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED,
};

static int r63452_get_modes(struct drm_panel *panel,
			    struct drm_connector *connector)
{
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(connector->dev, &r63452_mode);
	if (!mode)
		return -ENOMEM;

	drm_mode_set_name(mode);
	drm_mode_probed_add(connector, mode);

	connector->display_info.bpc = 8;
	connector->display_info.width_mm = 68;
	connector->display_info.height_mm = 122;

	return 1;
}

static const struct drm_panel_funcs r63452_funcs = {
	.prepare = r63452_prepare,
	.unprepare = r63452_unprepare,
	.enable = r63452_enable,
	.disable = r63452_disable,
	.get_modes = r63452_get_modes,
};

static int r63452_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct r63452 *ctx;
	int ret;

	ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->dsi = dsi;
	mipi_dsi_set_drvdata(dsi, ctx);

	/* All supplies optional: vdd(2.85V)/vddio(1.8V) real rails are
	 * FIXME(unverified) in the board dts; lab/ibb come from pmi8950. */
	ctx->vdd = devm_regulator_get_optional(dev, "vdd");
	if (IS_ERR(ctx->vdd)) {
		if (PTR_ERR(ctx->vdd) == -ENODEV)
			ctx->vdd = NULL;
		else
			return dev_err_probe(dev, PTR_ERR(ctx->vdd), "vdd\n");
	}
	ctx->vddio = devm_regulator_get_optional(dev, "vddio");
	if (IS_ERR(ctx->vddio)) {
		if (PTR_ERR(ctx->vddio) == -ENODEV)
			ctx->vddio = NULL;
		else
			return dev_err_probe(dev, PTR_ERR(ctx->vddio), "vddio\n");
	}
	ctx->lab = devm_regulator_get_optional(dev, "lab");
	if (IS_ERR(ctx->lab)) {
		if (PTR_ERR(ctx->lab) == -ENODEV)
			ctx->lab = NULL;
		else
			return dev_err_probe(dev, PTR_ERR(ctx->lab), "lab\n");
	}
	ctx->ibb = devm_regulator_get_optional(dev, "ibb");
	if (IS_ERR(ctx->ibb)) {
		if (PTR_ERR(ctx->ibb) == -ENODEV)
			ctx->ibb = NULL;
		else
			return dev_err_probe(dev, PTR_ERR(ctx->ibb), "ibb\n");
	}

	ctx->reset = devm_gpiod_get(dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(ctx->reset))
		return dev_err_probe(dev, PTR_ERR(ctx->reset), "reset\n");

	ctx->avdd_en = devm_gpiod_get_optional(dev, "enable", GPIOD_OUT_LOW);
	if (IS_ERR(ctx->avdd_en))
		return dev_err_probe(dev, PTR_ERR(ctx->avdd_en), "enable\n");

	/* 4 lanes, RGB888, CMD mode (no VIDEO). tx-eot-append is set in the
	 * vendor dtsi, so do NOT use MIPI_DSI_MODE_NO_EOT_PACKET. */
	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_CLOCK_NON_CONTINUOUS | MIPI_DSI_MODE_LPM;

	drm_panel_init(&ctx->panel, dev, &r63452_funcs, DRM_MODE_CONNECTOR_DSI);
	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0) {
		dev_err_probe(dev, ret, "Failed to attach to DSI host\n");
		drm_panel_remove(&ctx->panel);
	}

	return ret;
}

static void r63452_remove(struct mipi_dsi_device *dsi)
{
	struct r63452 *ctx = mipi_dsi_get_drvdata(dsi);

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);
}

static const struct of_device_id r63452_of_match[] = {
	{ .compatible = "oppo,r63452-cmd" },
	{ }
};
MODULE_DEVICE_TABLE(of, r63452_of_match);

static struct mipi_dsi_driver r63452_driver = {
	.probe = r63452_probe,
	.remove = r63452_remove,
	.driver = {
		.name = "panel-oppo-jdi-r63452",
		.of_match_table = r63452_of_match,
	},
};
module_mipi_dsi_driver(r63452_driver);

MODULE_DESCRIPTION("JDI R63452 command-mode display panel driver for OPPO R9s (16027)");
MODULE_LICENSE("GPL");