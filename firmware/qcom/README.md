# GPU 微码（内置固件）

这两个文件通过 `CONFIG_EXTRA_FIRMWARE` 编译进内核本体，不放在文件系统里。

## 为什么必须内置

DRM adreno 的 a5xx 驱动在 probe 阶段（开机约 2.3 s）就加载微码，
而 Android 的 `/vendor/firmware_mnt` 要到约 74 s 才被 init 挂载。
时间差导致真机报：

```
Direct firmware load for qcom/a530_pm4.fw failed with error -2
[drm:adreno_request_fw] *ERROR* failed to load a530_pm4.fw
```

GPU 初始化失败 → SurfaceFlinger 拿不到 EGLConfig → 黑屏。

4.19 的 KGSL 是懒加载，不受影响；6.12 的 adreno 是 eager load，
`-ENOENT` 后不会重试。`EXTRA_FIRMWARE` 在
`_request_firmware_prepare()` 里最先匹配（`firmware_request_builtin_buf`），
完全不碰文件系统，所以能绕开这个时序问题。

## 文件来源

来自上游 `linux-firmware` 仓库（Adreno 505/506/508 共用 a530 微码，
见 `drivers/gpu/drm/msm/adreno/a5xx_catalog.c` 里 revn 506 的 `fw` 表）：

- `https://git.kernel.org/pub/scm/linux/kernel/git/firmware/linux-firmware.git/plain/qcom/a530_pm4.fw`
- `https://git.kernel.org/pub/scm/linux/kernel/git/firmware/linux-firmware.git/plain/qcom/a530_pfp.fw`

| 文件 | 大小 | sha256 前 16 位 |
|---|---|---|
| a530_pm4.fw | 19572 | 6419f35956ec7307 |
| a530_pfp.fw | 16144 | 7ab3cd917e1f875f |

结构自检：两文件的前 516 字节是头，`dword[2]` 为 payload 的 dword 数，
`0x129c*4 + 516 = 19572`、`0xf43*4 + 516 = 16144`，与文件大小精确吻合。

> **许可提醒**：这是高通专有二进制固件，由 linux-firmware 按其分发条款再分发。
> 内核 `CONFIG_EXTRA_FIRMWARE` 的 Kconfig 明确警告：把非 GPL 固件编进内核
> 会形成 GPL 与非 GPL 的混合产物。自用设备内核常见此做法；
> 若要公开分发整包镜像，请自行评估许可影响。