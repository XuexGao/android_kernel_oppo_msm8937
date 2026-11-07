/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2016-2020, The Linux Foundation. All rights reserved.
 */

#ifndef __LEDS_QPNP_FLASH_H
#define __LEDS_QPNP_FLASH_H

#include <linux/leds.h>

#define ENABLE_REGULATOR		BIT(0)
#define DISABLE_REGULATOR		BIT(1)
#define QUERY_MAX_AVAIL_CURRENT		BIT(2)
#define QUERY_MAX_CURRENT		BIT(3)

#define FLASH_LED_PREPARE_OPTIONS_MASK	GENMASK(3, 0)

int qpnp_flash_register_led_prepare(struct device *dev, void *data);

#if (defined CONFIG_LEDS_QTI_FLASH || defined CONFIG_LEDS_QPNP_FLASH || defined CONFIG_LEDS_QPNP_FLASH_V2)
int qpnp_flash_led_prepare(struct led_trigger *trig, int options,
					int *max_current);
/*
 * 按 led_classdev 直接控制 flash LED 的稳压器。
 * 相机侧（/proc/qcom_flash）只有 cdev 没有 trigger，用它来补上
 * 4.9 里 qpnp_flash_led_prepare(ENABLE_REGULATOR) 那一步。
 */
int qpnp_flash_led_regulator_set(struct led_classdev *led_cdev, bool on);
#else
static inline int qpnp_flash_led_prepare(struct led_trigger *trig, int options,
					int *max_current)
{
	return -ENODEV;
}
#endif

#ifdef CONFIG_BACKLIGHT_QCOM_SPMI_WLED
int wled_flash_led_prepare(struct led_trigger *trig, int options,
					int *max_current);
#else
static inline int wled_flash_led_prepare(struct led_trigger *trig, int options,
					int *max_current)
{
	return -EINVAL;
}
#endif

#endif
