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
#     $(obj)/Image.gz-dtb: $(obj)/Image.gz $(DTB_OBJS) FORCE
#             $(call if_changed,cat)
#
# 这里照搬，DTB_OBJS 取 dts 目录下所有 .dtb（CAF 会自己按 board-id 选）。
# -----------------------------------------------------------------------------
DTB_OBJS := $(shell find $(obj)/dts/ -name \*.dtb 2>/dev/null)

targets += Image.gz-dtb

$(obj)/Image.gz-dtb: $(obj)/Image.gz $(DTB_OBJS) FORCE
	$(call if_changed,cat)
