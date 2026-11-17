# 4.19 — OPPO MSM8937/MSM8953 内核 4.9 → 4.19 移植

本分支把 `bakasu-susfs-bpf`（4.9.337）上的 OPPO 机型支持，移植到
**Linux 4.19.325-cip136** 基线上。目标机型是 **OPPO R9s（16017 / 16027，
MSM8953 / 骁龙 625）**。

> ✅ **状态：真机已验证可以开机。** 详见下方「已验证 / 待确认」。

## 基线来源

```
4.19 基线：https://github.com/imren0x/msm-4.19 @ mi8953/latest (b05d368e0e48)
          = LineageOS/android_kernel_qcom_sdm660 lineage-20
            + CIP 4.19.325-cip136 合并后的统一 MSM8953/8937/450 内核
```

以**全新根提交**导入，与 4.9 各分支历史相互独立（4.9 分支不受影响）。

基线本身已自带：

- 完整 MSM8953 SoC 支持（`ARCH_MSM8953` / `ARCH_SDM450` / `ARCH_MSM8937`）
- `drivers/kernelsu`（KSU_VERSION **32567**，非 4.9 分支的 BakaSU 35212）
- `techpack/xiaomi-msm8953`（小米机型支持）

所以本分支的工作重点是**把 OPPO 机型支持重新移植上来**，而不是从零引入 root 方案。

## 分支结构

| 提交 | 内容 |
|---|---|
| `import:` | 4.19.325-cip136 msm8953 基线（原样） |
| `ci:` | 构建工作流 `.github/workflows/build-4.19.yml` |
| `port(oppo):` | OPPO R9s 设备树 + 驱动移植 |
| `fix(oppo):` | 4.9 → 4.19 的 API 断点修正 |

## 构建

用 GitHub Actions（`.github/workflows/build-4.19.yml`）。

`lineage-22.1-k4.19` 是仓库默认分支，`workflow_dispatch` 可直接触发，也可以走 gh CLI：

```sh
# 纯净内核（R9s + A57）
gh workflow run build-4.19.yml --ref lineage-22.1-k4.19 \
  -f build_ref=lineage-22.1-k4.19 -F r9s=true -F a57=true

# BakaSU + SUSFS 内核（走 bakasu-k4.19 分支）
gh workflow run build-4.19.yml --ref bakasu-k4.19 \
  -f build_ref=bakasu-k4.19 -F r9s=true -F a57=true
```

> 两个 run 必须用**不同的 `--ref`**，否则会被 workflow 的 `concurrency`
> （分组里含 `github.ref`）互相取消。

`ci/targets` 只在 **push 事件**下生效——push 触发器只监听本默认分支的 `ci/**`
改动，改这个文件并 push 即触发一次构建。

| target | 说明 |
|---|---|
| `base` | 4.19 基线冒烟（`vendor/msm8953-perf_defconfig` + `vendor/mi8953.config`） |
| `R9s` | OPPO R9s 纯净内核（+ `oppo-common` / `oppo-r9s` / `oppo-no-ksu` / `oppo-bpf-containers`） |
| `A57` | OPPO A57 纯净内核（+ `oppo-common` / `oppo-a57` / `oppo-no-ksu` / `oppo-bpf-containers`） |
| `R9s-bakasu` / `A57-bakasu` | BakaSU + SUSFS，只在 `bakasu-k4.19` 分支上出 |

> ⚠️ `publish_release` 输入默认是 `true`，而 release 步骤用的
> `ncipollo/release-action` **没有设 `draft`** —— 也就是会直接发一个公开 Release。
> 只想验证编译时显式传 `-F publish_release=false`。

### 本地构建

```sh
make O=out ARCH=arm64 -j1 \
     vendor/msm8953-perf_defconfig \
     vendor/mi8953.config \
     vendor/oppo-common.config \
     vendor/oppo-r9s.config \
     vendor/oppo-no-ksu.config \
     vendor/oppo-bpf-containers.config
make O=out ARCH=arm64 -j"$(nproc)" CC=clang \
     CLANG_TRIPLE=aarch64-linux-gnu- CROSS_COMPILE=aarch64-linux-gnu- \
     Image.gz-dtb
```

## ⚠️ `vendor/mi8953.config` 是必需项，不是可选项

`drivers/power/supply/qcom/qpnp-fg.c` 在**没有** `IS_ENABLED(CONFIG_MACH_XIAOMI_MSM8953)`
保护的情况下直接调用 `xiaomi_msm8953_mach_get()` / `XIAOMI_MSM8953_MACH_*`。
因此不合并 `vendor/mi8953.config`（即 `CONFIG_MACH_XIAOMI_MSM8953=y`）时，
**基线自身就编不过**：

```
error: implicit declaration of function 'xiaomi_msm8953_mach_get'
error: use of undeclared identifier 'XIAOMI_MSM8953_MACH_MIDO'
```

**而且不能"顺手把小米机型关掉来瘦身"。** 4.19 基线里有些**使用点没有加
`CONFIG_MACH_XIAOMI_*` 保护**，但对应变量的**定义**却在这些保护里面，例如：

| 文件 | 声明处（有保护） | 使用处（**无**保护） |
|---|---|---|
| `drivers/video/fbdev/msm/mdss_dsi.c` | `vspn_power_state`、`change_par_ctrl` 在 `#if IS_ENABLED(CONFIG_MACH_XIAOMI_VINCE)` 内 | `:541`、`:3717`、`:3964` 直接使用 |
| `techpack/camera-legacy/sensor/flash/msm_flash.c` | `flashlight_brightness_value` 在 `CONFIG_MACH_XIAOMI_VINCE` 内 | `:795` 直接使用 |
| `techpack/audio-legacy/asoc/msm8952.c` | `EXT_PA_MODE` | `:418` 直接使用 |

所以 `CONFIG_MACH_XIAOMI_VINCE=y`（以及 `mi8953.config` 里的整套小米机型开关）
是**编译所必需**的。曾尝试只保留平台符号、关掉具体机型，结果就是上面这 5 个
`undeclared` 错误。**结论：原样保留 `vendor/mi8953.config` 的全部机型开关。**
（代价是会多编 4 个用不到的小米 DTB；要清理得逐个给那些使用点补上保护，
属于独立的重构，不在本次移植范围内。）

本仓库面向 OPPO、不启用任何小米机型，但该符号仍必须为 `y`，
好让 `qpnp-fg` 走 `mach_get() == UNKNOWN` 的分支。

## 移植过程中修掉的 4.9 → 4.19 断点

这些都用**真实内核构建系统**逐个目标编译验证过（不是推测）：

| # | 问题 | 修法 |
|---|---|---|
| 1 | 4.9 的 `<asm/uaccess.h>` 在 arm64 4.19 上不再传递 `<linux/uaccess.h>`，`copy_to_user` 等变成隐式声明 | 按 4.19 树惯例（1120 处）统一改 `<linux/uaccess.h>` |
| 2 | arm64 4.19 置了 `CONFIG_ARCH_HAS_SYSCALL_WRAPPER=y`，`include/linux/syscalls.h` 把 `sys_open/sys_write/sys_close` 放在 `#ifndef CONFIG_ARCH_HAS_SYSCALL_WRAPPER` 里 | 改用 `ksys_open()/ksys_write()/ksys_close()`（21 处 / 3 文件） |
| 3 | `smem_alloc()` 在 4.19 已移除，`<soc/qcom/smem.h>` 也已移走，`SMEM_ID_VENDOR1/2` 枚举被裁掉 | 改用 `qcom_smem_get(QCOM_SMEM_HOST_ANY, ...)`；item 号补进 `include/soc/oppo/oppo_smem_ids.h`（用编译器求值确认 VENDOR1=135、VENDOR2=136） |
| 4 | 4.19 的 `qpnp-fg.c` 缺 OPPO gauge 后端接口，`oppo_bq27541` 会 undefined symbol | 补回 `fg_oppo_register()`/`fg_oppo_unregister()` 及 4 处取值钩子 |
| 5 | `merge_config.sh` 的 `OUTPUT` 默认是 `.`，在 `O=out` 之外手工调用会把 `.config` 写进**源码树**，触发 `prepare3` 的 `.. is not clean` 检查 | 把 defconfig 与所有片段交给**一次 make 调用**，用内核自带的 `%.config` 规则合并 |

### ⚠️ 写 config 片段时最大的坑：注释也会被当成配置指令

`scripts/kconfig/merge_config.sh` 用这条 sed 抽取配置名：

```sh
SED_CONFIG_EXP="s/^\(# \)\{0,1\}\(CONFIG_[a-zA-Z0-9_]*\)[= ].*/\2/p"
```

凡是匹配 `^# CONFIG_XXX=...` 的行都会被当成一条**配置指令**，接着执行

```sh
sed -i "/$CFG[ =]/d" $TMP_FILE
```

把它从合并结果里**删掉**。也就是说，一句看起来无害的说明性注释

```
# CONFIG_MACH_XIAOMI_MSM8953=y，其余具体机型全部关掉
```

会把 `CONFIG_MACH_XIAOMI_MSM8953` 变成**未设置**（不是被忽略！），
从而让 `qpnp-fg.c` 编译失败。本项目实际踩过这个坑。

**规则：描述某个符号时，句首不要紧跟 `CONFIG_`。**
（`arch/arm64/configs/vendor/oppo-r9s.config` 顶部有同样的警告。）

## 已验证 / 待确认

### 已验证

- **真机可以开机**（作者确认）
- 4.19 基线在 CI 上编译通过
- OPPO 驱动逐个目标编译通过（`soc/oppo` ×3、`power/supply/oppo` ×2、
  `synaptics_oppo` ×5、`oppo_touchscreen` ×3、含 OPPO 钩子的 `qpnp-fg.o`）
- 完整 `Image.gz-dtb` 本地构建成功（20 MiB），OPPO 符号已链入 `vmlinux`
  （`bq27541`、`vooc`、`oppo_project`、`get_boot_mode`）
- 两个 OPPO DTB（`msm8953-mtp_16017.dtb`、`msm8953-mtp_16027.dtb`）编译成功，
  且其内容（`oppo16027jdi_r63452`、`oppo16017samsung_ea8064`、
  `oppo,bq27541-battery`、`oppo,stm8s-vooc`、`synaptics,s1302`）
  已确认嵌进 `Image.gz-dtb`
- CI 覆盖 R9s / A57 两个机型、纯净（`lineage-22.1-k4.19`）与
  BakaSU + SUSFS（`bakasu-k4.19`）两条产物线，全部编译通过

### 待确认（以下项目缺逐项实测记录）

- **面板**：据 `XuexGao/oppo-r9s-mainline` 的实机记录，真机是 **16027 / JDI
  R63452**（不是 16017 的 Samsung EA8064 AMOLED）。两个 DTB 都已编出且能开机，
  但「哪块 DTB 对应哪一批次的屏」还要逐台核对。
- **仍未移植的驱动**：`KTD2026_SLED`（指示灯）——4.19 基线没有对应驱动，
  所以 `oppo-r9s.config` 里没有声明（该文件末尾有说明）。其余
  `AK4376` / `TFA98XX` / `FPC1020` / `LM3697` 都已移植并在 config 里打开。
- **root 方案版本不一致**：基线自带 KernelSU `KSU_VERSION=32567`，
  而 4.9 分支用的是 BakaSU 35212 + SUSFS。管理器版本码需与内核对齐。
- **`vooc_charger.c` 的既有缺陷**（4.9 分支就有，非本次引入）：
  `vooc_poll_func()` 里 `data |= ...` 前未初始化；若干 `dev_info` 格式串
  与参数不匹配（`-Wformat-extra-args`）。只报警告，不阻断编译。

## 相关仓库

- 4.9 原树：`XuexGao/android_kernel_oppo_msm8937` @ `bakasu-susfs-bpf`
- 主线 bring-up（含权威硬件记录）：`XuexGao/oppo-r9s-mainline`
- 4.19 基线：`imren0x/msm-4.19` @ `mi8953/latest`
