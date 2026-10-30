# R9s 6.12 内核刷机与日志抓取操作卡

> 目标机型：OPPO R9s **16027**（JDI R63452 面板）
> 当前可用包：CI run `41008352805` → `r9s-android-6.12-6.12.52-g9b319f4ef9b2-*.zip`

---

## 第 1 步：刷之前必须先备份

**这一步不能省。** 卡屏时只有备份能让你立刻回到能用的状态。

TWRP → Backup → 只勾 **Boot** → 滑动作业。

备份完成后顺便确认一下机型，在 TWRP 终端里执行：

```bash
cat /proc/device-tree/qcom,board-id | od -An -tx1
```

应该能看到 `... 00 08 00 00 3e 9b ...`（`3e9b` = 16027 = r9sk）。
刷机包会根据这个值自动选 dtb，读到 `3e9b` 就用 `r9sk.dtb`。

---

## 第 2 步：刷入

TWRP → Install → 选那个 zip → 滑动刷入。

刷完之后界面上应该会出现一行：

```
board-id=...3e9b... -> msm8953-oppo-r9sk.dtb
```

**看到这行说明 dtb 选对了。** 如果没看到这行，先不要重启，把界面截图发出来。

---

## 第 3 步：重启，然后分三种情况

### 情况 A：正常开机进系统

意外之喜。立刻在系统里收集：

```bash
dmesg > /sdcard/dmesg.txt
cat /sys/class/power_supply/*/uevent > /sdcard/power.txt
```

### 情况 B：还是卡在 logo 一屏

**不要马上重刷。** 按下面做：

1. 长按电源键强制关机
2. 音量上 + 电源 进 TWRP
3. TWRP → Mount → 勾选 **Data**（如果 Data 解不开就跳过这步）
4. TWRP → Advanced → Terminal，执行：

```bash
ls -la /sys/fs/pstore/
```

**这一步的结果很关键，分两种：**

#### B-1：`/sys/fs/pstore` 里有 `console-ramoops-0`

说明 6.12 内核**跑起来了**，只是卡在某个子系统。直接：

```bash
cat /sys/fs/pstore/console-ramoops-0
```

把整份内容发出来。里面有 6.12 自己的完整 dmesg，
能直接看到卡在显示、时钟还是供电。

> 判断方法：看第一行的 `Linux version`。
> 如果是 `6.12.52-...` 就是我的内核 —— 有救了。
> 如果还是 `4.9.337-lineageos` 说明是旧数据，请看 B-2。

#### B-2：里面还是 4.9 的旧日志，或者完全是空的

说明 6.12 内核死在 pstore 初始化之前（比 `postcore_initcall` 还早）。
这种情况下：

```bash
# 把旧的也存下来，至少能确认确实没被覆盖
cat /sys/fs/pstore/console-ramoops-0 > /sdcard/pstore-old.txt
```

然后告诉我"还是 4.9 的日志"，我会换手段 —— 见文末。

### 情况 C：直接进 fastboot

说明 bootloader 阶段就失败了。这种情况请提供：

```bash
# 在 fastboot 模式下
fastboot getvar all 2>&1
```

以及确认你刷的是 `r9sk` 的包。

---

## 一些容易踩的坑

| 坑 | 说明 |
|---|---|
| 用 ADB sideload 刷 | 建议用 TWRP 直接 Install 选 zip，sideload 有时不触发 board-id 检测 |
| 刷完没清 cache | 本内核不改 ramdisk，通常不用清；但如果反复卡同一点，试一次清 cache |
| 拿 `Image.gz` 而非 zip | 别手动刷 `.img`，zip 里已经拼好了 `Image.gz-dtb`，手动刷会丢 dtb |
| 抓日志时没 mount Data | `/sys/fs/pstore` 是内核的，不需要 mount Data 也能读 |

---

## 已修复但未验证的四个问题

按"死在哪"排序（越靠前越早死）：

| # | 问题 | 死在哪 | 修复 |
|---|---|---|---|
| 1 | Image `text_offset` 是 0 | 进入内核第一条指令前 | `.quad 0x80000`（CAF 约定） |
| 2 | `VA_BITS=52` | `head.S` 建页表阶段 | 改 39（Cortex-A53 上限） |
| 3 | ramoops 地址错 | pstore 写入无人读的区域 | 改 `0xb0000000` |
| 4 | pstore 没通电 | 黑匣子静默失效 | `PSTORE_RAM/CONSOLE/PMSG` 全 `=y` |

前两个是"烧进去必死"级别。第 3、4 个不致命，但它们决定了**出问题时你能否看到日志** —— 这就是为什么前两次卡屏我什么都拿不到。

---

## 如果这次还是失败

那就只剩两条路，我会在下一轮准备：

1. **最小诊断内核**：只留串口 + pstore + 最小 ARM64 核心，
   把显示、音频、充电、GPU 全关掉。目的是"先证明能起"，
   再一项一项加回来定位。
2. **串口**：R9s 主板有 UART 测试点，需要拆机。这是最后手段。

告诉我 B-1 还是 B-2，我就能决定走哪条。
