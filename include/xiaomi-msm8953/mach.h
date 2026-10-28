/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Xiaomi MSM8953 机型判断 stub
 *
 * 本内核面向 OPPO R9s。下游 Xiaomi 的 qpnp-fg / qpnp-smbcharger 里插了
 * 一批 xiaomi_msm8953_mach_get() 判断（针对小米各机型微调电量计/充电
 * 参数），其中部分没被 IS_ENABLED(CONFIG_MACH_XIAOMI_MSM8953) 包住，
 * 在非小米内核上会直接报 undeclared。
 *
 * 这里提供恒假的 stub：所有 == XIAOMI_MSM8953_MACH_xxx 都为假，
 * 小米专属分支全部跳过，QCOM 原生逻辑不受影响。
 */
#ifndef _XIAOMI_MSM8953_MACH_STUB_H
#define _XIAOMI_MSM8953_MACH_STUB_H

#ifndef CONFIG_MACH_XIAOMI_MSM8953

#define XIAOMI_MSM8953_MACH_TIFFANY (-1)
#define XIAOMI_MSM8953_MACH_VINCE   (-2)
#define XIAOMI_MSM8953_MACH_MIDO    (-3)
#define XIAOMI_MSM8953_MACH_SAKURA  (-4)
#define XIAOMI_MSM8953_MACH_TISSOT  (-5)
#define XIAOMI_MSM8953_MACH_YSL     (-6)

static inline int xiaomi_msm8953_mach_get(void)
{
	return -100;	/* 不匹配任何已知机型 */
}

#endif /* !CONFIG_MACH_XIAOMI_MSM8953 */
#endif /* _XIAOMI_MSM8953_MACH_STUB_H */
