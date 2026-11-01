/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * CAF 私有 subsystem_restart 接口的兼容层（6.12 mainline 没有）。
 *
 * KGSL 用它加载 zap shader：
 *     ptr = subsystem_get(a5xx_core->zap_name);   // 传入 "a506_zap"
 *
 * mainline 6.12 里对应的机制是 remoteproc（rproc_get_by_phandle 等），
 * 但 KGSL 这里只需要「加载成功/失败」这个布尔语义。
 *
 * 本仓库的 GPU 节点在 DTS 里是 status = "disabled"（MSM8953 的
 * Adreno 506 走 KGSL 的独立 probe 路径，还没接 zap-shader 的
 * remoteproc 绑定），所以这条路径当前根本不会被执行。
 *
 * 为了让它能编译通过，这里提供最小实现：
 *   · subsystem_get() 返回 ERR_PTR(-ENODEV)
 *     → 调用方会认为「zap 固件不可用」，跳过加载，
 *       与主线未配置 zap-shader 时的行为一致；
 *   · subsystem_put() 空操作。
 *
 * 后续如果要真正启用 GPU 硬件加速 + secure 模式，
 * 需要把这里换成 rproc_get_by_phandle() 的真实实现。
 */
#ifndef __SOC_QCOM_SUBSYSTEM_RESTART_H
#define __SOC_QCOM_SUBSYSTEM_RESTART_H

#include <linux/err.h>
#include <linux/errno.h>
#include <linux/types.h>

static inline void *subsystem_get(const char *name)
{
	return ERR_PTR(-ENODEV);
}

static inline void subsystem_put(void *subsystem)
{
}

#endif /* __SOC_QCOM_SUBSYSTEM_RESTART_H */
