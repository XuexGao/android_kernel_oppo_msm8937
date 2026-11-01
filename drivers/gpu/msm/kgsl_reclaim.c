// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 *
 * ---------------------------------------------------------------------------
 * 本文件是 KGSL 的「进程内存回收」模块 —— 在内存压力下把 GPU
 * 已经映射但可以牺牲的 shmem 页面换出去，等下次用的时候再换回来。
 *
 * 4.19 的实现依赖一批 QCOM 对 mm 子系统的**私有改动**：
 *
 *     check_move_unevictable_pages()    （操作 unevictable LRU）
 *     __pagevec_release()               （pagevec，已改为 folio 接口）
 *     pagevec_init / pagevec_add / pagevec_count
 *     reclaim_address_space()           （QCOM 私有，按 address_space 回收）
 *     proc_reclaim_notifier_register()  （QCOM 私有，进程退出通知）
 *     mapping_clear_unevictable / mapping_set_unevictable（私有 shmem 标志）
 *
 * 6.12 mainline 里这些**全部不存在**（pagevec 整体换成了 folio batch、
 * notifier 改名、QCOM 私有回收接口从未进入主线）。
 *
 * 要把这套逻辑在 6.12 上重写，等于重做一遍 QCOM 的 shmem 回收补丁，
 * 工作量大且与主线 mm 演进方向冲突。
 *
 * 结论：本模块降级为**空实现**。
 *
 * 影响评估：
 *  · 它只是内存压力下的**优化**，不影响 GPU 正确性 ——
 *    禁用后 GPU 占用的内存不会被主动回收出去，低内存时
 *    系统会选择杀进程（标准 Android 行为），而不是悄悄换出 GPU 页。
 *  · 本机是 Adreno 506 + 3/4GB 内存，日常场景下这部分回收收益有限。
 *  · 对外接口（kgsl_reclaim.h 里 7 个函数）全部保留，
 *    调用方（kgsl.c / kgsl_sharedmem.c）**一行都不用改**。
 *
 * 如果将来确实需要，正确方向是按 6.12 的 folio / shmem 接口重写，
 * 而不是把 4.19 的私有 API 硬搬回来。
 * ---------------------------------------------------------------------------
 */

#include "kgsl_reclaim.h"

int kgsl_reclaim_init(struct kgsl_device *device)
{
	/* 空实现：不注册 notifier、不建 sysfs 节点 */
	return 0;
}

void kgsl_reclaim_close(void)
{
	/* 空实现 */
}

int kgsl_reclaim_to_pinned_state(struct kgsl_process_private *priv)
{
	/*
	 * 返回 0 = 成功。调用方（kgsl.c 的 ioctl 路径）把它当作
	 * "已经处于 pinned 状态"来处理，不会因此报错。
	 */
	return 0;
}

void kgsl_reclaim_proc_sysfs_init(struct kgsl_process_private *process)
{
	/* 空实现：不创建 per-process 的 reclaim sysfs */
}

void kgsl_reclaim_proc_private_init(struct kgsl_process_private *process)
{
	/* 空实现 */
}

ssize_t kgsl_proc_max_reclaim_limit_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	/* 接受写入但忽略（没有回收机制可调），返回 count 让 sysfs 认为成功 */
	return count;
}

ssize_t kgsl_proc_max_reclaim_limit_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "0\n");
}
