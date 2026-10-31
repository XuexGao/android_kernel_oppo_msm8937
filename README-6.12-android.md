# 6.12-test —— 真 Android 6.12 内核（ACK + MSM8953 + OPPO R9s）

这个分支是**真正的 Android 内核**，不是 mainline。

---

## 基线是什么

| 层 | 来源 | 说明 |
|---|---|---|
| 内核 | **Android Common Kernel** `android16-6.12` | Linux **6.12.52** + Google 的 Android 补丁 |
| Android 特性 | ACK 自带 | binder / binderfs / dmabuf heaps / vendor hooks |
| MSM8953 平台 | ACK 继承自主线 | clk(MSM_GCC_8953) / pinctrl / interconnect / sdhci / USB PHY / remoteproc |
| 板级 | **本分支新增** | OPPO R9s(16017) / R9sk(16027) 设备树 |

关键点：**ACK 是带 MSM8953 的**。主线的 MSM8953 支持是 6.6 进的，
ACK 从主线 fork，所以 `arch/arm64/boot/dts/qcom/msm8953.dtsi` 和
`gcc-msm8953.c` / `pinctrl-msm8953.c` / `msm8953.c`(interconnect) 全都在。

这意味着"Android 6.12 + 这颗 SoC"在**平台层是成立的**，
不像 5.4 那样连 SoC 支持都没有（主线 5.4/5.10/5.15/6.1 都没有 MSM8953）。

---

## 与其它分支的区别

| 分支 | 内核类型 | 能跑 LineageOS |
|---|---|---|
| `lineage-22.1-k4.19` | 4.19 CAF downstream | ✅ 可以（现在就跑着） |
| `bakasu-k4.19` | 4.19 CAF + BakaSU/SUSFS | ✅ 可以 |
| `6.12-test`（本分支） | ACK 6.12 + 主线平台层 | ❌ 还不行，见下 |
| `oppo-r9s-mainline` 的 main | mainline 7.1.3 overlay | ❌ 永远不行（不是 Android） |

---

## 本分支新增的东西

```
arch/arm64/boot/dts/qcom/msm8953-oppo-common.dtsi   R9s/R9sk 公共部分
arch/arm64/boot/dts/qcom/msm8953-oppo-r9s.dts       board-id <8 0 16017>
arch/arm64/boot/dts/qcom/msm8953-oppo-r9sk.dts      board-id <8 0 16027>
arch/arm64/boot/dts/qcom/Makefile                   两条 dtb 规则
arch/arm64/configs/msm8953-oppo_defconfig           设备配置
.github/workflows/build-6.12.yml                    CI（GCC）
```

设备树以主线 `msm8953-motorola-potter.dts` 为蓝本（同芯片组合
MSM8953 + PM8953 + PMI8950，触摸屏同在 i2c_3@0x20 且都是 Synaptics），
真机参数取自本仓库 4.9/4.19 的 `oppo-msm8953/`：

| 项 | 值 |
|---|---|
| msm-id | 293 (MSM8953) |
| board-id | `<8 0 16017>` / `<8 0 16027>` |
| 屏幕 | 1080×1920 simple-framebuffer |
| 触摸屏 | i2c_3 0x20，中断 gpio17 / 复位 gpio16 |
| 音量键 | gpio85 / gpio86 |
| 背光 | PMI8950 WLED |
| SD 卡检测 | gpio133 |
| 面板 reset | gpio131（enable 38 / bklight-en 34）|

---

## 已验证

### 本地（aarch64-linux-gnu-gcc 14.2）

```
msm8953-oppo-r9s.dtb    39475 bytes
msm8953-oppo-r9sk.dtb   39475 bytes
```

### GitHub Actions 实跑通过（run 40460610787）

```
KERNEL_RELEASE = 6.12.52-gc8707895af5a      ← ACK android16-6.12
Image                 52,816,384 bytes
Image.gz              16,230,031 bytes
msm8953-oppo-r9s-Image.gz-dtb   16,269,506 bytes
msm8953-oppo-r9sk-Image.gz-dtb  16,269,506 bytes
```

日志确认 DTC 两个 dtb 都编了，配置也确认落地
（`CONFIG_ANDROID_BINDER_IPC=y`、`CONFIG_PINCTRL_MSM8953=y` 等）。

配置确认落地：

```
CONFIG_ANDROID_BINDER_IPC=y
CONFIG_ANDROID_BINDERFS=y
CONFIG_ANDROID_VENDOR_HOOKS=y
CONFIG_DMABUF_HEAPS=y / _SYSTEM=y / _CMA=y
CONFIG_MSM_GCC_8953=y
CONFIG_PINCTRL_MSM8953=y
CONFIG_MMC_SDHCI_MSM=y
CONFIG_BACKLIGHT_QCOM_WLED=y
CONFIG_RMI4_I2C=y
```

> `ANDROID_VENDOR_HOOKS` 依赖 `TRACEPOINTS`，所以 defconfig 里同时开了
> `CONFIG_TRACING=y`，否则这个选项会被 Kconfig 静默丢掉。

### 显示链路（第二轮，已接入真 MDSS/DSI）

把显示从 `simpledrm` 的静态 framebuffer 换成了**真正的 MDSS/DSI 链路**：

| 组件 | 来源 | 状态 |
|---|---|---|
| MDSS / MDP5 | ACK 的 `drm/msm`（`qcom,msm8953-mdp5`） | ✅ DTS 已 enable |
| DSI 控制器 | ACK（`qcom,msm8953-dsi-ctrl`） | ✅ DTS 已 enable |
| JDI R63452 面板（16027） | 从 `oppo-r9s-mainline` 搬入 | ✅ 编入内核 |
| Samsung EA8064 面板（16017） | 从 `oppo-r9s-mainline` 搬入 | ✅ 编入内核 |
| PMI8950 LAB/IBB 偏压 | 给 `pmi8950.dtsi` 新增 | ✅ 复用主线 `qcom,pmi8998-lab-ibb` |

搬面板驱动时踩到两处 API 差异：

1. `gpiod_set_value_cansleep()` 在 mainline/6.12 返回 **void**，而原驱动
   沿用了 downstream 的 `ret = gpiod_...` 写法 → `void value not ignored`
2. 修掉上一条后 `err_ibb` 成为不可达标签 → `-Werror=unused-label`

LAB/IBB 能直接复用主线 pmi8998 驱动的原因：主线 `qcom-labibb-regulator.c`
把寄存器基址硬编码为 `LAB=0xde00 / IBB=0xdc00`，与 PMI8950 一致，
且同在 SPMI USID 3。

**`simpledrm` 节点已刻意移除** —— simpledrm 与 mdss 同时拥有同一块屏会打架
（一个写线性 framebuffer、一个配 MDP/DSI），结果就是花屏。
要回退做对照实验：把 `chosen/framebuffer@90001000` 加回来，并把
`&mdss / &mdp / &mdss_dsi0` 设回 `disabled`。


### 背光 / 触摸 / USB / 存储（第三轮）

| 子系统 | 做了什么 | 关键点 |
|---|---|---|
| 背光 | LM3697 @ i2c_2 0x36，enable gpio46 | 见下 |
| 触摸 | RMI4 + 两路 gpio 使能 regulator | 见下 |
| USB gadget | configfs + ffs 内置 | `CONFIG_USB_CONFIGFS=y` |
| 存储 | EROFS + F2FS 打开 | Android 12+ 的 system 常用 EROFS |

#### 背光（LM3697）

ACK 自带 `leds-lm3697.c`，DTS 里配：

```
led-controller@36 { compatible="ti,lm3697"; reg=<0x36>;
                    enable-gpios=<&tlmm 46>; led@0 { reg=<0>; led-sources=<2>;
                    max-current-milliamp=<18>; default-trigger="default-on"; } }
```

两个容易搞错的地方：

1. **`reg` 是 control bank，不是灯串号**。主线驱动只接受 `reg=<0>` 或 `<1>`
   （对应 CONTROL_A/B）；vendor 里的 `led-sources=<2>` 才是"用第 3 路灯串"，
   写进 `reg` 会直接 `probe` 失败报 `reg property is invalid`。
2. `vled-supply` 可以不给 —— `devm_regulator_get()` 拿不到就退化成 dummy。

`default-trigger="default-on"`：bring-up 阶段没有 Android 用户态去设亮度，
用这个触发器让内核启动就把背光点亮，好让"屏到底亮没亮"可观测。
接 Android 后改成 `CONFIG_BACKLIGHT_LED` 桥接，由 HAL 走
`/sys/class/backlight`。

#### 触摸（Synaptics S3320）

16027 用的是 **S3320**，4.19 上是私有驱动 `synaptics-s3320`，主线只能挂
通用 `rmi4`。DTS 补齐了 vendor 节点里的两路使能脚：

```
synaptics,enable1v8-gpio = <&tlmm 134>   → touch_1v8 (fixed-regulator, vio-supply)
synaptics,enable2v8-gpio = <&tlmm 132>   → touch_2v8 (fixed-regulator, vdd-supply)
```

原来只写了 `vdd-supply=<&pm8953_l22>` 是错的 —— 这两路是 **GPIO 控制**的
LDO 使能，不是 PMIC 常电轨。同时补了 `ts_int`（gpio17）pinctrl。

另外开了 `CONFIG_RMI4_F12`（2D 多点触控）—— 不开的话设备能 probe
但完全不出触摸事件。

#### USB gadget

Android 的 adb / fastboot 都靠 `configfs + ffs`，原来这几个是 `=m`，
改成内置（`USB_CONFIGFS / USB_F_FS / USB_CONFIGFS_F_FS`），
并把 `&usb3` 设成 `dr_mode="peripheral"`。

#### 存储

`CONFIG_EROFS_FS(+XATTR/ZIP)` 与 `CONFIG_F2FS_FS(+XATTR/COMPRESSION)`，
EROFS 给 system/vendor，F2FS 给 data。

CI 新增 **DTB 静态自检**：反编译 dtb 确认面板节点 / DSI 控制器 / LAB-IBB
都在，且 `simple-framebuffer` 确实不存在。任一不满足则构建失败，
避免"等刷机才发现屏不亮"。

---

## 充电 / 电量计（第四轮）

从 4.9/4.19 前移了 PMI8950 的 QPNP 电源栈：

| 文件 | 行数 | 作用 |
|---|---|---|
| `qpnp-smbcharger.c` | 8443 | 充电器 / OTG |
| `qpnp-fg.c` | 9299 | 电量计 |
| `batterydata-lib.c` | 485 | 电池曲线解析 |
| `pmic-voter.c` | 832 | 充电参数投票框架 |
| `qpnp-revid.c` | 298 | PMIC 版本识别 |
| `of_batterydata.c` | 719 | DT 电池特性解析 |

选 `qpnp-smbcharger + qpnp-fg` 而不是 `qpnp-smb2/smb5`：R9s 的 vendor DTS
用的就是这代，而且它们只依赖标准 `power_supply` API。

### 最大障碍：power_supply 私有属性被上游删光

4.19 有 **213** 个 property，6.12 只剩 **109**。QCOM 加的那批
（`USB_OTG` / `CAPACITY_RAW` / `BATTERY_INFO` / `SOC_REPORTING_READY` …）
全没了，另外还有 `DP_DM` 协商命令、`HVDCP` 类型等独立枚举。

处理方式：把 4.19 独有的 **147 个 property + 11 个 type + DP_DM 匿名 enum
+ 2 个 CHARGE_TYPE** 用脚本精确 diff 后**追加到枚举末尾**。

> 关键点：不能插到中间 —— property 的数值是 sysfs ABI，插中间会改掉所有
> 后续项的编号。追加则不影响既有 ABI。
>
> 安全性：`power_supply_sysfs.c` 的 `show_property` 对新名字没有专门
> case，会落到 `default` 直接打印 intval，不会崩也不会误走分支。

### 修掉的 9 类不兼容

1. **私有 ADC 框架** `qpnp-adc.h` 不存在 —— 但驱动其实只用标准 IIO
   consumer API（`iio_read_channel_processed`），去掉私有 include 即可
2. **Kconfig 循环依赖** —— `depends on REGULATOR/EXTCON` 形成
   `QPNP_SMBCHARGER → EXTCON → CHARGER_MANAGER → POWER_SUPPLY → 回来`，改 select
3. **batterydata-lib.h 条件漏了 QPNP_FG** —— 头文件走 inline stub 与 `.c`
   实现撞名（8 个 redefinition）
4. **Xiaomi 机型 hack** —— 下游插了一批未受 CONFIG 保护的
   `xiaomi_msm8953_mach_get()`，新增恒假 stub 头
5. `rtc_tm_to_time` → `rtc_tm_to_time64`（含变量类型改 `time64_t`）
6. `debugfs_create_u32/x32` 在 6.x 返回 void
7. `platform_driver.remove` 从 6.11 起返回 void
8. **implicit-fallthrough 里一处真 bug** —— `POWER_SUPPLY_PROP_CURRENT_MAX`
   分支缺 `break`，会误落到 `default` 返回 `-EINVAL`，已补
9. `%ld` 与 `time64_t` 不匹配 → `(long long)` + `%lld`

### DTS

`pmi8950.dtsi` 新增两个节点（参数取自 4.19 vendor）：

```
charger@1000  qcom,qpnp-smbcharger
              iterm 200mA / float 4380mV / fastchg 2000mA
              thermal-mitigation <2000 1500 1000 800>
              io-channels = <&pmi8950_vadc 3>     ← vchg_sns
fg@4000       qcom,qpnp-fg
              iterm 200mA / cutoff 3200mV / cc-cv 4360mV / 2820mAh
```

私有 phandle 属性 `qcom,vchg_sns-vadc` 改成标准 `io-channels/io-channel-names`。

> FIXME：R9s 的电池曲线（BATTERYDATA profile）还没塞进 DTS，
> 上机后 SOC 可能不准。

## 当前能力清单（截至第 11 轮）

| 子系统 | 状态 | 关键实现 |
|---|---|---|
| 显示 MDSS/MDP5/DSI + 14nm PHY | ✅ 全内置 | ACK 的 `drm/msm` |
| 面板 16017（Samsung EA8064） | ✅ | `panel-oppo-ea8064.c` |
| 面板 16027（JDI R63452） | ✅ | `panel-oppo-jdi-r63452.c` |
| 背光 LM3697 | ✅ | `leds-lm3697.c` + `default-on` |
| 触摸 Synaptics | ✅ | `rmi4` + 两路 gpio 供电 |
| USB gadget（adb/fastboot） | ✅ 内置 | `USB_CONFIGFS` + `ffs` |
| 存储 | ✅ | `EROFS_FS` / `F2FS_FS` |
| 充电器 | ✅ | `qpnp-smbcharger` |
| 电量计 | ✅ | `oppo_bq27541`（**不是** qpnp-fg） |
| VOOC 快充 | ✅ | `vooc_charger` + `stm8s-vooc` |
| 音频 CPU 侧 | ✅ | 上游 `q6afe/q6asm/q6adm` |
| 耳机 DAC | ✅ | `ak4376` |
| OPPO 设备信息 | ✅ | `drivers/soc/oppo/` |
| GPU (KGSL) | ❌ **WIP** | 源码在树里，47 处 API 差异待清，**配置关闭** |
| 外放 TFA98XX | ❌ | 2.5 万行独立栈，未开始 |
| 相机 | ❌ | 未开始 |

### 一个重要修正

R9s **不用 PMI8950 内部的电量计**，而是外挂 **TI BQ27541**（i2c_5 0x55），
快充由 **STM8S 单片机**（i2c_5 0x26）走 VOOC 协议。
`qpnp_fg` 节点保留作为兜底，实际 SOC 由 bq27541 提供。

---

## 上机验证指南

### 第 13 轮真机反馈：卡在一屏不动

用户刷了当时最新的包，结果是**卡在一屏、进不了系统**。
抓 ramoops 出来的却是 4.9 内核的旧日志 —— 说明 6.12 一行 dmesg 都没留下。

顺着这个现象查下去，找出三个"编译完全通过、烧进去必然卡死"的问题。
这三个都已在第 13 轮修掉：

#### 1. `ARM64_VA_BITS` 是 52，Cortex-A53 只支持 39

**这是卡屏的直接原因。** ACK 的 arm64 默认 defconfig 给的是
`VA_BITS=52` + 5 级页表，那是 ARMv8.2 的 LPA 扩展，Cortex-A53（ARMv8.0）
根本没有。内核会在 `head.S` 建页表的阶段就挂死 —— 正好是"卡一屏、
连 dmesg 都没有"。

设备侧反证（4.9 内核的启动日志）：

    vmalloc : 0xffffff8008000000 - 0xffffffbebfff0000   (250 GB)
                    ↑ 这个结束地址就是 39-bit VA 布局

修复：`CONFIG_ARM64_VA_BITS_39=y`，生效值 `PGTABLE_LEVELS=3`。

#### 2. ramoops 地址写错

我按 mainline 的假设填了 `0xef000000`，设备 bootloader 实际保留的是
`0xb0000000`（长 8 MiB）：

    [    0.164432] ramoops: attached 0x800000@0xb0000000, ecc: 0/0
                    ↑ 设备实测值

位置错了，panic 时 pstore 写到了没人读的地方，
`/sys/fs/pstore` 永远是空的。

#### 3. pstore 没有真正通电

只改地址还不够。实际配置是：

    CONFIG_PSTORE_RAM=m      ← 模块，panic 时没机会 insmod
    （PSTORE_CONSOLE 未设置）← 不开就不记普通启动的 dmesg

结果是黑匣子装了但从来没通上电。现在改成全 `=y`：

    CONFIG_PSTORE=y / PSTORE_RAM=y / PSTORE_CONSOLE=y / PSTORE_PMSG=y

**这三项修完之后，下一次刷机如果还卡，就能拿到 6.12 自己的 dmesg，
而不是继续盲飞。** CI 已加断言，这几个配置再被改坏会直接 fail。

---

#### 4. Image header 的 text_offset 是 0（CAF 要 0x80000）

修完 VA_BITS 之后仍然卡 logo，于是让 CI 把 Image header 打出来测量：

    header.text_offset = 0x0            ← 问题所在
    header.image_size  = 0x37a0000 (55.6 MiB)
    header.magic       = 0x644d5241     （正确）

**为什么是 0**：mainline 从 v4.2 起就不支持"指定加载偏移"了，
`arch/arm64/Makefile` 里的 `TEXT_OFFSET` 整个被删，
`head.S` 里那一行改成硬编码：

```asm
.quad 0    // Image load offset from start of RAM
```

语义变成"位置无关，bootloader 自己决定装哪"。

**但 Qualcomm 的 ABL/lk 不是这么工作的**：它读这个字段，
把内核装到「RAM 起始 + text_offset」。4.9 的 CAF 内核里这个值
是 `0x80000`（512KB），ABL 也一直按 512KB 装。

两者对不上时的后果：ABL 按 0 偏移加载 → 内核被搬到 RAM 起始而
不是起始+512KB → 入口点错位 → 内核在第一条指令就走飞 →
**死在 head.S 之前**，pstore 的 `postcore_initcall` 根本没机会执行。

这正好解释了同时观察到的两个现象：

| 现象 | 原因 |
|---|---|
| 卡在 ABL 画的那屏 logo | 内核从未接管，屏幕还停在 bootloader 画的帧 |
| ramoops 里始终是 4.9 的旧日志 | 6.12 一行都没写进去 |

修复：`arch/arm64/kernel/head.S` 把那行改回 `.quad 0x80000`。
只影响 bootloader 怎么装内核，不影响内核自身虚拟地址布局
（`KIMAGE_VADDR` 在 6.12 和 4.9 都是 `MODULES_END`）。

验证：编译 `head.o` 后反汇编 `.head.text`，偏移 0x08 处为
`00000800 00000000`（小端序的 0x80000）；CI 实测
`header.text_offset = 0x80000`。

### 前两个嫌疑的对照表

| 修复 | 错在哪 | 死在哪 |
|---|---|---|
| VA_BITS 52 → 39 | Cortex-A53 不支持 52-bit VA | `head.S` 建页表阶段 |
| text_offset 0 → 0x80000 | CAF ABL 按此字段定位 | 进入内核第一条指令前 |

两个都是"编译完全正常、烧进去必死"的类型。

#### 5. DSI PHY 没打开 / 面板 graph 不成对 / AK4376 挂错总线

（第 17 轮代码审查发现，都是"屏不亮/没声音"的直接原因）

| # | 问题 | 后果 |
|---|---|---|
| B-1 | `mdss_dsi0_phy` 仍是 `status="disabled"` | DSI PHY 的 platform_device 根本没创建 → `-EPROBE_DEFER` → msm_dsi 永不 probe → **屏永远不亮** |
| B-2 | 面板 graph 端点不成对（`mdss_dsi0_out` 没有 `remote-endpoint`） | `drm_of_find_panel_or_bridge()` 返回 `-ENODEV` → `dsi_bind()` 失败 → 同样黑屏 |
| B-3 | AK4376 挂在根节点下，不在 `&i2c_8` 上 | i2c probe 永不调用 → 声卡 `-EPROBE_DEFER` → **整张声卡注册失败** |

**B-1 特别隐蔽**：`status="disabled"` 的节点**照样留在 dtb 里**，
而 CI 的 DTB 自检只 grep compatible 字符串，所以自检 100% 通过。
这类问题只有刷机上电才会暴露。

现在 CI 已加两组断言：
- 按**节点名**（`phy@1a94400` / `dsi@1a94000` / `panel@0`）检查 status 必须 okay
- 反编译时若出现 `graph_endpoint ... is not bidirectional` 告警直接 fail

（注意不能按 compatible 判断：R9s 是单 DSI，`phy@1a96400`（dsi1）
本来就该 disabled，按 compatible 会误报。）

#### 6. Image.gz-dtb 塞进了 1193 个 dtb

照 4.19 的 `find $(obj)/dts/ -name *.dtb` 写，在 6.12 下行不通：
4.19 的 `dtb-y` 只有 9 个条目（全是本平台），**6.12 mainline 有 1193 个**
（覆盖所有 ARM64 高通机型）。find 到多少拼多少 → 全塞进去。

后果：体积暴涨 + **ABL 按 board-id 顺序扫 1193 个 dtb，有挑错的风险**。

修法：显式限定 `msm8953-oppo-r9s.dtb` / `msm8953-oppo-r9sk.dtb` 两个。
打包脚本的自检也从"至少 1 个"升级为"1..4 个"，
数量异常直接在 CI 拦下。

#### 7. 内核镜像 54.8 MiB 太大，ABL 装不下（"连日志都没有"的根因）

第 18 轮用户反馈「还是卡一屏，而且**连日志都没有了**」。

这句话本身就是最强线索 —— **诊断内核（10 MiB）能启动并留下日志，
正常内核反而一个字节都没有**。两者同样的 text_offset、同样的 VA_BITS、
同样的 DTB 拼接方式，唯一巨大差异就是体积。

实测对比（同一条 CI 流程产出）：

| 内核 | 平台数 | Image 解压后 | Image.gz |
|---|---|---|---|
| 裁剪前 6.12 | **71** | **54.8 MiB** | 17.0 MiB |
| 裁剪后 6.12 | 1 | **39 MiB** | 15.1 MiB |
| 4.19（真机可启动） | 3 | 36.2 MiB | 12.2 MiB |
| 4.9 CAF（原厂） | — | ~30 MiB | — |

**根因**：这个 defconfig 继承自 mainline 的
`arch/arm64/configs/defconfig`，那份配置把**几乎所有 ARM64 平台**
都启用了（ACTIONS / AIROHA / SUNXI / APPLE / BCM / EXYNOS /
MEDIATEK / TEGRA / RENESAS(十几个子型号) / ROCKCHIP … 共 **71 个**），
连带把这些平台的所有驱动都编进内核。

MSM8953 的 bootloader 要把内核解压到低端内存的连续区域，
54.8 MiB 超出可用范围 → 加载阶段失败 → 屏幕停在 ABL 画的那帧 logo，
**内核一条指令都没执行**，pstore 自然空白。

现在只保留 `ARCH_QCOM`（其余 70 个全部 `# ... is not set`），
并关掉 ACPI/EFI（MSM8953 用 DT 启动）。

> **踩过的判断错误**：前几轮我一直用
> 「`Image.gz-dtb` 17.9 MB < 32 MB boot 分区」判断为安全。
> 但 boot 分区只决定**刷写**能否成功，
> **解压后 54.8 MiB 才是真正要放进 RAM 的数字** —— 那才是瓶颈。

#### 8. **DTB 从来没有生效过**（最根本的一条）

前面几轮一直在改 `msm8953-oppo-common.dtsi`，但改了多少次现象都不变。
原因：**ABL 根本不用我们追加在 `Image.gz-dtb` 尾部的 dtb。**

CAF 的 ABL 从 **boot.img 的 dt 分区**里按 `msm-id` / `board-id`
挑一个 dtb，然后把地址放进 `x0` 传给内核。
而 AnyKernel3 的 `flash_boot()` 是这么做的：

```sh
for fdt in dt recovery_dtbo dtb; do
  for i in $AKHOME/$fdt $AKHOME/$fdt.img $SPLITIMG/$fdt; do
    if [ -f $i ]; then eval local $fdt=$i; break; fi
...
[ "$dt" ] && dt="--dt $dt";
mkbootimg --kernel $kernel ... $dt ...
```

`split_boot()` 解包原厂 boot.img 时会把原厂的 dt 存成 `$SPLITIMG/dt`，
于是 `flash_boot()` 拿**原厂的 dt** 传给 mkbootimg。
**我们追加在 kernel 尾部的 dtb 完全被忽略。**

铁证（诊断内核的日志）：

```
Machine model: Qualcomm Technologies, Inc. MSM8953 + PMI8950 MTP
                ↑ 这是 CAF DTS 的 model，我们的 DTB 里写的是 "OPPO R9sk"
OF: reserved mem: node venus_region@0 ...
/soc/wake-gic@601d4: ...
                ↑ 这些节点名只在 CAF DTS 里存在，我们的 DTB 里没有
```

这解释了全部现象：
- 改 DTB 无效
- `arch_timer: No interrupt available`（CAF DTB 的 timer 节点配不上 mainline 驱动）
- 4.19 能启动（CAF 内核 + CAF DTB 本来就配套）
- 6.12 不能启动（mainline 驱动树 + CAF DTB 不匹配）

**修复**：`ci/mkflashable.sh` 新增 `ak3-oppo-replace-dt.sh`，
在 `dump_boot` 之后（`SPLITIMG/dt` 已解出）、`write_boot` 之前执行，
按 board-id 选机型并把我们的 dtb 覆盖进去。三种情况都处理：

| 原厂 dt 格式 | 处理 |
|---|---|
| `QCDT`（`51434454`） | 用 Python 重新生成只含我们 dtb 的 QCDT 表 |
| 裸 FDT（`d00dfeed`） | 直接 `cp` 覆盖 |
| 没有独立 dt | 自己创建（裸 FDT） |

**验证方法**：刷机后看日志里的 `Machine model`。
如果出现 `OPPO R9sk` 就说明 DTB 终于生效了；如果还是 CAF 的 MTP，就说明替换没成功。

### 刷机前

1. **备份原 boot 分区**（TWRP 里 backup boot）—— 卡屏时靠它回滚
2. 确认机型：`adb shell getprop ro.product.device`
   - 16027 → `msm8953-oppo-r9sk`（用户的机器是这个）
   - 16017 → `msm8953-oppo-r9s`
3. 刷机包会读 `/proc/device-tree/qcom,board-id` 自动选 dtb，
   读不到默认 r9sk

### 刷完之后的三种结果，分别怎么办

#### A. 正常进系统

那是意外之喜。立刻收集：

```bash
dmesg > /sdcard/dmesg.txt
cat /sys/class/power_supply/*/uevent  > /sdcard/power.txt
cat /proc/device-tree/qcom,board-id | od -An -tx1
```

#### B. 卡一屏不动（这次的情况）

**关键：不要立刻重刷。** 先重启一次进 TWRP，把 pstore 抓出来：

```bash
# TWRP 的终端里
mount /data
ls -la /sys/fs/pstore/
# 有内容就说明 pstore 修好了，直接看：
cat /sys/fs/pstore/console-ramoops-0
cat /sys/fs/pstore/pmsg-ramoops-0
```

把 `console-ramoops-0` 整份发出来 —— 里面有 6.12 的完整 dmesg，
能直接定位卡在哪个子系统。

> 如果 `/sys/fs/pstore` 还是空的，说明内核连 pstore 初始化都没走到
> （即死得比 pstore 更早）。那说明还有比 VA_BITS 更早的问题，
> 需要换手段（见下）。

#### C. 进 fastboot

说明 bootloader 阶段就没过去，大概率是 `Image.gz-dtb` 尺寸或
DTB 结构问题。CI 现在会打印：

    Image(未压缩) = 57420288 bytes (54 MiB)
    Image.gz      = 17806276 bytes

`Image.gz-dtb`（约 17.8 MB）能放进 32 MB 的 boot 分区，这一项是安全的。
若仍进 fastboot，换另一机型的 dtb 再试一次。

### 诊断内核（最小配置，兜底手段）

如果正常内核仍然卡屏、而且 pstore 里拿不到 6.12 的日志，
用这个诊断内核先证明"内核能起来"，再逐项加回子系统定位。

CI 里手动触发（Actions → 6.12 Android OPPO MSM8953 → Run workflow →
variant 选 `diag`），产物名为
`r9s-android-6.12-diag-<ver>-<date>.zip`。

| 项目 | 正常内核 | 诊断内核 |
|---|---|---|
| `CONFIG_*=y` 数量 | 3301 | 约 800 |
| Image 大小 | 55.6 MiB | **10 MiB** |
| 显示/音频/充电/GPU | 全开 | **全关** |
| pstore | 全内置 | 全内置 |
| USB/adb | 开 | 开（便于直接抓 dmesg） |

关掉的都是"卡屏嫌疑人"：`DRM_MSM`、`QCOM_APR`、`SND_SOC_QDSP6`、
`QPNP_SMBCHARGER`、`QCOM_KGSL`。CI 会反过来断言它们**确实关闭**，
保证每次出的都是真正的最小集。

如果诊断内核能起、正常内核不能，那就是某一个被关掉的子系统的问题，
二分几次就能定位。如果诊断内核也卡，问题就在更底层的平台代码。

### 若 pstore 也拿不到日志时的兜底

内核死得比 pstore 更早（比如 `head.S` 阶段），就只剩两条路：

1. **串口**：R9s 主板有 UART 测试点，需要拆机接线，代价大
2. **二分法缩小**：先用最小 config（只留 display + serial + pstore）
   确认能起，再逐项加回来。这个我可以帮你做，但需要你配合刷 2-3 次

### 各子系统的排查关键字

一旦能拿到 dmesg：

```bash
dmesg | grep -iE "msm_drm|dsi|panel"      # 显示
dmesg | grep -iE "rmi4|synaptics|touch"   # 触摸
dmesg | grep -iE "bq27|power_supply"      # 电量（设备实际是 bq27411）
dmesg | grep -iE "ak4376|q6afe|q6asm"     # 音频
dmesg | grep -iE "smbchg|qpnp"            # 充电
```

### 已知但未修的问题

| 问题 | 说明 |
|---|---|
| SD 卡电源脚 | 4.19 有 `sdcard_vdd_enable`（gpio 64），mainline 的 `sdhci-msm` 无对应 DT 属性，可能要改驱动 |
| 触摸 S3320 | 可能不走标准 RMI4 协议，是最大未知数 |
| GPU (KGSL) | 47 处 API 差异未清，配置关闭中 |
| 外放 TFA98XX | 2.5 万行独立栈，未开始 |
| 相机 | 未开始 |

---

## 还差什么才能跑 Android

ACK 给了 Android **内核接口**，主线给了 MSM8953 **平台**，
但高通的 **vendor 栈**只有 4.19 有。这些必须从 4.9/4.19 前移过来：

| 子系统 | 作用 | 缺了会怎样 |
|---|---|---|
| KGSL (`drivers/gpu/msm`) | 高通 GPU 驱动，gralloc/HWC 依赖 | 无硬件加速，SurfaceFlinger 起不来 |
| msm_ion (`ION_IOC_*`) | 高通 ION 内存分配 | 图形/相机 buffer 无法分配 |
| CAMSS/CCI + OPPO 私有上电 | 摄像头 | 相机不可用 |
| Q6 音频 (apr/q6asm + cal) | 音频路由 | 无声 |
| qpnp-smb2 / qpnp-fg | 充电、电量计 | 电量显示异常、充电不受控 |
| MDSS/DSI + OPPO 面板 | 显示控制器 | 只有 bootloader 留下的静态 framebuffer |
| Synaptics s3320 / fpc1020 | 触摸、指纹 | 输入不可用 |

**建议的移植顺序**（每步都能单独验证）：

1. **KGSL** —— 最关键，Android 图形栈的地基。4.19 的 `drivers/gpu/msm`
   需要大改（`mmap_lock`、`dma_buf` API、`file_operations`），但结构还算接近
2. **msm_ion → dmabuf heaps 桥接** —— 让 HAL 的 ION ioctl 落到主线 dmabuf heaps
3. **MDSS/DSI** —— 参考 mainline 已有的 `msm` DRM 驱动，再叠加 OPPO 面板初始化序列
   （`panel-oppo-ea8064` / `panel-oppo-jdi-r63452` 两个驱动已在
   `oppo-r9s-mainline` 仓库写好，可以直接搬过来用）
4. **qpnp 充电/电量计**
5. **Q6 音频**
6. **触摸/指纹**
7. **相机**

---

## 刷机包

CI 会额外产出 **AnyKernel3 刷机包**（`r9s-android-6.12-<kver>-<date>.zip`），
直接 TWRP 刷入即可，只替换 boot 分区里的内核+dtb，不动其它分区。

打包脚本是 `ci/mkflashable.sh`，要点：

- `BLOCK=/dev/block/bootdevice/by-name/boot`（注意变量名是**大写**）
- `do.devicecheck=0`：不靠机型字符串校验，因为 R9s/R9sk 的 `ro.product.device`
  在不同 ROM 上写法不统一
- `do.modules=0` / `do.systemless=0`：本内核不带模块
- 包内同时放两个 dtb，安装时由 `ak3-oppo-dtb.sh` 读
  `/proc/device-tree/qcom,board-id` 自动选：
  `0x3e91` → r9s(16017)，`0x3e9b` → r9sk(16027)，读不到默认 r9sk

> 本地没有 `zip` 命令时脚本会退化用 python 的 zipfile，CI 上装了 `zip`。

## 构建

```bash
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- msm8953-oppo_defconfig
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -j$(nproc) Image dtbs

# 只编设备树（改 DTS 时用这个，几秒）
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- qcom/msm8953-oppo-r9sk.dtb
```

CI 用 GCC 而不是 clang：ACK 的 GKI 配置面向 clang（LTO/CFI），
我们走自定义 defconfig，不碰那些 clang-only 特性，GCC 够用且 runner 自带。

---

## 状态标注

- `[ack]` = 来自 ACK 上游，可信
- `[vendor]` = 来自本仓库 4.9/4.19 下游树
- `FIXME(unverified)` = 未真机验证

**当前阶段：内核能编译出完整产物（ACK 6.12.52 + OPPO R9s 板级），
但还没有任何 vendor 驱动，真机启动未验证。**
