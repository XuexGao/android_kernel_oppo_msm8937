#!/bin/bash
# 把内核产出的 Image.gz-dtb 打成 AnyKernel3 刷机包。
#
# 用法（在仓库根目录）：
#   ci/mkflashable.sh out/ [zip名]
#
# 前置：out/ 里要有 Image.gz-dtb（由 CI 的 make Image.gz-dtb 产出）。
#
# ---------------------------------------------------------------------------
# 关于 DTB：ABL **不读 kernel 尾部**，必须放进 boot.img 的 dt 区
# ---------------------------------------------------------------------------
#
# 历史教训：我一开始以为 CAF 的 ABL 会在 Image.gz-dtb 这个
# 「gzip 流 + 若干 dtb」的拼接体里按 msm-id/board-id 挑 dtb。
# **这是错的。**
#
# 实际情况（逐行读 AnyKernel3 的 tools/ak3-core.sh 确认）：
#
#   split_boot():101-104
#       原厂 boot.img 的 dt 若是 QCDT → 存成 $SPLITIMG/dt（**原厂的**）
#       若是裸 FDT → cat 进 kernel.gz-dtb 尾部（没有任何东西会读它）
#
#   flash_boot():349-356  找 $dt，命中 $SPLITIMG/dt
#   flash_boot():381      mkbootimg ... --dt $SPLITIMG/dt
#                         ↑ 原厂 QCDT 原样写回
#
#   ABL 从 boot.img 的 dt 区取 DTB → 内核拿到的一直是 CAF 的原厂 DTB
#
# 所以正确做法是：把我们的 dtb 做成 boot.img 的 dt 区。
# 本脚本的 ak3-oppo-replace-dt.sh 就是在 dump_boot 之后、
# write_boot 之前把 $SPLITIMG/dt 换成我们的（QCDT/裸FDT 都处理）。
#
# Image.gz-dtb 仍然保留（无害），但**不要指望它能让 ABL 用上我们的 DTB**。
#
# 判据：刷机后 pstore 里 `Machine model` 应该是 "OPPO R9sk"，
# 而不是 "Qualcomm Technologies, Inc. MSM8953 + PMI8950 MTP"。
#
# ---------------------------------------------------------------------------

set -euo pipefail

# 固定到仓库根目录执行：后面有 `rm -rf ak3-tpl`，
# 如果在别的目录跑这个脚本，会误删当前目录下同名目录。
cd "$(git rev-parse --show-toplevel)"

OUT="${1:-out}"
ZIP_NAME="${2:-}"

if [ ! -d "$OUT" ]; then
  echo "ERROR: 输出目录 '$OUT' 不存在" >&2
  exit 1
fi

# 转成绝对路径：下面会 cd 进 ak3-tpl，相对路径会失效
OUT="$(cd "$OUT" && pwd)"
[ -f "$OUT/Image.gz-dtb" ] || {
  echo "ERROR: 缺少 $OUT/Image.gz-dtb" >&2
  echo "       请先 make ARCH=arm64 Image.gz-dtb" >&2
  exit 1
}

# AnyKernel3 模板
rm -rf ak3-tpl
git clone --depth 1 https://github.com/osm0sis/AnyKernel3.git ak3-tpl
cd ak3-tpl
rm -f boot.img *.zip
rm -rf .git

# 注意：后面的操作全部留在 ak3-tpl 目录里做，
# 别再 cd 回上层，否则 anykernel.sh 的相对路径会失效（之前踩过）。
cp "$OUT/Image.gz-dtb" Image.gz-dtb

# 把两个机型 dtb 也单独放进包里。
#
# 为什么：CAF 的 ABL 不在 kernel 尾部找 DTB，而是从 boot.img 的
# **dt 分区**里按 msm-id / board-id 挑一个传给内核（x0）。
# AnyKernel3 的 flash_boot() 会保留原厂的 SPLITIMG/dt 并
# `mkbootimg --kernel <新的> --dt <原厂的>` ——
# 也就是说我们追加在 Image.gz-dtb 尾部的 dtb **从来没被用过**。
# 这解释了为什么日志里 Machine model 一直是 CAF 的
# "MSM8953 + PMI8950 MTP"，而我们的 DTB 里写的是 "OPPO R9sk"。
#
# 所以必须把我们的 dtb 作为独立 dt 覆盖掉原厂的。
for d in msm8953-oppo-r9s.dtb msm8953-oppo-r9sk.dtb; do
  [ -f "$OUT/$d" ] && cp "$OUT/$d" "./$d"
done
# msm_id/board_id 由构建阶段用 dtc 提取好，刷机时直接读，避免在设备上解析 FDT
[ -f "$OUT/msm8953-oppo-dtb-ids.txt" ] && cp "$OUT/msm8953-oppo-dtb-ids.txt" .

# 保险：必须已经在 ak3-tpl 里
[ -f anykernel.sh ] || { echo "不在 ak3-tpl 目录，anykernel.sh 不存在" >&2; exit 1; }

# anykernel.sh：指向 boot 分区
#
# do.devicecheck=0：刷机时跑的内核（TWRP）里 ro.product.device 可能是
#   twrp_* 或 lineage_*，跟我们的机型名对不上，开着会误拒。
# do.modules=0    ：本内核全内置，不装独立模块。
# do.systemless=0 ：不往 system 里塞东西。
sed -i 's|^BLOCK=.*|BLOCK=/dev/block/bootdevice/by-name/boot;|' anykernel.sh
sed -i 's|^do.devicecheck=.*|do.devicecheck=0;|'  anykernel.sh
sed -i 's|^do.modules=.*|do.modules=0;|'        anykernel.sh
sed -i 's|^do.systemless=.*|do.systemless=0;|'  anykernel.sh
sed -i 's|^do.cleanup=.*|do.cleanup=1;|'        anykernel.sh

# ---------------------------------------------------------------------------
# 用我们的 DTB 覆盖原厂的 dt 分区
#
# CAF 的 ABL 不在 kernel 尾部扫 DTB，而是从 boot.img 的 dt 分区
# 按 msm-id / board-id 挑一个传给内核。AnyKernel3 的 flash_boot()
# 会保留原厂 dt 并 `mkbootimg --dt <原厂dt>`，所以我们追加在
# Image.gz-dtb 尾部的 dtb 根本不会被使用。
#
# 这里在 dump_boot（内部会 split_boot 解出 SPLITIMG/dt）之后、
# write_boot 之前，把原厂 dt 替换成我们的。
#
# 两种原厂格式都要处理：
#   · 裸 FDT（魔数 d00dfeed）—— 直接拷贝对应机型 dtb
#   · QCDT（魔数 'QCDT'）    —— 生成只含我们 dtb 的 QCDT 表
#
# 按 board-id 选机型：<8 0 16017> → r9s，<8 0 16027> → r9sk
# ---------------------------------------------------------------------------
cat > ak3-oppo-replace-dt.sh <<'AKDT'
AK_BOARD_ID="$(cat /proc/device-tree/qcom,board-id 2>/dev/null | tr -d '\0' | od -An -tx1 | tr -d ' \n')"
case "$AK_BOARD_ID" in
  *3e91*) AK_DTB_NAME=msm8953-oppo-r9s.dtb ;;
  *3e9b*) AK_DTB_NAME=msm8953-oppo-r9sk.dtb ;;
  *)      AK_DTB_NAME=msm8953-oppo-r9sk.dtb ;;
esac
ui_print "  board-id=${AK_BOARD_ID:-unknown} -> $AK_DTB_NAME"

if [ -f "$AK_DTB_NAME" ] && [ -d "$SPLITIMG" ]; then
  # 判断原厂 dt 的格式（SPLITIMG/dt 由 split_boot 生成）
  AK_DT_FILE="$SPLITIMG/dt"

  # 情况 A：split_boot 没生成 dt 文件。
  # 这发生在原厂 boot.img 的 dt **不是** QCDT/ELF 的时候 ——
  # split_boot 会走 `*)` 分支，把原厂 dt 直接 cat 到 kernel.gz-dtb 尾部，
  # 不生成独立 dt 文件。此时 flash_boot() 的 $dt 为空，
  # mkbootimg 不带 --dt，ABL 就去 kernel 尾部找 —— 但 6.12 内核
  # 不会自己扫，所以也拿不到我们的 dtb。
  #
  # 解法：我们自己创建 $SPLITIMG/dt，让 flash_boot 用 --dt 传进去。
  if [ ! -f "$AK_DT_FILE" ]; then
    ui_print "  原厂无独立 dt，创建 dt（裸 FDT）"
    cp -f "$AK_DTB_NAME" "$AK_DT_FILE"
    AK_DT_FILE=""
  fi

  if [ -n "$AK_DT_FILE" ]; then
    AK_MAGIC="$(od -An -N4 -tx1 "$AK_DT_FILE" 2>/dev/null | tr -d ' \n')"
    case "$AK_MAGIC" in
      51434454*)   # "QCDT"
        ui_print "  原厂 dt 是 QCDT 格式，生成新的 QCDT"
        # msm_id / board_id 由构建阶段写进 msm8953-oppo-dtb-ids.txt
        # （格式：机型文件名 msm_id board_id，每行一条）。
        # 刷机时直接读这个文本，不再在设备上解析 FDT 二进制 ——
        # 手写 FDT 解析器太容易出错（对齐、NOP、END_NODE 的细节），
        # 而构建阶段有 dtc 可用，可靠性高得多。
        python3 - "$AK_DT_FILE" "$AK_DTB_NAME" <<'PYQCDT'
import struct, sys, os

out_path, dtb_path = sys.argv[1], sys.argv[2]
dtb = open(dtb_path, 'rb').read()
base = os.path.basename(dtb_path)

msm_id = board_id = None
ids_file = 'msm8953-oppo-dtb-ids.txt'
if os.path.exists(ids_file):
    for line in open(ids_file):
        parts = line.split()
        if len(parts) == 3 and parts[0] == base:
            msm_id, board_id = int(parts[1]), int(parts[2])
            break

if msm_id is None or board_id is None:
    # 兜底：按机型名硬编码（本仓库只有这两个机型）
    if 'r9sk' in base:
        msm_id, board_id = 293, 16027
    elif 'r9s' in base:
        msm_id, board_id = 293, 16017
    else:
        msm_id, board_id = 293, 16027

# QCDT: magic 'QCDT'(4) + version=1(4) + num_entries=1(4)
#       + entry[20] = platform_id/variant_id/soc_rev/offset/size
#       + dtb（按 4096 对齐）
hdr_len, entry_len = 12, 20
data_start = hdr_len + entry_len
pad = (-len(dtb)) % 4096
out = b'QCDT' + struct.pack('<II', 1, 1)
out += struct.pack('<IIIII', msm_id, board_id, 0, data_start, len(dtb))
out += dtb + b'\x00' * pad
open(out_path, 'wb').write(out)
print(f"    QCDT: msm_id={msm_id} board_id={board_id} "
      f"dtb={len(dtb)}B pad={pad}B total={len(out)}B")
PYQCDT
        ;;
      d00dfeed*)
        ui_print "  原厂 dt 是裸 FDT，直接替换"
        cp -f "$AK_DTB_NAME" "$AK_DT_FILE"
        ;;
      *)
        # 认不出格式：直接覆盖成裸 FDT，能为多数 ABL 接受
        ui_print "  原厂 dt 格式未知($AK_MAGIC)，覆盖成裸 FDT"
        cp -f "$AK_DTB_NAME" "$AK_DT_FILE"
        ;;
    esac
    ui_print "  已替换 dt"
  fi
fi
AKDT

# 在 dump_boot 之后插入（dump_boot 内部会 split_boot，产生 SPLITIMG/dt）
if ! grep -q 'ak3-oppo-replace-dt.sh' anykernel.sh; then
  python3 - <<'PYINS'
import re
p = 'anykernel.sh'
s = open(p).read()
m = re.search(r'^dump_boot;', s, re.M)
assert m, 'anykernel.sh 里找不到 dump_boot;'
# 1) dump_boot → split_boot
#    AnyKernel3 官方注释：带 dtb 的 CAF 设备应跳过 ramdisk 解包/重打包
#    （hdr v4 + vendor_kernel_boot 场景用 split_boot；我们的设备 ramdisk
#     不需要改，跳过更安全，也不会碰 vendor_dlkm / dtbo 那些不存在的分区）
s = s[:m.start()] + 'split_boot;' + s[m.end():]
# 2) 在 split_boot 之后插入 dt 替换钩子（此时 $SPLITIMG 已生成）
m2 = re.search(r'^split_boot;', s, re.M)
s = s[:m2.end()] + '\n. ./ak3-oppo-replace-dt.sh\n' + s[m2.end():]
# 3) write_boot → flash_boot（与 split_boot 配套）
s = s.replace('write_boot;', 'flash_boot;', 1)
open(p, 'w').write(s)
print('  dump_boot→split_boot, write_boot→flash_boot, 已插入 dt 替换钩子')
PYINS
fi

# 校验：sed 没匹配上时会**静默不改**，包里的 anykernel.sh 就用上游默认值。
# 上游若改了默认分区名，会刷错分区 —— 这种错只有刷机后才暴露，
# 所以这里必须显式确认改成功了。
for pat in 'BLOCK=/dev/block/bootdevice/by-name/boot;' \
           'do.devicecheck=0;' 'do.modules=0;'; do
  grep -qF "$pat" anykernel.sh || {
    echo "ERROR: anykernel.sh 里没找到 '$pat'，sed 可能没匹配上" >&2
    echo "       （AnyKernel3 上游改了变量格式？）" >&2
    exit 1
  }
done

# ---------------------------------------------------------------------------
# 自检：Image.gz-dtb 里必须真的含有 dtb
#
# 现在不再手工拼接，风险点变成了「内核 Makefile 的 cat 规则有没有生效」。
# 如果没生效，Image.gz-dtb 就只是个 gzip 流，ABL 会退回内置 DTB，
# 而这种情况**构建不会报错**，只有刷机才发现 —— 所以在这里拦。
# 做法：扫整份数据找 FDT 魔数（大端 d00dfeed）。
# ---------------------------------------------------------------------------
python3 - <<'PYDTB'
import sys, struct

fn = 'Image.gz-dtb'
data = open(fn, 'rb').read()

if data[:3] != b'\x1f\x8b\x08':
    print('自检失败：Image.gz-dtb 开头不是 gzip 流', file=sys.stderr)
    sys.exit(1)

count = 0
pos = 0
offsets = []
while True:
    pos = data.find(b'\xd0\x0d\xfe\xed', pos)
    if pos < 0:
        break
    offsets.append(pos)
    count += 1
    pos += 4

if count == 0:
    print('自检失败：Image.gz-dtb 里找不到任何 dtb（FDT 魔数 d00dfeed）', file=sys.stderr)
    print('         说明内核 Makefile 的 Image.gz-dtb 规则没生效，', file=sys.stderr)
    print('         或 dtbs 没编出来。刷进去 ABL 会退回它内置的 CAF DTB。', file=sys.stderr)
    sys.exit(1)

# 上限检查：只应包含本机型（r9s + r9sk）两个 dtb。
# mainline 的 dtb-y 有 1193 个条目，如果用
#     find dts/ -name *.dtb
# 就会把全部机型都拼进去 —— 体积暴涨，而且 CAF 的 ABL 是按
# msm-id / board-id **顺序扫描**找第一个匹配的，1193 个里混着别的
# 机型，有挑错的风险。（真的发生过：产物里 1193 个 dtb。）
EXPECTED_MAX = 4
if count > EXPECTED_MAX:
    print(f'自检失败：Image.gz-dtb 里含 {count} 个 dtb，超过上限 {EXPECTED_MAX}', file=sys.stderr)
    print('         应该只拼 msm8953-oppo-r9s/r9sk 两个机型。', file=sys.stderr)
    print('         检查 arch/arm64/boot/Makefile 的 CAF_DTB_NAMES。', file=sys.stderr)
    sys.exit(1)

print(f'  自检通过：Image.gz-dtb 内含 {count} 个 dtb')
for o in offsets:
    totalsize = struct.unpack('>I', data[o + 4:o + 8])[0]
    print(f'    dtb @0x{o:x}  totalsize={totalsize}')
PYDTB

if [ -z "$ZIP_NAME" ]; then
  ZIP_NAME="r9s-android-6.12-$(date +%Y%m%d).zip"
fi

# 有 zip 就用，没有就退化用 python（CI 上装了 zip，本地可能没有）
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

echo "  打包完成: $OUT/$ZIP_NAME"
ls -la "$OUT/$ZIP_NAME"
