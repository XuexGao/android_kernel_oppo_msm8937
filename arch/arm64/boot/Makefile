#
# arch/arm64/boot/Makefile
#
# This file is included by the global makefile so that you can add your own
# architecture-specific flags and dependencies.
#
# This file is subject to the terms and conditions of the GNU General Public
# License.  See the file "COPYING" in the main directory of this archive
# for more details.
#
# Copyright (C) 2012, ARM Ltd.
# Author: Will Deacon <will.deacon@arm.com>
#
# Based on the ia64 boot/Makefile.
#

OBJCOPYFLAGS_Image :=-O binary -R .note -R .note.gnu.build-id -R .comment -S

targets := Image Image.bz2 Image.gz Image.lz4 Image.lzma Image.lzo \
	Image.zst Image.xz image.fit

$(obj)/Image: vmlinux FORCE
	$(call if_changed,objcopy)

$(obj)/Image.bz2: $(obj)/Image FORCE
	$(call if_changed,bzip2)

$(obj)/Image.gz: $(obj)/Image FORCE
	$(call if_changed,gzip)

$(obj)/Image.lz4: $(obj)/Image FORCE
	$(call if_changed,lz4)

$(obj)/Image.lzma: $(obj)/Image FORCE
	$(call if_changed,lzma)

$(obj)/Image.lzo: $(obj)/Image FORCE
	$(call if_changed,lzo)

$(obj)/Image.zst: $(obj)/Image FORCE
	$(call if_changed,zstd)

$(obj)/Image.xz: $(obj)/Image FORCE
	$(call if_changed,xzkern)

$(obj)/image.fit: $(obj)/Image $(obj)/dts/dtbs-list FORCE
	$(call if_changed,fit)

EFI_ZBOOT_PAYLOAD	:= Image
EFI_ZBOOT_BFD_TARGET	:= elf64-littleaarch64
EFI_ZBOOT_MACH_TYPE	:= ARM64
EFI_ZBOOT_FORWARD_CFI	:= $(CONFIG_ARM64_BTI_KERNEL)

EFI_ZBOOT_OBJCOPY_FLAGS	= --add-symbol zboot_code_size=0x$$( \
				$(NM) vmlinux|grep _kernel_codesize|cut -d' ' -f1)

include $(srctree)/drivers/firmware/efi/libstub/Makefile.zboot

# -----------------------------------------------------------------------------
# CAF（Qualcomm ABL/lk）专用：Image.gz 后面追加 DTB 列表
#
# 为什么需要这个：
#   Qualcomm 的 bootloader（ABL/lk）不像 mainline 那样从 x0 拿一份 DTB，
#   而是在 Image.gz-dtb 这个"gzip 流 + 若干 dtb"的拼接体里，
#   按 root 节点的 qcom,msm-id / qcom,board-id 自己挑匹配的那个。
#
#   如果按 mainline 的方式只给一个裸 Image.gz（或不给 dtb），
#   ABL 就退回它自己内置的那份 CAF DTB —— 那份 DTB 描述的是
#   4.9 时代的硬件节点名（modem_region@0 / wake-gic@601d4 等），
#   与 mainline 6.12 的驱动树完全对不上。
#
#   实测证据（6.12 诊断内核的 pstore 日志）：
#       Machine model: Qualcomm Technologies, Inc. MSM8953 + PMI8950 MTP
#       OF: reserved mem: node venus_region@0 compatible matching fail
#       arch_timer: No interrupt available, giving up
#   而本仓库的 DTB 是 model = "OPPO R9sk"、
#   reserved-memory 用 mpss@86c00000 / venus@91400000 的命名。
#   两者不同 → 证明内核用的是 ABL 内置 DTB，不是我们的。
#
# 4.9 和 4.19 分支都有这条规则，且真机验证可用：
#     $(obj)/Image.gz-dtb: $(obj)/Image.gz $(CAF_DTB_OBJS) $(obj)/dts/dtbs-list FORCE
#             $(call if_changed,cat)
#
# 这里照搬，DTB_OBJS 取 dts 目录下所有 .dtb（CAF 会自己按 board-id 选）。
# -----------------------------------------------------------------------------
# -----------------------------------------------------------------------------
# CAF（Qualcomm ABL/lk）专用：Image.gz 后面追加 DTB 列表
#
# 为什么需要：Qualcomm 的 bootloader 不在 x0 给 DTB，而是在
# Image.gz-dtb 这个「gzip 流 + 若干 dtb」的拼接体里，按每个 dtb 的
# root 节点 qcom,msm-id / qcom,board-id 自己挑匹配的那份。
# 只给裸 Image.gz 的话它退回内置的 CAF DTB（4.9 时代的节点名），
# 与 mainline 6.12 驱动树对不上 → arch_timer 拿不到中断、外设全废。
# 实测证据见 6.12 诊断内核的 pstore 日志。
#
# 拼接方式与 4.9 / 4.19 完全一致（那两个分支真机验证可启动）：
#     DTB_OBJS := $(shell find $(obj)/dts/ -name \*.dtb)
#     $(obj)/Image.gz-dtb: $(obj)/Image.gz $(DTB_OBJS) FORCE
#             $(call if_changed,cat)
#
# 注意两个前提，缺一不可：
#
# 1) **必须先编 dtbs**。4.19 是靠 arch/arm64/Makefile 里的
#        all: $(KBUILD_DTBS) $(KBUILD_TARGET)
#    先跑 dtbs 再跑内核目标来保证顺序的。6.12 的 arch/arm64/Makefile
#    没有这一行，所以本项目在顶层 Makefile 的调用处显式先 make dtbs
#    （见 .github/workflows/build-6.12.yml 的编译步骤）。
#
# 2) 因为 $(shell ...) 是解析期求值，如果 dtbs 没先编完，
#    DTB_OBJS 就是空 → cat 只拼出个 Image.gz。
#    所以 recipe 里再做一次运行时 find，并且**空列表直接报错退出**，
#    绝不允许静默产出无 dtb 的 Image.gz-dtb。
# -----------------------------------------------------------------------------
# 只拼**本机型**的 dtb，不能把整棵树的 dtb 全塞进去。
#
# mainline 的 arch/arm64/boot/dts/qcom/Makefile 里 dtb-y 有 1193 个条目
# （覆盖所有 ARM64 高通机型）。如果按 4.19 的写法
#     DTB_OBJS := $(shell find $(obj)/dts/ -name *.dtb)
# 就会把全部 1193 个 dtb 都拼到 Image.gz-dtb 后面 ——
# 实测产物里确实是 1193 个 dtb，问题有两个：
#   1. 体积暴涨（每个 dtb 几十 KB，加起来好几 MB）
#   2. CAF 的 ABL 是按 qcom,msm-id / qcom,board-id 在拼接体里**顺序扫描**
#      找第一个匹配的。1193 个里混着各种机型的 dtb，一旦某个机型的
#      board-id 恰好与我们相同（或者 msm-id 相同），就有挑错的风险。
#
# 4.19 分支没这个问题，因为它的 dtb-y 只有 9 个（全是本平台机型）。
#
# 所以这里显式限定为 OPPO 的两个机型 dtb。
CAF_DTB_NAMES := msm8953-oppo-r9s.dtb msm8953-oppo-r9sk.dtb
CAF_DTB_OBJS = $(addprefix $(obj)/dts/qcom/, $(CAF_DTB_NAMES))

targets += Image.gz-dtb

$(obj)/Image.gz-dtb: $(obj)/Image.gz $(CAF_DTB_OBJS) $(obj)/dts/dtbs-list FORCE
	$(Q)set -e; \
	cat $(obj)/Image.gz > $@.tmp; \
	for d in $(CAF_DTB_OBJS); do \
		if [ ! -f "$$d" ]; then \
			echo "ERROR: 缺少 $$d，请先 make dtbs" >&2; \
			rm -f $@.tmp; exit 1; \
		fi; \
		cat $$d >> $@.tmp; \
	done; \
	mv $@.tmp $@; \
	echo "  Image.gz-dtb: Image.gz + $(words $(CAF_DTB_OBJS)) 个 dtb"
