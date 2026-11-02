// SPDX-License-Identifier: GPL-2.0-only
/*
 * QCOM devfreq 带宽扩展的兼容层
 *
 * ---------------------------------------------------------------------------
 * KGSL 的 kgsl_pwrctrl.c 会调用四个 QCOM 私有扩展：
 *
 *     devfreq_vbif_update_bw()               （governor_bw_vbif.c）
 *     devfreq_vbif_register_callback(cb)     （governor_bw_vbif.c）
 *     devfreq_suspend_devbw(dev)             （devfreq_devbw.c）
 *     devfreq_resume_devbw(dev)              （devfreq_devbw.c）
 *
 * 这两个文件在 4.9/4.19 是 QCOM 对 devfreq 子系统的私有扩展：
 *   · devbw  —— 给 device 挂一个「按吞吐量投票」的 devfreq device
 *                （DEVFREQ_GOV_DEVBW），GPU 按自身负载投 DDR 带宽
 *   · vbif  —— VBIF 的总线仲裁器带宽投票
 *
 * 6.12 mainline **完全没有这两个 governor**（QCOM 的 devbw/bw_vbif
 * 从未进入主线；主线对应的是 interconnect framework + icc votes）。
 *
 * 影响评估：
 *  · 它们只负责**动态调整 DDR 总线带宽**，是性能/省电逻辑。
 *    禁用后 GPU 仍能正常工作，只是带宽固定在 bootloader/初始配置值，
 *    高负载场景（大型游戏）可能有性能损失或轻微欠载。
 *  · 本机 Adreno 506 属于低端 GPU，带宽压力有限。
 *  · 不涉及任何正确性风险 —— 不会导致 GPU 挂死或画面错误。
 *
 * 所以这里提供空实现，保证链接完整、调用点不用改。
 *
 * 后续如果要补回带宽调节，正确方向是用 6.12 的 interconnect：
 *     icc_get() / icc_set_bw() 替代 devfreq_devbw 那套。
 * ---------------------------------------------------------------------------
 */

#include <linux/device.h>
#include <linux/msm_adreno_devfreq.h>
#include <soc/qcom/devfreq_devbw.h>
#include <linux/types.h>

/*
 * 原型是 int devfreq_vbif_update_bw(void)（msm_adreno_devfreq.h:89），
 * 返回 0 = 成功。
 */
int devfreq_vbif_update_bw(void)
{
	/* 空实现：不做 VBIF 带宽投票 */
	return 0;
}

/*
 * 签名必须与 include/linux/msm_adreno_devfreq.h:90 的原型一致：
 *     void devfreq_vbif_register_callback(getbw_func func, void *data);
 * 之前写成 (void *p) 会让第二个实参被静默丢弃（aarch64 上落在 x1），
 * 虽然当前不会崩，但 ABI 上是错的。
 */
void devfreq_vbif_register_callback(getbw_func func, void *data)
{
	/* 空实现：不注册回调（mainline 没有 vbif 带宽 governor） */
}

int devfreq_suspend_devbw(struct device *dev)
{
	/* 空实现：返回 0 = 成功 */
	return 0;
}

int devfreq_resume_devbw(struct device *dev)
{
	/* 空实现：返回 0 = 成功 */
	return 0;
}
