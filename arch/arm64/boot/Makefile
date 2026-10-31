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
# Image.gz-dtb：为兼容 CAF 传统打包而保留（但 ABL 并不读它）
#
# ⚠️ 重要更正：这条规则原本的注释说「ABL 会在 Image.gz-dtb 里按
# msm-id/board-id 挑 dtb」—— **那个说法是错的**，已实测确认。
#
# 实际情况：
#   · arm64 从来没有运行期 appended-DTB 解析器
#     （`CONFIG_BUILD_ARM64_APPENDED_DTB_IMAGE` 只是构建期开关，
#       git grep ARM_APPENDED_DTB -- arch/arm64/ 在 4.9/4.19 里也是空）
#   · CAF 的 ABL/lk 从 **boot.img 的 dt 分区**取 DTB，按 msm-id/board-id
#     挑一个，把地址放进 x0 传给内核
#   · ABL 从不扫描 kernel 尾部
#
# 所以这个 Image.gz-dtb 里的 dtb **不会被 ABL 使用**。
# 真正让我们的 DTB 生效的是 ci/mkflashable.sh 里的
# ak3-oppo-replace-dt.sh（刷机时把 boot.img 的 $SPLITIMG/dt 换掉）。
#
# 那为什么还保留这条规则？
#   1. 部分第三方 bootloader / 恢复模式的自定义流程会读它
#   2. 与 4.9/4.19 分支的产物形态保持一致，便于对照
#   3. 它是无害的（ABL 忽略尾部数据）
#
# 仍然限定为本机型的两个 dtb，不能 find 全树 ——
# mainline 的 dtb-y 有 1193 条（覆盖所有 ARM64 高通机型），
# 全拼进去会让产物从 16MB 涨到几十 MB。
# -----------------------------------------------------------------------------
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
