#!/bin/bash
# 把已编译的 Image.gz + dtb 打成 AnyKernel3 刷机包。
#
# 用法（在仓库根目录）：
#   ci/mkflashable.sh out/
#
# 前置：out/ 里要有 Image.gz 与两个 OPPO dtb（由 CI 的「打包产物」步骤生成）。
#
# 为什么用 AnyKernel3：它只替换 boot 分区里的内核+dtb，不动其它分区，
# 刷坏了还能从原内核恢复 —— 对 bring-up 阶段风险最低的方案。
set -euo pipefail

OUT="${1:-out}"
ZIP_NAME="${2:-}"

# 转成绝对路径：下面会 cd 进 ak3-tpl，相对路径会失效
OUT="$(cd "$OUT" && pwd)"
[ -f "$OUT/Image.gz" ] || { echo "缺少 $OUT/Image.gz" >&2; exit 1; }

# AnyKernel3 模板
rm -rf ak3-tpl
git clone --depth 1 https://github.com/osm0sis/AnyKernel3.git ak3-tpl
cd ak3-tpl
rm -f boot.img *.zip
rm -rf .git

# 注意：后面的 sed / cat / zip 全部留在 ak3-tpl 目录里做，
# 别再 cd 回上层，否则 anykernel.sh 的相对路径会失效（之前踩过）。
cp "$OUT/Image.gz" Image.gz
cp "$OUT/msm8953-oppo-r9s.dtb"  . 2>/dev/null || true
cp "$OUT/msm8953-oppo-r9sk.dtb" . 2>/dev/null || true

# 保险：必须已经在 ak3-tpl 里，否则会误改/误生成仓库根的 anykernel.sh
[ -f anykernel.sh ] || { echo "不在 ak3-tpl 目录，anykernel.sh 不存在" >&2; exit 1; }

# anykernel.sh：指向 boot 分区，关闭机型校验与模块安装
sed -i 's|^BLOCK=.*|BLOCK=/dev/block/bootdevice/by-name/boot;|' anykernel.sh
sed -i 's|^do.devicecheck=.*|do.devicecheck=0;|'  anykernel.sh
sed -i 's|^do.modules=.*|do.modules=0;|'        anykernel.sh
sed -i 's|^do.systemless=.*|do.systemless=0;|'  anykernel.sh
sed -i 's|^do.cleanup=.*|do.cleanup=1;|'        anykernel.sh

# 机型选 dtb 的逻辑写成一个独立的 shell 片段，由 anykernel.sh 调用。
# 不塞进 anykernel.sh 的 heredoc，避免和 CI 的 YAML 打架。
cat > ak3-oppo-dtb.sh <<'AKD'
# --- OPPO R9s / R9sk 自动选 dtb ---
# 从当前运行内核的设备树读 board-id：
#   <8 0 16017> → 0x3e91 → r9s (EA8064 AMOLED)
#   <8 0 16027> → 0x3e9b → r9sk (JDI R63452，实机)
# 读不到就默认 r9sk。
AK_BOARD_ID="$(cat /proc/device-tree/qcom,board-id 2>/dev/null | tr -d '\0' | od -An -tx1 | tr -d ' \n')"
case "$AK_BOARD_ID" in
  *3e91*) AK_DTB=msm8953-oppo-r9s.dtb  ;;
  *3e9b*) AK_DTB=msm8953-oppo-r9sk.dtb ;;
  *)      AK_DTB=msm8953-oppo-r9sk.dtb ;;
esac
ui_print "  board-id=${AK_BOARD_ID:-unknown} -> $AK_DTB"
cat Image.gz "$AK_DTB" > Image.gz-dtb
AKD

# 让 anykernel.sh 在安装阶段 source 这个片段
if ! grep -q 'ak3-oppo-dtb.sh' anykernel.sh; then
  echo '. ./ak3-oppo-dtb.sh' >> anykernel.sh
fi

# 打包
[ -n "$ZIP_NAME" ] || ZIP_NAME="r9s-android-6.12-$(date +%Y%m%d).zip"
# 没有 zip 就退化用 python 打（CI 上装了 zip，本地可能没有）
if command -v zip >/dev/null 2>&1; then
  zip -r9 "$OUT/$ZIP_NAME" . -x '*.git*' >/dev/null
else
  python3 - "$OUT/$ZIP_NAME" <<'PZ'
import os, sys, zipfile
out = sys.argv[1]
with zipfile.ZipFile(out, 'w', zipfile.ZIP_DEFLATED) as z:
    for root, dirs, files in os.walk('.'):
        dirs[:] = [d for d in dirs if d != '.git']
        for f in files:
            p = os.path.join(root, f)
            z.write(p, os.path.relpath(p, '.'))
PZ
fi
echo "打包完成: $OUT/$ZIP_NAME"
ls -la "$OUT/$ZIP_NAME"
