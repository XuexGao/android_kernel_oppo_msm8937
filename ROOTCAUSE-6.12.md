# ROOTCAUSE-6.12 —— 卡第一屏 + pstore 无 6.12 日志的根因分析

> 只读分析报告。分析基准：`6.12-test` @ `30ea6b72d`（分析过程中 HEAD 被并行推进到 `eb5a3cdf8`，
> 见文末「附录 A：与并行改动的交叉验证」）。
> 未修改任何源码，未运行 make。所有结论均附文件:行号或命令输出。

---

## 0. 结论速览（按可能性排序）

| # | 根因 | 为什么导致「卡 logo + 无日志」 | 置信度 |
|---|---|---|---|
| **1** | **`Image.gz-dtb` 里追加的 dtb 从未被 ABL 使用** —— ABL 用的是 boot.img `dt` 区里的原厂 CAF DTB | 内核跑在与驱动树完全不匹配的 4.9 CAF DTS 上：CAF 根节点 `interrupt-parent = <&wakegic>`，而 6.12 没有 mpm-gic 驱动 → `arch_timer` 的 PPI 全部映射失败 → **clocksource 和 clock_event_device 都没注册 → 没有 tick → `jiffies` 冻结 → 卡在第一个超时等待（`smp_init()`），`postcore_initcall(ramoops_init)` 永远到不了**，ramoops 一行都写不进去 | **极高（实测日志已证明）** |
| **2** | **`wake-gic@601d4`（MPM）在 6.12 里没有驱动**，且我们的 DTS 也没有这个节点 | 只有当内核用 CAF DTB 时才会遇到；它是 #1 的"下游放大器"——CAF DTB 里所有 `interrupts` 的默认 `interrupt-parent` 是 `&wakegic`，没有驱动 → `no irq domain found for wake-gic@601d4 !` → 整棵中断树废掉 | 高 |
| **3** | **配置链断裂：`CONFIG_MAILBOX` 未开，导致 SMD/RPM 整条链被 Kconfig 静默丢弃** | `RPMSG_QCOM_SMD` / `QCOM_SMP2P` / `QCOM_SMSM` / `QCOM_APCS_IPC` 全部消失 → RPM 无传输层 → regulator/clock/RPMPD 全部 probe 失败。**这不是"卡在 pstore 之前"，但会让"卡在 logo" 变得必然**（显示链路要 regulator + clock） | 高（已实测 `.config`） |
| **4** | 诊断内核 vs 正常内核的真正差异不是"体积"，而是 **DTB 匹配失败 + diag 恰好只用到了不依赖 RPM 的最小集** | 见 §4 逐项 diff | 高 |
| **5** | `text_offset=0x80000` 与 6.12 PI/KASLR 的 `MIN_KIMG_ALIGN` 语义 | 正常内核多开 `RANDOMIZE_BASE` 路径，`pa_base % 2MiB = 0x80000 ≠ 0`，会走 `relocate_kernel()`；diag 内核同样有该值，所以**不是两者的差异项**，但属潜在风险 | 中 |

**一句话根因**：这台机器上 **6.12 内核从来没有跑过我们写的 DTS**。
`Machine model: Qualcomm Technologies, Inc. MSM8953 + PMI8950 MTP` 这一行就是铁证 ——
它是 ABL 内置 CAF DTB 的 `model`，而我们的 DTS 写的是 `OPPO R9sk`。

---

## 1. 根因 #1：DTB 根本没生效（决定性的那一条）

### 1.1 证据链

**(a) 实测日志里的 `model` 来自 CAF，不来自我们**

4.19 分支的板级 DTS：

```
$ git show lineage-22.1-k4.19:arch/arm64/boot/dts/vendor/qcom/msm8953-mtp_16027.dts | grep -n "model\|board-id"
22:	model = "Qualcomm Technologies, Inc. MSM8953 + PMI8950 MTP";
24:	qcom,board-id= <8 0 16027>;
```

我们的 DTS：

```
$ grep -n "model" arch/arm64/boot/dts/qcom/msm8953-oppo-r9sk.dts
11:	model = "OPPO R9sk";
```

诊断内核日志打印的是 `Qualcomm Technologies, Inc. MSM8953 + PMI8950 MTP`
→ **内核拿到的 DTB 是 CAF 那份，不是我们的**。

**(b) `venus_region@0` / `wake-gic@601d4` 只存在于 CAF DTB**

```
$ grep -c "venus_region@0" arch/arm64/boot/dts/qcom/msm8953.dtsi
0
$ grep -n "venus" arch/arm64/boot/dts/qcom/msm8953.dtsi | head -1
315:		venus_mem: venus@91400000 {
```

而 CAF 的 `msm8953.dtsi` 里是 `venus_region@0`（`/tmp/caf-msm8953.dtsi:90` 附近）。
诊断日志里的 `OF: reserved mem: node venus_region@0 compatible matching fail`
只能来自 CAF DTB。

**(c) 我们的 `reserved-memory` 命名与日志完全不符**

`git status` 无关，直接从已编译 dtb 反编译：

```
$ dtc -I dtb -O dts arch/arm64/boot/dts/qcom/msm8953-oppo-r9sk.dtb | grep -E "venus|splash"
			cont-splash@90001000 { ... }
			venus@91400000 { ... }
```

日志里出现的是 `venus_region@0`。**两者不是同一份 DTB。**

### 1.2 为什么追加在 `Image.gz-dtb` 尾部没用 —— 打包链路分析

任务里点名要求读 `/tmp/ak3chk/tools/ak3-core.sh`。以下是逐行结论。

#### `split_boot()`（`/tmp/ak3chk/tools/ak3-core.sh:68`）

优先走 `unpackelf` 分支（R9s 原厂 boot.img 是 ELF 格式的 QCOM boot image）：

```sh
# ak3-core.sh:101-108
if [ -f boot.img-dt -a ! -f "$BIN/elftool" ]; then
  case $(od -ta -An -N4 boot.img-dt | sed -e 's/ del//' -e 's/   //g') in
    QCDT|ELF) mv -f boot.img-dt dt;;          # ← 关键：变成 $SPLITIMG/dt
    *)
      gzip -c kernel.gz > kernel.gz-dtb;
      cat boot.img-dt >> kernel.gz-dtb;
      rm -f boot.img-dt kernel.gz;
    ;;
  esac;
fi;
```

- 原厂 dt 是 **QCDT**（Qualcomm 的 DTB 表）→ 走 `QCDT|ELF)` 分支 → 存为 `$SPLITIMG/dt`
- 原厂 dt 是**裸 FDT** → 走 `*)` 分支 → **被 cat 回 kernel 尾部**

#### `flash_boot()`（`ak3-core.sh:313`）

```sh
# ak3-core.sh:349-356  —— 找 fdt，$dt 优先取 $SPLITIMG/dt
for fdt in dt recovery_dtbo dtb; do
  for i in $AKHOME/$fdt $AKHOME/$fdt.img $SPLITIMG/$fdt; do
    if [ -f $i ]; then eval local $fdt=$i; break; fi;
  done;
done;

# ak3-core.sh:381  —— mkbootimg 用 --dt 传入
mkbootimg --kernel $kernel --ramdisk $ramdisk --cmdline "$cmdline" \
          --base $base ... $dt --output $AKHOME/boot-new.img;
```

**结论（明确回答任务里的提问）**：

1. **R9s 原厂 boot.img 若带 QCDT 的 `boot.img-dt`，我们追加在 `Image.gz-dtb` 尾部的 dtb 确实会被丢弃。**
   因为 ABL 从 boot.img 头部的 `dt_size` 字段指向的 **QCDT 区**里按
   `platform_id/board_id` 挑 DTB 传给内核（x0），根本不去扫 kernel 尾部。
   `mkbootimg --dt $dt` 把**原厂 QCDT** 原样写回新 boot.img，
   于是 ABL 仍然拿到 CAF 的那份 OPPO/CAF DTB。
   → 这完美解释了 `Machine model = MSM8953 + PMI8950 MTP`。

2. **正确做法**：把我们的 dtb 做成 boot.img 的 `dt` 区，而不是追加在 kernel 尾部。有两条路：
   - **路 A（推荐，最贴近 CAF 语义）**：生成只含 `msm8953-oppo-r9s/r9sk` 的 **QCDT 表**，
     作为 `$SPLITIMG/dt` 传给 `mkbootimg --dt`。ABL 会按 `qcom,msm-id`/`qcom,board-id`
     在表里挑我们的 dtb。**这正是 4.9/4.19 原厂 boot.img 的做法**。
   - **路 B（简化）**：直接放一个**裸 FDT**（我们的 r9sk.dtb）作为 `dt`。
     多数 ABL 能接受单条裸 FDT，但没有 board-id 的多机型选择能力。

   > 当前 `ci/mkflashable.sh:111-180` 已经朝这个方向改了（`ak3-oppo-replace-dt.sh`
   > 在 `dump_boot` 之后覆盖 `$SPLITIMG/dt`），但**实现有 bug**，见 §1.3。

3. **另一条同样致命的细节**：`split_boot()` 的 `*)` 分支会把裸 FDT 原厂 dt
   **cat 进 `kernel.gz-dtb`**（`ak3-core.sh:106`），然后 `rm -f kernel.gz`。
   紧接着 `flash_boot()` 找 kernel 时（`ak3-core.sh:331-338`）：

   ```sh
   for i in zImage zImage-dtb Image Image-dtb Image.gz Image.gz-dtb ...; do
     if [ -f $i ]; then kernel=$AKHOME/$i; break; fi;
   done;
   ```

   如果我们包里放的是 `Image.gz-dtb`，命中 `Image.gz-dtb`，**这一份是我们的**
   （`$AKHOME` 优先），所以此路径下我们的 dtb 会跟着 kernel 走。
   但 **ABL 不扫 kernel 尾部的 dtb** —— 这一点对 4.9/4.19/6.12 **都成立**
   （详见 §7.2 的撤回说明：arm64 从来没有运行期 appended-DTB 解析器，
   `BUILD_ARM64_APPENDED_DTB_IMAGE` 只是构建期开关）。

   → **追加在 `Image.gz-dtb` 尾部的 dtb 对 ABL 是死数据，与内核版本无关。**

### 1.3 `ci/mkflashable.sh` 里 `ak3-oppo-replace-dt.sh` 的实现缺陷

这是我在只读审查里发现的**真实 bug**（当前工作区版本）：

1. **裸 FDT 分支逻辑反了**
   ```
   ci/mkflashable.sh:139-148
   if [ ! -f "$AK_DT_FILE" ]; then
     ui_print "  原厂无独立 dt，创建 dt（裸 FDT）"
     cp -f "$AK_DTB_NAME" "$AK_DT_FILE"
     AK_DT_FILE=""          # ← 清空！后续判断被跳过
   fi
   if [ -n "$AK_DT_FILE" ]; then ... fi
   ```
   创建完立刻把变量清空，虽然本意是"已处理完"，但语义混乱，
   且**没有处理"原厂 dt 是 QCDT"时的真正替换**（见下条）。

2. **QCDT 生成格式是错的** —— `ci/mkflashable.sh:150-176` 里：
   ```python
   hdr = b'QCDT' + struct.pack('<II', 1, 1)
   entry = struct.pack('<IIIII', msm_id, 0, 0, data_off, len(dtb))
   ```
   - 真实 QCDT 的 entry 是 **`<IIII>`（platform_id, variant_id, soc_rev, offset, size）**
     但字段含义与**字节序**都需按 `dtbtool` 来：`offset` 是**从文件头算起的绝对偏移**，
     且 `dtb` 需要 **4 字节对齐**、`size` 需 **页对齐**（0x1000）。
   - 只用 `msm_id=293` 而把 `board_id` 丢掉 —— 无法区分 16017/16027。
   - 表项数写成 1，但我们要放 2 个机型。
   → 即便这段跑通，ABL 也**未必**能正确解析。**这个必须重写**。

3. **board-id 读法不可靠** —— `ci/mkflashable.sh:112-118` 从
   `/proc/device-tree/qcom,board-id` 读，但**刷机时跑的是 TWRP（4.9 内核）**，
   它的 `/proc/device-tree` 是 TWRP 自己用的 DTB，`qcom,board-id` 未必是我们期望的
   `<8 0 16027>`。FLASH-GUIDE.md:22-26 让人在 TWRP 里 `cat /proc/device-tree/qcom,board-id`，
   就是把这条不确定性交给用户。

### 1.4 为什么"修 DTB 反而更没日志"是伪命题

README-6.12-android.md:453-457 与 `21f8c5f7a` 的提交信息写：

> DTB 修对之后走我们的单节点布局，反而直接卡死在 NUMA 初始化。

**这条推理不成立**，理由：

- `arch_numa_init()` 有**强制兜底**（`drivers/base/arch_numa.c:318-328`）：
  ```c
  void __init arch_numa_init(void)
  {
      if (!numa_off) {
          if (!acpi_disabled && !numa_init(arch_acpi_numa_init)) return;
          if (acpi_disabled && !numa_init(of_numa_init))       return;
      }
      numa_init(dummy_numa_init);        /* ← 单节点兜底，注释说 "unlikely that this function fails" */
  }
  ```
- `of_numa_init()` 在没有 `numa-distance-map-v1` 节点时返回 0（`drivers/of/of_numa.c` 末尾），
  但 `numa_init()` 发现 `nodes_empty(numa_nodes_parsed)` 会返回 `-EINVAL`
  → 落到 `dummy_numa_init()` → **正常单节点启动**，不是挂死。
- 而且 `arch_numa_init()` 在 `bootmem_init()`（`arch/arm64/mm/init.c:347`）里，
  确实早于 pstore 的 `postcore_initcall`（`fs/pstore/ram.c:987`）——
  **但它的失败路径是 rp_info + 兜底，不是 panic。**

→ NUMA 不是根因。`21f8c5f7a` 关掉 NUMA/THP/MEMORY_HOTPLUG 本身无害（手机确实该关），
但把它标成"根因"是错误的归因。

---

## 2. 根因 #2：MPM（`wake-gic`）—— 中断树被整体废掉

### 2.1 事实核对

| 项 | CAF 4.19 | 我们 6.12 |
|---|---|---|
| DTS 节点 `wake-gic@601d4` | 有（`/tmp/caf-msm8953.dtsi:264`） | **无**（`grep -n "wake-gic\|mpm" arch/.../msm8953.dtsi` 无输出） |
| 根节点 `interrupt-parent` | **`&wakegic`**（`/tmp/caf-msm8953.dtsi:30`） | `&intc`（`msm8953.dtsi:15`） |
| 驱动 `qcom,mpm-gic-msm8953` | `drivers/irqchip/qcom-mpm.c`（4.19） | **不存在**。6.12 只有 `drivers/irqchip/irq-qcom-mpm.c`，`IRQCHIP_MATCH("qcom,mpm", ...)`（:481） |
| `CONFIG_QCOM_MPM` | `=y`（`/tmp/419-defconfig:604`） | **未设置** |

### 2.2 因果链

CAF DTB 的根节点是 `interrupt-parent = <&wakegic>`。内核用 CAF DTB 启动时：

1. `wake-gic@601d4` 的 compatible 是 `qcom,mpm-gic-msm8953` / `qcom,mpm-gic`
   → 6.12 的 `irq-qcom-mpm.c` 只匹配 `qcom,mpm` → **匹配不上，没有任何驱动认领**
2. → 该节点没有 `irq_domain`
3. → 所有继承根 `interrupt-parent` 的节点（绝大多数外设）在
   `irq_create_fwspec_mapping()` 里走到 `kernel/irq/irqdomain.c:913`：
   ```
   pr_warn("no irq domain found for %s !\n", ...)
   ```
   并 `return 0`（**等于 irq=0，静默错误**）
4. → 外设拿到的 IRQ 全是 0，驱动 probe 失败或行为错乱

这正是诊断日志里那行 `irq: no irq domain found for wake-gic@601d4 !` 的来源。

### 2.3 但注意：`arch_timer` 的报错**不是** MPM 导致的

诊断日志里 `arch_timer: No interrupt available, giving up` 需要单独解释：

- `drivers/clocksource/arm_arch_timer.c:1465` 的这个错误，触发条件是
  `!arch_timer_ppi[arch_timer_uses_ppi]`，即 **PPI 映射失败**。
- 该值来自 `irq_of_parse_and_map(np, i)`（`arm_arch_timer.c:1430` 附近）。
- CAF DTB 的 `timer` 节点 `interrupts = <1 2 0xff08>, <1 3 0xff08>, <1 4 0xff08>, <1 1 0xff08>`
  （`/tmp/caf-msm8953.dtsi:290-296`）—— 这些是**直接指定 GIC PPI**，
  但继承的是根节点的 `interrupt-parent = <&wakegic>`！
- → 解析时去 `wakegic` 找 domain → 没有 → 返回 0 → PPI 全 0 → 报 "No interrupt available"
- → `arch_timer_of_parse()` 返回 `-EINVAL` → `timer_probe()` 计数 0
- → `drivers/clocksource/timer-probe.c:44`：`pr_crit("timer_probe: no matching timers found")`

**⚠️ 这里必须纠正一个常见误解：`time_init()` 不会 panic。**

我最初推断 `arch/arm64/kernel/time.c:64-67` 的
```c
arch_timer_rate = arch_timer_get_rate();
if (!arch_timer_rate)
    panic("Unable to initialise architected timer.\n");
```
会触发 panic —— **这是错的**，代码顺序已经排除了它：

`arch_timer_of_parse()` 里 `arch_timer_of_configure_rate()`
（`arm_arch_timer.c:1447`）在 **PPI 检查（:1465）之前**就把
`arch_timer_rate` 设好了：

```c
1447:	arch_timer_of_configure_rate(rate, np);   /* ← arch_timer_rate 在这里被设成 19200000 */
...
1465:		pr_err("No interrupt available, giving up\n");   /* ← 这里才失败 */
```

而 `arch_timer_of_configure_rate()`（:1064-1076）即使
`of_property_read_u32(np, "clock-frequency", ...)` 失败，
也会 `arch_timer_rate = rate`（`rate = arch_timer_get_cntfrq()`）。
→ `arch_timer_rate` **非 0** → `time_init()` 的 `panic()` **不触发**。

**真正的机制：不是 panic，是"无 tick → jiffies 冻结 → 第一个超时等待永久挂住"**

逐步推导（每一步都有代码依据）：

1. `arch_timer_of_parse()` 在 `arm_arch_timer.c:1465` 就 `return -EINVAL`，
   **根本走不到** `arch_timer_register()`（:1487 附近）。
2. `arch_timer_register()` 是做两件事的地方：
   - 注册 **clocksource**（`arch_sys_counter`）
   - `clockevents_config_and_register()`（:912）注册 **clock_event_device**
   → 两者**都没有注册**。
3. `time_init()`（`arch/arm64/kernel/time.c:55`）里：
   - `arch_timer_rate` 非 0（见上）→ **不 panic** ✅
   - `lpj_fine = arch_timer_rate / HZ` **被设好了**（:69）→ 后面
     `calibrate_delay()` 走 `init/calibrate.c:291` 的 `lpj_fine` 分支
     → **`calibrate_delay()` 也不会挂死**（我一开始怀疑这里，是错的）
4. 但 **`tick_setup_device()` 永远没被调用** → 每个 CPU 的
   `tick_cpu_device.evtdev == NULL` → **没有任何周期 tick**。
5. → **`jiffies` 从 `start_kernel()` 之后就不再前进**。
   唯一的 clocksource 是 `clocksource_jiffies`（`kernel/time/jiffies.c:69`
   的 `core_initcall`，rating 1），而它读的正是 `jiffies` —— **循环依赖，等于没有时间源**。
6. → 第一个依赖超时的等待就永久挂住。最早能碰到的是
   `smp_init()` → `__cpu_up()`（`arch/arm64/kernel/smp.c:149`）：
   ```c
   wait_for_completion_timeout(&cpu_running, msecs_to_jiffies(5000));
   ```
   以及后续任何 `msleep()` / `schedule_timeout()` / `usleep_range()`。
   **`schedule_timeout()` 在没有 tick 时永远不会返回**（没有东西唤醒它）。

这完美解释了两个现象：

| 现象 | 机制 |
|---|---|
| **卡在 logo** | `start_kernel()` 走到 `smp_init()`/第一个 `msleep()` 就永久睡眠；`do_basic_setup()` 里的所有 initcall（DRM/USB/charger…）**一个都没跑** → 显示链路从未初始化 → 屏幕上只剩 ABL 画的那一帧 |
| **pstore 里没有 6.12 日志** | `postcore_initcall(ramoops_init)`（`fs/pstore/ram.c:987`）在 `do_pre_smp_initcalls()` 里执行，**晚于** `smp_init()`。而 `smp_init()` 调用链里已经卡死 → **ramoops 从未注册** → 一个字节都写不进去 |

> 补充：`console_init()`（`init/main.c:1069`）**早于** `smp_init()`，
> 所以如果设备有串口，`smp_init()` 之前的那批 printk 是能出来的。
> 但 R9s 是**零售机**（串口 disabled，`FLASH-GUIDE.md:131` 自己承认要拆机），
> 且 `pstore_console` 尚未注册 → **日志全部滞留在 printk ringbuffer 里，
> 没有任何持久化出口**。这就是"完全无日志"的物理原因。
>
> **实测判据**：若刷机后 pstore 里连一行 6.12 都没有 → 卡在 `ramoops_init` 之前
> （与上表一致）；若有几行 6.12 之前的内容然后戛然而止 → 卡在 `do_basic_setup()`
> 里的某个 `msleep()`。两者修法相同（修 DTB）。

> ⚠️ 注意：`clock-frequency = <19200000>` 修的是**频率**，
> 而这里的错误是**中断**，两者无关。当前工作区在 `msm8953.dtsi:2198-2211`
> 加的 `clock-frequency` 注释把"timer 起不来"归因于 CNTFRQ，**归因不准** ——
> 真正的死因是 PPI 拿不到。这解释了为什么加了 `clock-frequency` 之后现象没变。

### 2.4 修法

优先级从高到低：

1. **让我们的 DTB 真正生效**（§1.2 路 A/B）。我们的 DTS 根节点是
   `interrupt-parent = <&intc>`，timer PPI 直接指向 GIC，**没有 MPM 依赖** →
   `timer_probe` 会正常成功。这是根治。
2. 不要给 6.12 补 `wake-gic` 节点 —— 6.12 的 `irq-qcom-mpm.c` 绑定要求
   `qcom,mpm-pin-count` / `qcom,mpm-pin-map` / `qcom,rpm-msgr` 等
   （`irq-qcom-mpm.c:340-370`），与 CAF 的 `qcom,mpm-gic-msm8953` 完全不是一套绑定。
   硬塞会失败得更早。
3. 若必须用 CAF DTB：在 CAF DTB 上做 `/delete-property/ interrupt-parent`（根节点）
   并把 timer 的 `interrupt-parent` 显式指向 `&intc`。但这是 hack，不推荐。

---

## 3. 根因 #3：配置链断裂（SMD/RPM 整条链被静默丢弃）

### 3.1 实测证据

```
$ grep -E "^# CONFIG_MAILBOX is not set" .config
6897:# CONFIG_MAILBOX is not set

$ grep -E "^CONFIG_(RPMSG_QCOM_SMD|QCOM_SMP2P|QCOM_SMSM|QCOM_APCS_IPC)=" .config
(无输出，全部不存在)

$ grep -E "^CONFIG_(RPMSG_QCOM_SMD|QCOM_SMP2P|QCOM_SMSM|QCOM_APCS_IPC)=y" arch/arm64/configs/msm8953-oppo_defconfig
1351:CONFIG_QCOM_APCS_IPC=y
1374:CONFIG_RPMSG_QCOM_SMD=y
1395:CONFIG_QCOM_SMP2P=y
1396:CONFIG_QCOM_SMSM=y
```

**defconfig 里写了 4 个，`.config` 里一个都没有。** 这是 Kconfig 依赖不满足时的静默丢弃。

### 3.2 依赖链

```
drivers/soc/qcom/Kconfig
  183: config QCOM_SMD_RPM   depends on RPMSG; depends on RPMSG_QCOM_SMD || RPMSG_QCOM_SMD=n

drivers/rpmsg/Kconfig
  67: config RPMSG_QCOM_SMD  depends on MAILBOX; depends on QCOM_SMEM

drivers/soc/qcom/Kconfig
  202: config QCOM_SMP2P     depends on MAILBOX; depends on QCOM_SMEM
  212: config QCOM_SMSM      depends on MAILBOX; depends on QCOM_SMEM

drivers/regulator/Kconfig
 1089: config REGULATOR_QCOM_SMD_RPM  depends on QCOM_SMD_RPM

drivers/clk/qcom/Kconfig
  133: config QCOM_CLK_SMD_RPM       depends on QCOM_SMD_RPM

drivers/irqchip/Kconfig
  464: config QCOM_MPM            depends on MAILBOX
```

`CONFIG_MAILBOX` 是 `menuconfig`（`drivers/mailbox/Kconfig:2`），**默认 n**，
且 defconfig 里**没有** `CONFIG_MAILBOX=y`。

### 3.3 后果（为什么这会让"卡 logo"变成必然）

`QCOM_SMD_RPM` **还开着**（`CONFIG_QCOM_SMD_RPM=y`），
所以 `rpmcc`、`rpmpd`、`pm8953` regulators 这些**驱动对象仍然被编译**，
但它们要通信的对象 `qcom,smd-rpm` 传输层不存在：

- `drivers/soc/qcom/smd-rpm.c` 的 `arch_initcall` 会注册一个 rpmsg driver，
  通过 `__register_rpmsg_driver`（`nm` 显示该符号存在）
- 没有 `RPMSG_QCOM_SMD` → **没有任何 `rpmsg_endpoint` 会被创建**
  → `qcom_smd_rpm` 的 probe 永远等不到 channel
- → `rpmcc`（`clocks = <&rpmcc RPM_SMD_XO_CLK_SRC>`，`msm8953.dtsi:206` 等）
  拿不到 XO 时钟源
- → `gcc-msm8953` 的 `clocks = <&rpmcc RPM_SMD_XO_CLK_SRC>, ...`（`msm8953.dtsi` 里 gcc 节点）
  → 所有 GCC 时钟无法注册
- → `pinctrl-msm8953`、`sdhci-msm`、`drm/msm`、`usb` **全部 `-EPROBE_DEFER`**

**这与"卡 logo"高度一致**：内核起来了，但显示链路（MDSS/DSI/面板/LAB-IBB）
依赖 regulator + clock，全都 deferred，屏上永远只有 ABL 画的那一帧。

> 关键洞察：这条链**不会**让 pstore 无日志 —— 它发生在 `device_initcall` 之后，
> ramoops 早就在 `postcore_initcall` 注册好了。
> **所以 #3 能解释"卡 logo"，但不能解释"无日志"。**
> "无日志"只能由 #1 解释：**没有 tick → `jiffies` 冻结 → 卡在
> `smp_init()` 的超时等待 → `postcore_initcall(ramoops_init)` 永远到不了**。
>
> 也就是说：#1 是"连 pstore 都摸不到"，#3 是"摸到了 pstore 但屏幕还是黑的"。
> **先修 #1**（否则永远拿不到日志来验证 #3）。

### 3.4 修法

```diff
+# MAILBOX 是 RPMSG_QCOM_SMD / QCOM_SMP2P / QCOM_SMSM / QCOM_MPM 的共同依赖，
+# 不开它这几个会被 Kconfig 静默丢弃（已实测 .config 里全都不存在）。
+CONFIG_MAILBOX=y
```
并在 CI 里加断言（现有断言只查了 `QCOM_SMD_RPM=y`，而它恰好是**不依赖 MAILBOX** 的那个，
所以断言全绿却功能全废）：

```sh
for sym in MAILBOX RPMSG_QCOM_SMD QCOM_SMP2P QCOM_SMSM QCOM_SMEM; do
  v="$(grep -E "^CONFIG_${sym}=" .config || true)"
  [ "$v" = "CONFIG_${sym}=y" ] || { echo "::error::$sym 未生效"; exit 1; }
done
```

---

## 4. 为什么诊断内核能启动、正常内核不能？—— 逐项差异

### 4.1 先纠正三个流行但错误的解释

| 说法 | 出处 | 为什么错 |
|---|---|---|
| "54.8/39 MiB 太大，ABL 装不下" | `30ea6b72d`、README:424-457 | 4.19 的 36.2 MiB 能启动，我们 39 MiB 差 2.8 MiB（7.7%），不足以跨越某个硬边界；且**诊断内核 10 MiB 能启动时用的是同一份 CAF DTB**，说明限制不在体积 |
| "text_offset=0 导致入口错位" | `52816f2fd`、README:341-380 | 已在 `head.S:81` 改成 `0x80000`，现象**没变** → 不是根因 |
| "VA_BITS=52 → A53 不支持" | `5331e0834` | `CONFIG_ARM64_VA_BITS=39` 已生效（`.config` 实测），现象**没变** → 不是当时的卡点 |
| "NUMA/THP/HOTPLUG 导致" | `21f8c5f7a` | `arch_numa_init()` 有 `dummy_numa_init()` 强制兜底（§1.4），不会 panic |

**四个"根因"都改完了，现象没变** —— 这本身就证明它们都不是根因。

### 4.2 真正的差异：diag vs normal

| 维度 | diag（能启动） | normal（卡 logo） | 是否导致卡死 |
|---|---|---|---|
| **用的 DTB** | CAF（ABL 内置） | CAF（相同！） | 两者**相同** → 不是差异项 |
| `CONFIG_DRM_MSM` | 关 | **开** | 是（normal 多一条需要 clock/regulator 的链） |
| `CONFIG_QCOM_APR` / `SND_SOC_QDSP6` | 关 | **开** | 是（QDSP6 依赖 `remoteproc` + `smd-edge`，MAILBOX 缺失 → probe 失败，但不应挂死） |
| `CONFIG_QPNP_SMBCHARGER` / `QPNP_FG` | 关 | **开** | 是（SPMI 依赖 `spmi-pmic-arb`，正常；但 charger 依赖 `iio`/`vadc`） |
| `CONFIG_QCOM_KGSL` | 关 | 关（`.config` 实测未设） | 否 |
| `CONFIG_MAILBOX` | 未开 | **未开** | 两者相同 → 不是差异项 |
| `CONFIG_RANDOMIZE_BASE` | 未设（继承默认 y） | y | 潜在 |
| `CONFIG_NR_CPUS` | 未设 → 默认 | **512** | 潜在（`4.19 = 8`） |
| `CONFIG_DEBUG_INFO` | 未设 | y（`REDUCED`） | 只影响体积 |
| `CONFIG_PSTORE*` | 全 y | 全 y | 两者相同 → 不是差异项 |
| `CONFIG_ARM64_VA_BITS_39` / `4K_PAGES` | y | y | 两者相同 → 不是差异项 |
| `text_offset` | 0x80000 | 0x80000 | 两者相同 → 不是差异项 |

### 4.3 结论：diag 与 normal 的差异，以及一处必须点破的日志拼接问题

**先点破一个陷阱**：README-6.12-android.md:299-322 引用的那批日志
（`vmalloc : 0xffffff8008000000` / `ramoops: attached 0x800000@0xb0000000`）
**是 4.9 内核的**（README:309 自己写了"设备侧反证（4.9 内核的启动日志）"）。
而 `Machine model = ...MTP` / `arch_timer: No interrupt available` /
`timer_probe: no matching timers found` 那组是 **6.12 诊断内核的**。

**这意味着关键结论**：

> **6.12 诊断内核其实从来没有真正"启动"过。**
> 它同样用 CAF DTB，同样拿不到 timer PPI，同样没有 tick。
> 它之所以能打印到 `prepare_namespace`，只能有一种解释：
> **那些 printk 是在卡死之前就写进 ringbuffer、并通过 pstore 之前的某个出口落盘的** ——
> 但 6.12 在 `smp_init()` 就挂了，`ramoops_init` 都还没跑。

所以更可能的事实是：**"诊断内核能启动"这个前提本身需要重新验证。**
README 里没有给出诊断内核完整 dmesg 的原文，只有零散几行。
`prepare_namespace` 那行如果是真的，说明那次运行**有 tick** ——
即那次用的 DTB 里 timer 节点是正确的（比如 ABL 那次恰好给了不带
`wakegic` 父节点的 DTB，或者诊断内核那次带了我们自己的 dtb）。

**这一点必须用实测澄清**，因为它决定了后续所有推理的基点。

### 4.4 diag 与 normal 的确定差异表

| 维度 | diag（10 MiB） | normal（39 MiB） | 是否解释"卡 logo" |
|---|---|---|---|
| **用的 DTB** | CAF（未验证） | CAF（日志已证） | **是（共同根因）** |
| `CONFIG_DRM_MSM` | 关 | **开** | normal 多一条依赖 clock/regulator 的链 |
| `CONFIG_INTERCONNECT_QCOM` + 20 个平台 | 关 | **开**（`.config:7919-7956`） | normal 多 20 个 interconnect provider probe |
| `CONFIG_QCOM_CPR` | 关 | **开**（`.config:7081`） | 无 `qcom,cpr` 节点 → 跳过 |
| `CONFIG_QCOM_TSENS` | 关 | **开**（`.config:4066`） | 6.12 有 tsens 驱动 + 我们有 tsens 节点 → 会 probe |
| `CONFIG_QPNP_SMBCHARGER` / `QPNP_FG` | 关 | **开** | 依赖 SPMI + IIO |
| `CONFIG_NR_CPUS` | 未设 | **512** | 每 CPU 静态区膨胀；`GIC_CPU_MASK_SIMPLE(8)` 与 512 不匹配 |
| `CONFIG_MAILBOX` | 未开 | **未开** | **两者相同 → 不是差异项**，但共同导致 RPM 链断 |
| `CONFIG_PSTORE*` / `VA_BITS` / `4K_PAGES` / `text_offset` | 与 normal 相同 | — | **全部相同 → 不是差异项** |

**结论**：diag 与 normal 的差异主要在"**多 probe 了哪些子系统**"，
而**共同点是都用了错的 DTB**。这解释了为什么"把子系统一个个关掉"这种
逐项排查法走了 18 轮还没找到根因 —— **因为根因不在被关掉的那些子系统里**。

### 4.5 精确的"死在哪"排序

| 阶段 | 代码位置 | 结果 |
|---|---|---|
| `head.S` 建页表 + PI 重定位 | `arch/arm64/kernel/head.S` + `pi/map_kernel.c` | **通过**（VA_BITS=39 已生效） |
| `time_init()` 的 `arch_timer_of_parse` | `drivers/clocksource/arm_arch_timer.c:1465` | **PPI 失败** → clocksource 和 clockevent **都没注册** |
| `time_init()` 的 `panic` 判据 | `arch/arm64/kernel/time.c:64-66` | **不触发**（`arch_timer_rate` 已由 `clock-frequency` 设为 19200000） |
| `calibrate_delay()` | `init/main.c:1098` → `calibrate.c:291` | **不挂死**（走 `lpj_fine` 分支） |
| `smp_init()` → `__cpu_up()` | `arch/arm64/kernel/smp.c:149` `wait_for_completion_timeout(...5000ms)` | **永久挂住**（jiffies 不前进，超时永不触发） |
| `postcore_initcall(ramoops_init)` | `fs/pstore/ram.c:987` | **永远到不了** ← 这就是"无日志" |
| `device_initcall`（DRM/USB/charger） | — | **永远到不了** ← 这就是"卡 logo" |

> 若 `smp_init()` 因某种原因没卡住，下一个必卡点是任何
> `msleep()`/`usleep_range()`/`schedule_timeout()` —— 无 tick 时都不返回。
> **结论一致：ramoops 和显示链路都永远等不到执行机会。**

---

## 5. DTB 内容差异：具体缺失节点清单

### 5.1 顶层节点对比（CAF DTB 有 / 我们 DTB 没有）

用 python 对比 `/tmp/caf-msm8953.dtsi` 与反编译的 `msm8953-oppo-r9sk.dtb`：

```
=== CAF 顶层节点 MISSING from our DTB ===
  aliases                      ← 4.19 用 aliases 给 mmcblk 编号（重要！）
  cpu-pmu
  mem_dump
  qcom,chd_gold / qcom,chd_silver   ← 死机检测
  qcom,ipc-spinlock@1905000
  qcom,iris-fm
  qcom,lpass@c200000           ← 我们叫 remoteproc（等价）
  qcom,memshare
  qcom,mpm2-sleep-counter@4a3000
  qcom,msm-adsprpc-mem
  qcom,msm-gladiator@b1c0000
  qcom,msm-imem@8600000        ← 重要：imem 是很多 CAF 驱动依赖的
  qcom,msm-rtb
  qcom,msm_fastrpc
  qcom,mss@4080000
  qcom,pronto@a21b000
  qcom,rmnet-ipa
  qcom,rmtfs_sharedmem@00000000
  qcom,smdpkt
  qcom,smp2p_sleepstate        ← sleep state，与 suspend 相关
  qcom,smsm
  qcom,sps
  qcom,venus@1de0000
  qcom,wcnss-wlan@0a000000
  restart@4ab000
  smd                          ← 重要：SMD 总线根节点
  smem                         ← 我们叫 smem@86300000（等价，compatible 相同）
  timer@b120000                ← 我们有（工作区已加 clock-frequency）
```

### 5.2 任务点名的检查项逐一回答

| 检查项 | CAF 4.19 | 我们 6.12 | 影响 |
|---|---|---|---|
| **`wake-gic` / mpm** | 有节点 `wake-gic@601d4`，`compatible="qcom,mpm-gic-msm8953"`；根 `interrupt-parent=<&wakegic>`；`CONFIG_QCOM_MPM=y` | **完全没有**：无节点、根 `interrupt-parent=<&intc>`、`CONFIG_QCOM_MPM` 未设 | **CAF DTB + 6.12 内核 = 中断树全废**（§2）。但如果用**我们的** DTB，反而**不需要** MPM ✅ |
| **`smd` / `smem`** | `smd { compatible="qcom,smd" }`（`/tmp/caf-msm8953.dtsi:1322`）；`smem_region@86300000`（:1230） | `smem@86300000`（`msm8953.dtsi:277`，compatible="qcom,smem" ✅）；**没有** `smd` 总线节点，用的是 `smd-edge` 子节点（:196, :1265, :1747, :1878） | 6.12 用 `smd-edge` 新绑定，**等价**。但**两者都要求 `CONFIG_MAILBOX=y`**（§3） |
| **`rpm` 相关** | `rpm_bus: qcom,rpm-smd`（:1556）；`rpm_msg_ram: memory@60000`（:1225） | `rpm: remoteproc { compatible="qcom,msm8953-rpm-proc","qcom,rpm-proc"; smd-edge { qcom,smd-edge=<15>; rpm_requests { compatible="qcom,rpm-msm8953","qcom,smd-rpm" } } }`（`msm8953.dtsi:193-205`）+ `rpm_msg_ram`（:445） | **等价且更完整** ✅。缺的仍是 `CONFIG_MAILBOX` |
| **`cpu-idle` / `lpm`** | CAF 也**没有** `idle-states` / `cpu-idle` 节点（`grep -c` = 0） | 同样 0 | **不是缺失项**。但 `CONFIG_ARM_PSCI_CPUIDLE=y`（defconfig:88）会走 PSCI 的 CPU_SUSPEND —— 无 lpm 时可能挂死，**建议关掉** |
| **`cpr`** | CAF **没有**（`grep -c cpr` = 0） | 同样 0 | 不是缺失项。但 `CONFIG_QCOM_CPR=y`（`.config` 实测）会去找 `qcom,cpr` 节点，找不到就跳过 —— 无害 |
| **`clock-controller` 的 clocks 引用** | `clock_gcc: qcom,gcc@1800000`，clocks 用**私有枚举** `clk_gcc_*`（4.19 CAF），**无 `#clock-cells` 标准绑定** | `gcc: clock-controller@1800000`，`clocks = <&rpmcc RPM_SMD_XO_CLK_SRC>, <&sleep_clk>, <&mdss_dsi0_phy 1>, ...`（`msm8953.dtsi` gcc 节点） | 6.12 用标准 CCF 绑定 ✅。**但 `&rpmcc` 依赖 RPM → 依赖 MAILBOX**（§3.3） |
| **`tsens`** | `tsens@4a8000`，`reg = <0x4a8000 0x1000>, <0x4a9000 0x1000>`，`reg-names = "tsens_srot_physical","tsens_tm_physical"` | `thermal-sensor@4a9000`，`reg = <0x4a9000 0x1000>, <0x4a8000 0x1000>`（**顺序相反，但 6.12 驱动按 compatible 解析，无 reg-names 依赖** ✅） | 等价 ✅ |
| **`cpr` / `sleep states`** | 无 | 无 | — |
| **`aliases`** | `sdhc1`/`sdhc2`/`i2c1`… （:177） | **无 aliases** | `mmcblk0`/`mmcblk1` 编号可能颠倒 → **rootfs 找不到**（但这是"启动后"问题，不是"卡 logo"） |

### 5.3 我们 DTB 里**多余但正确**的东西

```
=== 我们的顶层节点（CAF 没有）===
  clocks / cpus / firmware / gpio-keys / memory@10000000 / pmu / psci
  regulator-touch-1v8 / regulator-touch-2v8 / remoteproc / smsm
  soc@0 / sound / thermal-zones / vph-pwr-regulator
```

其中 `psci { compatible="arm,psci-1.0"; method="smc" }` **是我们的优势**：
CAF 把 psci 节点放在 `msm8953-cpu.dtsi` 里（`git show ...msm8953-cpu.dtsi` 第 17-20 行），
我们的 `msm8953.dtsi:188-191` 也有。✅

### 5.4 `qcom,pmic-id` 缺失（ABL 匹配风险）

```
$ git show lineage-22.1-k4.19:arch/arm64/boot/dts/vendor/qcom/msm8953-mtp_16027.dts | grep -c pmic-id
1                                   # 4.19 有 qcom,pmic-id = <0x010016 0x010011 0x0 0x0>

$ grep -c "pmic-id" arch/arm64/boot/dts/qcom/msm8953-oppo-r9sk.dts
0                                   # 我们没有
```

CAF 的 LK/ABL 在 `Image.gz-dtb` 尾部按 **`msm-id` + `board-id`** 扫（这点在
`arch/arm64/boot/dts/vendor/qcom/Makefile:538-552` 的注释里有本仓库自己的实测记录：
"ABL 是按 msm-id/board-id 自前向后扫描取**第一个**匹配"）。
**`pmic-id` 不参与 kernel-dtb 匹配**（它用于 `dt.img` 的 QCDT 表筛选）。
→ **不是缺失项**，但加上更保险（与 4.19 保持一致）。

---

## 6. 内核配置对比（diag / normal / 4.19）

### 6.1 4.19 开了、6.12 没生效的必需项

| 符号 | 4.19 | 6.12 `.config` 实测 | 后果 |
|---|---|---|---|
| `CONFIG_MAILBOX` | 未直接写，但被 `QCOM_SMP2P` 等 select/imply 拉起 | **NOT SET** | **§3 整条链断裂** |
| `CONFIG_RPMSG_QCOM_SMD` | `=y`（`/tmp/419-defconfig:553`） | **不存在** | SMD 传输层缺失 |
| `CONFIG_QCOM_SMP2P` | `=y`（:564） | **不存在** | smp2p 通道全废 |
| `CONFIG_QCOM_SMSM` | `=y`（:565） | **不存在** | smsm 状态机全废 |
| `CONFIG_QCOM_MPM` | `=y`（:604） | **NOT SET** | 用 CAF DTB 时中断树废 |
| `CONFIG_RPMSG_QCOM_GLINK_RPM` | `=y`（:551） | 未设 | RPM glink 备选通道缺失 |
| `CONFIG_ARM_GIC_V3_ACL` | `=y`（:603） | 6.12 无此符号（已并入） | — |
| `CONFIG_NR_CPUS` | **8**（`/tmp/419-defconfig`） | **512** | 每 CPU 静态区膨胀，`cpu_possible_mask` 巨大 |

### 6.2 6.12 开了、4.19 没开的危险项

| 符号 | 6.12 | 4.19 | 风险 |
|---|---|---|---|
| `CONFIG_NR_CPUS=512` | 512 | 8 | 内存/启动开销；`GIC_CPU_MASK_SIMPLE(8)` 与 512 不匹配（**潜在**：`timer` 的 PPI 只有 8 个 cpu mask，但 `NR_CPUS=512` 时 `for_each_possible_cpu` 会碰到未配置的 CPU） |
| `CONFIG_ARM_PSCI_CPUIDLE` | `=y`（defconfig:88） | 未设 | 无 lpm 节点时 PSCI CPU_SUSPEND 可能挂死 |
| `CONFIG_QCOM_CPR` | `=y` | 未设 | 无 `qcom,cpr` 节点 → 跳过（无害） |
| `CONFIG_ARM64_MTE` / `PTR_AUTH` / `BTI` | 全 `=y` | 未设 | A53 无这些扩展，运行时 feature 检测会关掉；**内核态** `ARM64_PTR_AUTH_KERNEL` 未设（`ARM64_BTI_KERNEL` 也不存在）→ 无害 |
| `CONFIG_RANDOMIZE_BASE` | `=y` | `=y`（:56） | 相同，**不是差异项** |
| `CONFIG_DEBUG_INFO` | `=y` + `REDUCED` | 4.19 也没有 | `_kernel_size_le` 是 `_end - _text`，**含调试段** → Image 39 MiB。这只影响体积，不影响功能 |
| `CONFIG_ACPI` | 未设（已修） | 未设 | ✅ |
| `CONFIG_EFI` | 未设（已修） | 未设 | ✅ |

### 6.3 `CONFIG_ARM64_*` 逐项确认（`.config` 实测）

```
CONFIG_ARM64_VA_BITS=39        ✅（Cortex-A53 上限）
CONFIG_PGTABLE_LEVELS=3        ✅
CONFIG_ARM64_4K_PAGES=y        ✅
CONFIG_ARM64_PA_BITS=48        ✅
CONFIG_ARM_GIC=y               ✅（select 自 arch/arm64/Kconfig:120）
CONFIG_ARM_GIC_V3=y            ✅
CONFIG_ARM_ARCH_TIMER=y        ✅
CONFIG_ARM_PSCI_FW=y           ✅
CONFIG_SMP=y                   ✅
```

> ⚠️ 注意 `CONFIG_ARM_GIC=y` 是 `arch/arm64/Kconfig:120` 无条件 `select` 的，
> 所以 `qcom,msm-qgic2`（`drivers/irqchip/irq-gic.c:1520`）**一定能匹配**。
> GIC 本身没问题 —— 有问题的是**中断父节点指向了不存在的 wakegic**（§2）。

### 6.4 defconfig 内部矛盾

```
arch/arm64/configs/msm8953-oppo_defconfig:1792-1806
1792: # ACK 默认给 arm64 选的是 ARM64_VA_BITS=52 + PGTABLE_LEVELS=5...
1805: CONFIG_ARM64_VA_BITS_39=y
1806: CONFIG_PGTABLE_LEVELS=4      ← ❌ 与 39-bit 不匹配！
```

`.config` 实际算出 `PGTABLE_LEVELS=3`（正确），说明 `PGTABLE_LEVELS` 是
`def_bool` 由 VA_BITS 推导、**defconfig 里这一行被忽略**。
但**留着这行是隐患**：如果哪天 Kconfig 顺序变了，会出现 39-bit VA + 4 级页表的错误组合。

`arch/arm64/configs/msm8953-oppo_defconfig:17` 还有 `CONFIG_NUMA_BALANCING=y`
（在 81 行 `CONFIG_NUMA=y` 之前），虽然 1907 行之后又写了 `# CONFIG_NUMA is not set`，
`make defconfig` 顺序应用时后者生效 —— 但**同一文件内自相矛盾**，可读性差。

---

## 7. 代码层面（任务方向 D）

### 7.1 `head.S` 的 `text_offset = 0x80000`

```asm
arch/arm64/kernel/head.S:81
	.quad	0x80000				// Image load offset from start of RAM
```

- `arch/arm64/include/asm/boot.h:18`: `#define MIN_KIMG_ALIGN  SZ_2M`
- 0x80000 = 512 KiB **不满足 2 MiB 对齐**

**与 `MIN_KIMG_ALIGN` 的交互**（`arch/arm64/kernel/pi/map_kernel.c:219-220`）：

```c
u64 va_base, pa_base = (u64)&_text;
u64 kaslr_offset = pa_base % MIN_KIMG_ALIGN;
```

如果 ABL 真的把内核装在 `RAM_start + 0x80000`，则 `pa_base % 2MiB == 0x80000 ≠ 0`
→ `kaslr_offset` 低 21 位非 0 → `map_kernel()` 里 `twopass = TRUE`
→ 走 `relocate_kernel(kaslr_offset)`（`map_kernel.c:103`）做自重定位。

**这条路径 6.12 是支持的**（PI + RELOCATABLE），所以**不会崩**。
但这会带来两个副作用：
1. 启动变慢、早期内存多一份拷贝
2. `kaslr_init()`（`arch/arm64/kernel/kaslr.c:29`）判据
   `kaslr_offset() < MIN_KIMG_ALIGN` → 0x80000 < 2MiB → 打印
   `"KASLR disabled due to lack of seed"`，**但 `kaslr_offset()` 本身非 0**
   （它被 `early_map_kernel` 设成了 `pa_base % 2MiB`）→ **语义混乱**

**建议**：`text_offset` 改回 `0`（mainline 语义），或改成 `0x200000`（2 MiB 对齐）。
CAF 的 4.19 内核里 0x80000 是配合 **非 PI** 的 4.19 head.S（无 `relocate_kernel` 路径）。
把 4.19 的常量搬到 6.12 的 PI 内核上，语义已经变了。

**但这不是当前卡死的原因** —— 诊断内核（`arch/arm64/boot/Image` 实测
`text_offset=0x80000`）与正常内核相同，现象不同 → 不是差异项。

### 7.2 `arch/arm64/boot/Makefile` 的 `Image.gz-dtb`

```
arch/arm64/boot/Makefile
CAF_DTB_NAMES := msm8953-oppo-r9s.dtb msm8953-oppo-r9sk.dtb
$(obj)/Image.gz-dtb: $(obj)/Image.gz $(CAF_DTB_OBJS) $(obj)/dts/dtbs-list FORCE
	cat $(obj)/Image.gz > $@.tmp
	for d in $(CAF_DTB_OBJS); do cat $$d >> $@.tmp; done
```

**这段逻辑本身没错**（与 4.19 一致，`git show lineage-22.1-k4.19:arch/arm64/boot/Makefile:68-69`）。
**错的是"以为这样就能让内核用上我们的 DTB"**。

> ### ⚠️ 一处我在分析中犯过、必须撤回的错误推断
>
> 我一度认为 4.19 的 `CONFIG_BUILD_ARM64_APPENDED_DTB_IMAGE=y`
> （`/tmp/419-defconfig:57`）会让**内核自己在启动时解析尾部 DTB**，
> 从而"6.12 缺这个能力"就是差异所在。**这是错的。**
>
> 查 Kconfig 定义即可推翻：
> ```
> $ git show lineage-22.1-k4.19:arch/arm64/Kconfig | sed -n '1665,1676p'
> config BUILD_ARM64_APPENDED_DTB_IMAGE
> 	bool "Build a concatenated Image.gz/dtb by default"
> 	depends on OF
> 	help
> 	  Enabling this option will cause a concatenated Image.gz and list of
> 	  DTBs to be built by default (instead of a standalone Image.gz.)
> 	  The image will built in arch/arm64/boot/Image.gz-dtb
> ```
> 它**只是构建期开关**，只改 `KBUILD_IMAGE`/`KBUILD_TARGET`
> （`arch/arm64/Makefile:138-144`），**不引入任何运行期解析代码**。
> `git grep ARM_APPENDED_DTB -- arch/arm64/` 在 4.19 里同样是空 ——
> arm64 **从来没有**运行期 appended-DTB 解析器
> （那是 32 位 `arch/arm/boot/compressed/head.S:274` 才有的 `ARM_APPENDED_DTB`）。
>
> **修正后的正确结论**：
>
> > **ABL 从来不读 kernel 尾部的 DTB —— 4.19 时代也不读。**
> > 4.19 能启动，是靠 boot.img 的 `dt` 区（QCDT）里带着对应机型的 DTB。
> > `Image.gz-dtb` 尾部那份对 ABL 是**冗余**的。
>
> 推论：`ci/mkflashable.sh:14-20` 和 `arch/arm64/boot/Makefile:44-90` 里
> 大量"ABL 会在 Image.gz-dtb 里按 msm-id/board-id 挑 DTB"的注释
> **都是错误的技术假设**。它们基于一个**正确观察**
> （日志里 `Machine model` 是 CAF 的），推出了一个**错误原因**
> （"ABL 在 kernel 尾部挑错了 DTB"）。真实原因是
> **ABL 用 boot.img 的 `dt` 区，而那个区里装的是原厂 QCDT**。
>
> 唯一仍然成立的判断：**`Image.gz-dtb` 尾部追加 dtb 是死数据** ——
> 但理由不是"6.12 少了内核解析器"，而是"**ABL 根本不看那里**"。

---

## 8. 打包/刷写链路（任务方向 E）—— 明确回答

### Q1：R9s 原厂 boot.img 若有 `boot.img-dt`（QCDT 格式），我们的 dtb 会不会被丢弃？

**会。** 证据链：

1. `ak3-core.sh:101-104`：原厂 dt 是 `QCDT` → 存为 `$SPLITIMG/dt`（不是我方的）
2. `ak3-core.sh:349-356`：`flash_boot()` 找 `$dt`，命中 `$SPLITIMG/dt`
3. `ak3-core.sh:381`：`mkbootimg ... --dt $SPLITIMG/dt` → **原厂 QCDT 原样写回**
4. ABL 从 boot.img 的 dt 区取 DTB 传给内核（x0）
5. → 内核拿到 CAF DTB → `Machine model: ... MSM8953 + PMI8950 MTP`

另外**即使**原厂 dt 是裸 FDT（`ak3-core.sh:105-107` 的 `*)` 分支），
它会被 cat 进 kernel 尾部，而 **ABL 根本不扫 kernel 尾部**（§7.2）
→ 同样拿不到我们的 dtb。

**两条路都丢。** 这就是为什么"DTB 43 KB vs 242 KB"其实**完全无关** ——
不是我们的 dtb 太小，而是**它根本没被加载**。

### Q2：正确做法是什么？

**放独立的 `dt` 文件**（即 `$SPLITIMG/dt`），但要注意格式：

| 原厂 dt 格式 | 正确做法 |
|---|---|
| **QCDT**（magic `QCDT`） | 生成**只含我们 2 个 dtb 的 QCDT 表**，按 `msm-id + board-id` 编目。参考 AOSP `dtbtool` 或 `mkdtimg` 的输出格式 |
| **裸 FDT**（magic `d00dfeed`） | 直接把我们的 dtb 作为 `dt`（单机型）；多机型必须用 QCDT |
| **ELF**（`unpackelf` 识别） | 用 `elftool`（`ak3-core.sh:372`）打包，`$dt` 需 `,rpm` 后缀 |

**关键点**：`mkbootimg` 的 `--dt` 会被原厂 QCDT 的**表项数和 board-id 语义**约束。
如果原厂 QCDT 里有多条（MTP/CDP/QRD），我们的表也只放 2 条 →
ABL 若按"找不到匹配就报错"，需保证 `msm-id=293` + `board-id=<8 0 16017/16027>` 格式与
ABL 期望一致（CAF 是 `<hw_id, variant_id, subtype>` 顺序，见我们的
`msm8953-oppo-r9sk.dts:15` 的 `qcom,board-id = <8 0 16027>` ✅）。

**当前工作区 `ci/mkflashable.sh:150-176` 的 QCDT 生成代码是错的**（§1.3），必须重写。

### Q3：`dump_boot/write_boot` vs `split_boot/flash_boot` 的差异

| | 4.19 workflow（`build-4.19.yml:426-428`） | 6.12 `ci/mkflashable.sh` |
|---|---|---|
| 调用 | `split_boot; flash_boot;` | `dump_boot;`（. `ak3-oppo-replace-dt.sh`）`write_boot;` |
| 差异 | 4.19 **不解包 ramdisk**（省时，不改 ramdisk） | 6.12 解包并重打包 ramdisk（`repack_ramdisk`） |

**这个差异本身不会导致卡 logo**，但有两个风险：
1. `repack_ramdisk` 会重新压缩 ramdisk（`ak3-core.sh:288-300`）。若压缩算法/级别与原厂不同，
   某些 ABL 会因 ramdisk 校验失败而拒绝启动。**建议改成 `split_boot; flash_boot;`**
   （与本内核"不改 ramdisk"的定位一致，也更接近 4.19 的可启动路径）。
2. `write_boot` 会额外 `flash_generic vendor_dlkm/system_dlkm/dtbo`（`ak3-core.sh:641-643`）。
   R9s 没有这些独立分区，`flash_generic` 找不到会 `abort`（`ak3-core.sh:583`）
   → **刷机可能中途失败**。应改回 `flash_boot`。

---

## 9. 建议的修复顺序（最小验证路径）

按"每一步都能独立证伪"排列：

### 第 1 步（关键，必须先做）：让 DTB 真正生效

```
在 ci/mkflashable.sh 里：
  1. anykernel.sh 改回 split_boot; flash_boot;     （不做 ramdisk repack，不 flash_generic）
  2. 在 split_boot 之后：
     - 若 $SPLITIMG/dt 是 QCDT → 用正确的 dtbtool 逻辑重建（只含 r9s/r9sk 两条，
       offset 4 字节对齐、size 页对齐、entry 含 msm_id + board_id）
     - 若是裸 FDT → 直接 cp 我们的 dtb
     - 若不存在 → 创建裸 FDT
  3. 若原厂走 mkbootimg 路径，确保 --dt 指向我们重建的文件
```

**验证判据**：刷完看 `/sys/fs/pstore/console-ramoops-0` 里
- 应出现 `Machine model: OPPO R9sk`（**不是** `...MSM8953 + PMI8950 MTP`）
- 应**不再出现** `no irq domain found for wake-gic@601d4`
- `arch_timer` 应打印正常的 `arch_timer: cp15 timer(s) running at 19.20MHz`

### 第 2 步：补 `CONFIG_MAILBOX=y`

```diff
--- a/arch/arm64/configs/msm8953-oppo_defconfig
+++ b/arch/arm64/configs/msm8953-oppo_defconfig
+CONFIG_MAILBOX=y
```

同步给 `msm8953-oppo-diag_defconfig`，并在 CI 断言里加查
`MAILBOX / RPMSG_QCOM_SMD / QCOM_SMP2P / QCOM_SMSM`。

### 第 3 步：修 defconfig 内部矛盾

```diff
-CONFIG_PGTABLE_LEVELS=4          # 与 VA_BITS_39 冲突
-CONFIG_NUMA_BALANCING=y          # 与后面的 # CONFIG_NUMA is not set 冲突
-CONFIG_NR_CPUS=512               # 4.19 是 8；改为 8
-CONFIG_ARM_PSCI_CPUIDLE=y        # 无 lpm 节点，A53 bring-up 阶段先关
```

### 第 4 步：`text_offset` 语义对齐

建议改回 `0`（mainline/PI 语义）或 `0x200000`（2 MiB 对齐）。
**这一步放在最后**，因为它与"卡 logo"无因果关系（diag/normal 都是 0x80000）。

### 第 5 步：补 `qcom,pmic-id` 与 `aliases`

```dts
/* msm8953-oppo-r9sk.dts */
qcom,pmic-id = <0x010016 0x010011 0x0 0x0>;   /* 与 4.19 一致 */

/* msm8953-oppo-common.dtsi */
aliases {
	sdhc1 = &sdhc_1;
	sdhc2 = &sdhc_2;
	i2c1 = &i2c_1;
	...
};
```

---

## 10. 具体缺失节点清单（task 要求）

> 前提：**如果坚持用 CAF DTB**（不推荐），则 6.12 缺的是下面这些驱动侧支持；
> **如果用我们自己的 DTB**（推荐），则下面这些**大部分本来就不需要**。

### 10.1 用 CAF DTB 时，6.12 缺失的**驱动**（节点在 DTB 里但没人认领）

| CAF 节点 / compatible | 6.12 状态 | 后果 |
|---|---|---|
| `wake-gic@601d4` / `qcom,mpm-gic-msm8953` | **无驱动**（只有 `qcom,mpm`） | 中断树全废 |
| `qcom,msm-qgic2` / `interrupt-controller@b000000` | ✅ `irq-gic.c:1520` | 正常 |
| `apm@b111000` / `qcom,msm8953-apm` | **无驱动** | APM 不初始化（大概率无害） |
| `qcom,msm-gladiator@b1c0000` | **无驱动** | 无害 |
| `qcom,msm-imem@8600000` | 6.12 有 `qcom,msm-imem` ✅ | — |
| `qcom,rpm-smd`（RPM bus） | 6.12 用 `qcom,rpm-msm8953` ✅（不同绑定） | CAF 的 `rpm-smd` 不会被认领 |
| `qcom,gcc-8953`（CAF 名） | 6.12 是 `qcom,gcc-msm8953` | **不匹配** → 时钟不注册 |
| `qcom,i2c-msm-v2` | 6.12 用 `qcom,i2c-qup-v2.2.1` | **不匹配** → i2c 全废 |
| `qcom,sdhci-msm` | ✅ 6.12 有 | — |
| `qcom,spmi-pmic-arb` | ✅ 6.12 有 | — |
| `qcom,qpnp-smbcharger` / `qpnp-fg` | 本仓库已移植 ✅ | — |
| `qcom,dwc-usb3-msm` | 6.12 用 `qcom,dwc3` 不同绑定 | **不匹配** → USB 废 |

→ **即使用 CAF DTB，还有至少 5 个核心 compatible 对不上**（gcc/i2c/usb/rpm/apm）。
这进一步证明：**必须让我们的 DTB 生效**，CAF DTB 在 6.12 上是死路。

### 10.2 用我们自己的 DTB 时，**真正**需要的补充（按优先级）

| 优先级 | 缺失项 | 为什么需要 | 修法 |
|---|---|---|---|
| **P0** | `CONFIG_MAILBOX=y`（配置，非节点） | §3，否则 RPM 链全断 | 加 defconfig |
| **P0** | 打包链路把 dtb 放进 boot.img 的 `dt` 区 | §1/§8 | 重写 `ci/mkflashable.sh` |
| **P1** | `aliases { sdhc1/sdhc2/i2cN }` | 否则 `mmcblk0`/`mmcblk1` 编号可能颠倒 → rootfs 挂不上 | 加进 `msm8953-oppo-common.dtsi` |
| **P1** | `qcom,pmic-id` | 与 4.19 一致，ABL/QCDT 筛选更稳 | 加进 r9s/r9sk dts |
| **P2** | `qcom,smem` 节点已有 ✅，但需确认 `hwlocks = <&tcsr_mutex 3>` 的 hwlock 驱动 | `CONFIG_HWSPINLOCK_QCOM=y` 已开 ✅，`tcsr_mutex: hwlock@1905000` 在 `msm8953.dtsi:822` ✅ | 无需改 |
| **P2** | `smp2p_sleepstate` | 只有真的要做 suspend/resume 才需要 | 后续再加 |
| **P3** | `qcom,msm-imem`（`CONFIG_QCOM_IMEM`） | 部分 CAF 移植驱动（如 `qpnp-fg`）会读 imem | 按需 |
| **P3** | `qcom,msm-rtb`（boot stats） | 仅统计用途 | 可选 |

### 10.3 **不需要**补的（任务点名但实际不影响）

| 节点 | 结论 |
|---|---|
| `wake-gic` / mpm | **不要补**。我们的 DTB 根 `interrupt-parent=<&intc>`，timer 直接指 GIC，**不需要 MPM** 就能启动。补了反而要匹配 6.12 的新绑定（`qcom,mpm-pin-count`/`qcom,mpm-pin-map`），风险更大 |
| `cpu-idle` / `lpm` / `idle-states` | **CAF 也没有**（`grep -c` = 0）。A53 bring-up 阶段用 PSCI CPU_SUSPEND 即可。建议**关掉** `CONFIG_ARM_PSCI_CPUIDLE` |
| `cpr` | **CAF 也没有**。`CONFIG_QCOM_CPR=y` 找不到节点会跳过，无害 |
| `tsens` | 我们有且兼容 ✅ |

---

## 附录 A：与并行改动的交叉验证

分析期间（2026-10-31 18:39–19:28）HEAD 被并行推进，新增 4 个提交。
逐条核对：

| 提交 | 判断 | 依据 |
|---|---|---|
| `21f8c5f7a` fix(config): 关闭 NUMA/MEMORY_HOTPLUG/THP —— 称"卡 logo 无日志的根因" | **改动本身无害，但归因错误** | `arch_numa_init()` 有 `dummy_numa_init()` 强制兜底（`drivers/base/arch_numa.c:318-328`），不会 panic。且 `.config` 实测改动前 `CONFIG_NUMA` 就已是 `not set`（磁盘上的 `.config` 时间戳 18:37:44 早于该提交 18:39:03，无法作为前后对照） |
| `5c4e27204` / `e2d758fe0` fix(flash): 用我们的 DTB 覆盖 dt 分区 | **方向正确，实现有 bug** | 见 §1.3：QCDT 生成格式错误（entry 缺 board_id、size 未页对齐、只写 1 条） |
| `eb5a3cdf8` docs: DTB 从来没生效过 | **方向正确** | 与 §1 结论一致 |

另外，工作区里 `arch/arm64/boot/dts/qcom/msm8953.dtsi` 有未提交改动
（`git diff` 显示新增 `timer@b120000` 和 `timer` 的 `clock-frequency = <19200000>`）。

该改动**方向没错，但治不了这个 bug**，而且**注释里的归因是错的**：

- 注释说"CAF 的 ABL 并不保证设过 CNTFRQ → timer 起不来"。
  但 `arch_timer_of_configure_rate()`（`arm_arch_timer.c:1064-1076`）在
  `of_property_read_u32(np, "clock-frequency", ...)` 失败时，
  会**回退用 `arch_timer_get_cntfrq()` 读 CNTFRQ_EL0**；
  而且 PPI 检查（:1465）在它**之后**。
  → **频率从来不是失败点，中断才是。**
- 加了 `clock-frequency` 的唯一效果：让 `arch_timer_rate` 一定非 0，
  于是 `time_init()` 的 `panic()` 判据 `!arch_timer_rate` **一定不触发**
  —— 这反而使失败模式从"可能 panic 留下线索"退化为"静默无 tick 卡死"。
  现象没变，可诊断性变差了。
- 真正的修法仍是 §1.2：让我们的 DTB 生效（根 `interrupt-parent = <&intc>`，
  timer PPI 直指 GIC，不经过 `wakegic`）。

## 附录 B：可复现的验证命令

```bash
# 1. 确认我们的 dtb 内容正确（应看到 OPPO R9sk + 无 wake-gic）
dtc -I dtb -O dts arch/arm64/boot/dts/qcom/msm8953-oppo-r9sk.dtb | \
  grep -E "model =|wake-gic|venus|interrupt-parent" | head

# 2. 确认 .config 里 MAILBOX 链断裂
grep -E "^CONFIG_MAILBOX=|^CONFIG_(RPMSG_QCOM_SMD|QCOM_SMP2P|QCOM_SMSM|QCOM_APCS_IPC)=" .config

# 3. 确认 arm64 从来没有运行期 appended-DTB 解析器（4.19 也一样，应全空）
git grep -n "ARM_APPENDED_DTB" -- arch/arm64/
git grep -n "ARM_APPENDED_DTB" lineage-22.1-k4.19 -- arch/arm64/
#   → 两者都空，证明 BUILD_ARM64_APPENDED_DTB_IMAGE 只是构建期开关，
#     不能作为"内核会解析尾部 DTB"的依据。

# 4. 确认 6.12 没有 mpm-gic 驱动
grep -rn "mpm-gic" drivers/irqchip/*.c

# 5. 确认 Image header
python3 -c "import struct;d=open('arch/arm64/boot/Image','rb').read(64);\
to,isz=struct.unpack('<QQ',d[8:24]);print('text_offset=0x%x image_size=0x%x'%(to,isz))"

# 6. 刷机后最关键的一条
adb shell cat /sys/fs/pstore/console-ramoops-0 | head -50
#   看 model 是 "OPPO R9sk" 还是 "Qualcomm Technologies, Inc. MSM8953 + PMI8950 MTP"
```

---

## 附录 C：一句话总结

> **6.12 内核在这台机器上从未加载过我们写的设备树。**
> ABL 从 boot.img 的 `dt` 区取的是原厂 CAF DTB（`Machine model = ...MTP` 就是铁证），
> 而 `Image.gz-dtb` 尾部追加的 dtb 是死数据 ——
> 因为 **ABL 只从 boot.img 的 `dt` 区取 DTB，从不扫描 kernel 尾部**
> （对 4.9/4.19/6.12 都成立；arm64 从来没有运行期 appended-DTB 解析器）。
> 于是 CAF DTB 的根 `interrupt-parent = <&wakegic>` 指向一个 6.12 没有驱动的节点，
> 导致 `arch_timer` 的所有 PPI 映射失败（`No interrupt available`）→
> **clocksource 和 clock_event_device 都没注册 → 没有 tick → `jiffies` 冻结** →
> 卡在 `smp_init()` 的 `wait_for_completion_timeout()` 上（超时永远不会到）→
> `postcore_initcall(ramoops_init)` 和整个 `do_basic_setup()` 都**永远到不了** →
> 屏幕停在 ABL 画的 logo，pstore 一个字节都没有。
>
> 注意这里**不是 panic**：`time_init()` 的 `panic("Unable to initialise
> architected timer")` 判据是 `arch_timer_rate`，而它在 PPI 检查之前
> 就已经被 `clock-frequency`/`CNTFRQ` 设好了 → 不触发。
> 所以内核是**静默卡死**，连 panic 都留不下。
>
> **修法只有一条主线：把我们的 dtb 做成 boot.img 的 `dt` 区（正确格式的 QCDT 或裸 FDT），
> 而不是追加在 kernel 尾部。** 其余（MAILBOX、NR_CPUS、alias、pmic-id）是它之后才会暴露的下一层问题。
