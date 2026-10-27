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
