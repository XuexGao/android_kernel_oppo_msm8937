/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * QCOM 私有时钟标志（4.9/4.19 CAF 提供，6.12 mainline 删除了）。
 *
 * KGSL 用它切换 GPU 时钟的 retain/noretain 状态：
 *   `clk_set_flags(clk, CLKFLAG_RETAIN_MEM)` 之类。
 * 语义是「允许/禁止该时钟在低功耗状态下被保留」，
 * 属于**省电优化**，不影响时钟本身能否正常工作。
 *
 * mainline 的 clk 框架改用 clk_disable_unprepare / clk_set_rate 这套
 * 标准接口，没有对等的私有标志位。这里做成兼容层：
 *   · 保留 CLKFLAG_* 枚举，让调用点能编译
 *   · clk_set_flags() 空操作并返回 0（调用方会当成"设置成功"）
 *
 * 代价是 GPU 在空闲时可能不会主动关掉部分时钟，功耗略高一点，
 * 但功能完全正常。
 */
#ifndef __LINUX_CLK_QCOM_H
#define __LINUX_CLK_QCOM_H

#include <linux/clk.h>
#include <linux/types.h>

enum clk_flags {
	CLKFLAG_RETAIN_PERIPH = 0,
	CLKFLAG_NORETAIN_PERIPH,
	CLKFLAG_RETAIN_MEM,
	CLKFLAG_NORETAIN_MEM,
	CLKFLAG_PERIPH_OFF_SET,
	CLKFLAG_PERIPH_OFF_CLEAR,
};

static inline int clk_set_flags(struct clk *clk, unsigned long flags)
{
	/*
	 * 空操作。mainline 没有对等的私有标志位；
	 * 返回 0 让调用方认为设置成功，避免把 GPU 时钟路径当成错误。
	 */
	return 0;
}

#endif /* __LINUX_CLK_QCOM_H */
