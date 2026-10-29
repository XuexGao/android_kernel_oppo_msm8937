/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * OPPO SMEM item IDs.
 *
 * 4.19 适配说明：
 *   4.9 的 include/soc/qcom/smem.h 里有一个巨大的 SMEM item 枚举
 *   （SMEM_ID_VENDOR0/1/2 ...），4.19 下游树里这个枚举已被裁掉，
 *   只剩下 include/soc/qcom/socinfo.h 里的 SMEM_HW_SW_BUILD_ID(137)。
 *   OPPO 的 bootloader 会把机型/内存信息写进 vendor SMEM 区，
 *   soc/oppo 需要按固定 item 号去读，所以这里把用到的号补回来。
 *
 * 数值来源：把 4.9 的 include/soc/qcom/smem.h 枚举原样交给编译器求值
 *   SMEM_APPS_BOOT_MODE=106  SMEM_ID_VENDOR0=134
 *   SMEM_ID_VENDOR1=135     SMEM_ID_VENDOR2=136
 *   SMEM_HW_SW_BUILD_ID=137（4.19 仍在 include/soc/qcom/socinfo.h 定义）
 */
#ifndef _OPPO_SMEM_IDS_H
#define _OPPO_SMEM_IDS_H

#ifndef SMEM_ID_VENDOR1
#define SMEM_ID_VENDOR1		135
#endif
#ifndef SMEM_ID_VENDOR2
#define SMEM_ID_VENDOR2		136
#endif

#endif /* _OPPO_SMEM_IDS_H */
