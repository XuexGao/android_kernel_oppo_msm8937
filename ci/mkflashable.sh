#!/bin/bash
# 把内核产出的 Image.gz-dtb 打成 AnyKernel3 刷机包。
#
# 用法（在仓库根目录）：
#   ci/mkflashable.sh out/ [zip名]
#
# 前置：out/ 里要有 Image.gz-dtb（由 CI 的 make Image.gz-dtb 产出）。
#
# ---------------------------------------------------------------------------
# 为什么直接吃 Image.gz-dtb，而不是自己拼 Image.gz + dtb
# ---------------------------------------------------------------------------
#
# Qualcomm 的 bootloader（ABL/lk）不像 mainline 那样从 x0 拿一份 DTB。
# 它在 Image.gz-dtb 这个「gzip 流 + 若干 dtb」的拼接体里，
# 按每个 dtb 的 root 节点 qcom,msm-id / qcom,board-id 自己挑匹配的那份。
#
# 如果只给一个裸 Image.gz，或者手工 `cat Image.gz <某个dtb>`，
# ABL 认不出来就退回它内置的 CAF DTB —— 那是 4.9 时代的节点名
# （modem_region@0 / wake-gic@601d4 / venus_region@0），
# 与 mainline 6.12 的驱动树完全对不上，实测 pstore 日志里是：
#     arch_timer: No interrupt available, giving up
#     Failed to initialize '/soc/timer@b120000': -22
# 外设全废，最后 VFS 挂不上 rootfs 而 panic。
#
# 所以这里严格照 4.9 / 4.19 分支的做法：内核 Makefile 里
#     $(obj)/Image.gz-dtb: $(obj)/Image.gz $(DTB_OBJS) FORCE
#             $(call if_changed,cat)
# 把 dts 目录下**所有** dtb 拼上去，打包脚本只负责搬运。
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
  if [ -f "$AK_DT_FILE" ]; then
    AK_MAGIC="$(od -An -N4 -tx1 "$AK_DT_FILE" 2>/dev/null | tr -d ' \n')"
    case "$AK_MAGIC" in
      51434454*)   # "QCDT"
        ui_print "  原厂 dt 是 QCDT 格式，生成新的 QCDT"
        python3 - "$AK_DT_FILE" "$AK_DTB_NAME" <<'PYQCDT'
import struct, sys
out_path, dtb_path = sys.argv[1], sys.argv[2]
dtb = open(dtb_path, 'rb').read()
# 从 dtb 根节点读 qcom,msm-id / qcom,board-id（简化：用已知值）
# MSM8953 = 293, R9s=16017 / R9sk=16027
import re
def read_prop(name):
    # 粗略从 dtb 结构块里找，失败就用默认
    return None
# 本仓库 msm8953-oppo-r9s/r9sk 的已知标识
if 'r9s' in dtb_path and 'r9sk' not in dtb_path:
    msm_id, board_id = 293, 16017
else:
    msm_id, board_id = 293, 16027
# QCDT: magic(4) version(4) num_entries(4) 然后每条 20 字节
# QCDT 布局：magic(4) + version(4) + num_entries(4) + entry[20]
# entry = platform_id(msm_id) + variant_id + soc_rev + offset + size
# offset 是相对文件头的数据偏移 = 12 + 20*n
hdr_len = 12
entry_len = 20
data_off = hdr_len + entry_len   # 单条目时 = 32
hdr = b'QCDT' + struct.pack('<II', 1, 1)
entry = struct.pack('<IIIII', msm_id, 0, 0, data_off, len(dtb))
open(out_path, 'wb').write(hdr + entry + dtb)
print(f"    QCDT: msm_id={msm_id} board_id={board_id} dtb={len(dtb)}B")
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
  else
    ui_print "  未生成 SPLITIMG/dt，跳过替换"
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
# 插在 dump_boot; 这一行之后（dump 完才有 SPLITIMG/dt）
s = s[:m.end()] + '\n. ./ak3-oppo-replace-dt.sh\n' + s[m.end():]
open(p, 'w').write(s)
print('  已在 dump_boot 之后插入 dt 替换钩子')
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
