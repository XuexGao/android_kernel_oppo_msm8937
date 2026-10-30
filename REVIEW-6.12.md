# 6.12-test 分支代码审查报告

审查对象：`6.12-test` 分支（HEAD = `899ff4533`）
审查方式：纯只读 + 静态分析。用 `git archive 6.12-test` 取出一份干净的树到 `/tmp/rev`
（审查期间工作区被另一个并发任务 `git checkout` 切走了，直接读工作区不可靠），
再用 `cpp` + `scripts/dtc/dtc` 对 DTS 做了实际编译/反编译，用 `python3` 解析了
`mmzone.h` / `vmstat.c` / `power_supply.h` 的枚举与数组顺序，用真实
`scripts/Makefile.build` 复现了 kbuild 规则解析。

**没有运行 `make`，没有修改任何源码。** 唯一的写入是本文件。

---

## 摘要

| 严重程度 | 条目数 | 关键项 |
|---|---|---|
| A. 会导致编译/构建失败 | 4 | `Image.gz-dtb` 规则缺 `dtbs` 目标（**已实测复现**）；`select MSM_RPM_SMD` 指向不存在的符号；`drivers/platform/oppo` 未纳入构建 |
| B. 会导致启动失败 | 5（+1 条「已确认无问题」） | DSI PHY 仍 `disabled`（屏永远不亮）；面板 graph 端点不成对；AK4376 节点挂错总线 |
| C. 会导致功能异常 | 11 | `PSTORE_DEFLATE_COMPRESS` / `QCOM_RPMCC` / `USB_CONFIGFS_F_ADB` 等符号不存在；`gpio-keys` 缺 down 键 pinctrl；`device_info` 死代码；`bq27541` UAF |
| D. 代码质量问题 | 16（+1 条「已排除」） | **`power_supply_attrs[]` 越界读致 panic**；**`vmstat_text[]` 顺序错位 4 项**；`ci/targets` 垃圾文件等 |

**最需要注意的两条**：

1. **A-1**：`arch/arm64/boot/Makefile:101` 的
   `$(obj)/Image.gz-dtb: $(obj)/Image.gz dtbs FORCE` 在 kbuild 子 make 上下文里
   `dtbs` **没有对应规则**，会直接 `No rule to make target 'dtbs'`。
   已用真实 `scripts/Makefile.build` 复现。
2. **D-2**：`power_supply_attrs[]` 只有 78 项，而 `qpnp-smbcharger.c:5672`
   把 `POWER_SUPPLY_PROP_REAL_TYPE`（idx **82**）放进了 `properties[]`
   → `add_prop_uevent()` 数组越界读 → **开机 uevent 就 panic**。

---

# A. 会导致编译/构建失败

## A-1【致命】`arch/arm64/boot/Makefile:101` — `dtbs` 依赖在子 make 里无规则

**位置**：`arch/arm64/boot/Makefile:96-111`（`CAF_DTB_OBJS` 定义与规则）

```
 96: CAF_DTB_OBJS = $$(find $(obj)/dts/ -name '*.dtb' 2>/dev/null | sort)
 98: targets += Image.gz-dtb
100:
101: $(obj)/Image.gz-dtb: $(obj)/Image.gz dtbs FORCE
102: 	$(Q)set -e; \
```

**问题**：`arch/arm64/boot/Makefile` 是被 `scripts/Makefile.build` 在**子 make** 里
`include` 的（`$(Q)$(MAKE) $(build)=$(boot) $(boot)/$@`）。在这个子 make 的上下文里：

- `dtbs` 不是 `.PHONY`（`Makefile.build` 的 `PHONY` 只包含 `$(obj)/`、
  `$(subdir-ym)`、`FORCE`、`$(single-subdir-goals)`）；
- 也没有任何一条规则定义 `dtbs`（`scripts/Makefile.dtbs` 里只有
  `$(obj)/dtbs-list` / `$(obj)/%.dtb` 等，没有裸 `dtbs`）；
- 顶层 `Makefile:1462` 的 `dtbs:` 规则只存在于**顶层 make**，不会传到子 make。

我用真实的 `scripts/Makefile.build` + 真实的 `arch/arm64/boot/Makefile`
在 `/tmp/kbt` 里复现了：

```
$ make -f .../scripts/Makefile.build ... /tmp/kbt/obj/arch/arm64/boot/Image.gz-dtb
make: *** No rule to make target 'dtbs', needed by
        '/tmp/kbt/obj/arch/arm64/boot/Image.gz-dtb'.  Stop.
```

**这会让 CI 的「编译内核与设备树」这一步直接失败。**

注意这里有个容易被误判的地方：4.9/4.19 的 `Image.gz-dtb` 规则看起来也是
`$(obj)/Image.gz $(DTB_OBJS) FORCE`，但 4.19 的 `dtbs` 目标是在
**`arch/arm64/Makefile`（顶层上下文）** 里定义的，并且
`Image.gz-dtb: vmlinux scripts dtbs Image.gz` 这条规则也在**顶层**
（见 4.19 `arch/arm64/Makefile:168-178`），子 make 只负责最后的 `cat`。
本分支把两者合并到 boot/Makefile 里，就丢了 `dtbs` 的上下文。

**建议修法**（任选其一，推荐第 1 种）：

1. **在 boot/Makefile 里显式声明 `dtbs` 为空目标**，让子 make 有规则可用：
   ```make
   PHONY += dtbs
   dtbs:
   	$(Q)$(MAKE) $(build)=$(obj)/dts need-dtbslist=1
   ```
2. **把 `dtbs` 依赖移到顶层 `arch/arm64/Makefile`**，照 4.19 的写法：
   ```make
   # arch/arm64/Makefile
   Image.gz-dtb: vmlinux dtbs Image.gz
   	$(Q)$(MAKE) $(build)=$(boot) $(boot)/$@
   ```
   然后 boot/Makefile 里去掉 `dtbs` 只留 `$(obj)/Image.gz`。
3. 最省事的兜底：把 `dtbs` 从 prerequisites 删掉，改成在 recipe 里
   `$(MAKE) -C $(srctree) ARCH=$(ARCH) ... dtbs`（不推荐，容易递归）。

同时 `targets += Image.gz-dtb` 之后，`$(call if_changed,cat)` 用的是
`.Image.gz-dtb.cmd` 记录命令行；改成裸 shell recipe 后这个 cmd 文件不会更新，
增量构建时**每次都会重跑**（不算错，但如果想保持 kbuild 风格，应该用
`$(call if_changed,cat)` + 在 recipe 里先算好 DTB 列表）。

---

## A-2【致命】`drivers/soc/qcom/Kconfig:303` — `select MSM_RPM_SMD` 指向不存在的符号

**位置**：`drivers/soc/qcom/Kconfig:299-310`（`config QCOM_BUS_SCALING`）

```
299: config QCOM_BUS_SCALING
300: 	bool "QCOM bus scaling driver"
301: 	depends on ARCH_QCOM
302: 	select MSM_RPM_SMD          ← 这个符号在 6.12 树里不存在
303: 	help
```

**问题**：`MSM_RPM_SMD` 在整个 6.12 树里**没有任何 Kconfig 定义**。
我扫了全部 1826 个 `Kconfig*` 文件，`config MSM_RPM_SMD` 出现 0 次
（只有 `QCOM_SMD_RPM`、`QCOM_RPM_MASTER_STATS`、`QCOM_RPMH`）。

但树里有三处在用它：
- `arch/arm64/configs/msm8953-oppo_defconfig:1757`（`CONFIG_MSM_RPM_SMD=y`）
- `arch/arm64/configs/msm8953-oppo-diag_defconfig:70`（`CONFIG_MSM_RPM_SMD=y`）
- `drivers/soc/qcom/msm_bus/Makefile:7`（`obj-$(CONFIG_MSM_RPM_SMD) += msm_bus_rpm_smd.o`）
- `drivers/soc/qcom/msm_bus/msm_bus_core.h:295`（`#if defined(CONFIG_MSM_RPM_SMD)`）

后果分两层：

1. **`select` 一个不存在的符号**：kconfig 会给出
   `WARNING: unmet direct dependencies detected for MSM_RPM_SMD` 之类的警告
   （`sym_warn_unmet_dep()`，`scripts/kconfig/symbol.c:360`），但**不会**中止构建
   （除非设了 `CONFIG_WERROR`）。所以这条单独不会 fail。
2. **真正致命的是连锁**：`msm_bus_rpm_smd.c` **永远编不进去**，于是
   `msm_bus_rpm_hw_init()` / `msm_bus_remote_hw_commit()` /
   `msm_bus_rpm_fill_cdata_buffer()` 三个函数**没有实现**。
   `msm_bus_core.h:295-318` 的 `#if defined(CONFIG_MSM_RPM_SMD)` 分支走
   `#else`（inline 空实现），所以**能过编译**，但运行时 bus scaling 全是空操作
   → 所有 `msm_bus_scale_*` 调用（KGSL、display、camera）不生效。

**建议修法**：

```kconfig
# drivers/soc/qcom/Kconfig
config QCOM_BUS_SCALING
	bool "QCOM bus scaling driver"
	depends on ARCH_QCOM
	depends on QCOM_SMD_RPM      # R9s 是 RPM-SMD 平台，不用 RPMh
	select QCOM_BUS_CONFIG_RPM_SMD   # 或者直接把 msm_bus_rpm_smd.o 改成 obj-y
```

更干净的做法是**不要新增符号**，直接复用已有的 `QCOM_SMD_RPM`：

```make
# drivers/soc/qcom/msm_bus/Makefile
obj-$(CONFIG_QCOM_SMD_RPM) += msm_bus_rpm_smd.o
```
```c
/* drivers/soc/qcom/msm_bus/msm_bus_core.h:295 */
#if defined(CONFIG_QCOM_SMD_RPM)
```
然后从两个 defconfig 里删掉 `CONFIG_MSM_RPM_SMD=y`。

---

## A-3【致命】`drivers/platform/oppo/` 未被纳入构建

**位置**：`drivers/platform/oppo/Kconfig`（整个文件）、`drivers/platform/Makefile`

**问题**：
- `drivers/platform/Kconfig` 里有 `source "drivers/platform/oppo/Kconfig"`（✓ 已加）
- 但 `drivers/platform/Makefile` 里**没有** `obj-$(CONFIG_MACH_OPPO) += oppo/`
- 而且 `drivers/platform/oppo/` 目录下**连 `Makefile` 都没有**

我确认过：
```
drivers/platform/oppo/Makefile  →  MISSING
drivers/platform/Makefile       →  grep oppo 结果为空
```

也就是说 `MACH_OPPO` / `MACH_16017` / `MACH_16027` / `MACH_OPPO_MSM8937`
这几个菜单项**纯属摆设**：选了什么都不编。

**为什么现在还能"编过"**：`MACH_OPPO` 真正起作用的地方是
`drivers/soc/Makefile:28`、`drivers/power/supply/Makefile:89`、
`drivers/soc/oppo/Makefile:1` 这三处，它们只依赖 Kconfig 里的符号值，
不依赖 `drivers/platform/oppo/` 里有东西。所以是"能过但不完整"。

**建议修法**（二选一）：

1. 如果 `drivers/platform/oppo/` 确实是空的（只有 Kconfig），那
   `source "drivers/platform/oppo/Kconfig"` 就该删掉，
   把 `MACH_OPPO` / `MACH_16017` 等移到 `drivers/soc/oppo/Kconfig` 或
   `arch/arm64/Kconfig.platforms` 里（语义上更对，因为它们不是
   "platform driver"）。
2. 如果打算将来往里放驱动，那现在就得建 `drivers/platform/oppo/Makefile`
   并在 `drivers/platform/Makefile` 加一行。

同时注意 `drivers/platform/Kconfig` 是**无条件 source** 的，
而 `MACH_16061` 的 help 里写着
`板级 DTS 见 arch/arm64/boot/dts/vendor/qcom/oppo-msm8937/` ——
**这个路径在 6.12-test 分支里不存在**（那是 4.19 分支的路径）。见 D-3。

---

## A-4【高】`arch/arm64/boot/Makefile` 的 `Image.gz-dtb` 未加入 `BOOT_TARGETS`

**位置**：`arch/arm64/boot/Makefile:98`、`arch/arm64/Makefile:171`

**问题**：`arch/arm64/Makefile:171` 是
```make
BOOT_TARGETS := Image vmlinuz.efi image.fit
```
`Image.gz` 和 `Image.gz-dtb` 都不在里面。`Image.%: Image` 这条通用规则
（`arch/arm64/Makefile:192-193`）能覆盖 `Image.gz`，但**不能覆盖
`Image.gz-dtb`** —— 因为 `Image.%` 匹配的是 `Image.<单段>`，
`Image.gz-dtb` 在 `%` 里要匹配 `gz-dtb`，实际**能**匹配上
（`%` 可以包含 `-`），所以 `make Image.gz-dtb` 目前是靠这条通用规则
落进 `$(boot)/Image.gz-dtb` 的。

这意味着：**`Image.gz-dtb` 规则只在"用户显式 `make Image.gz-dtb`"时触发**，
`make all` / `make Image.gz` 都不会产出它。CI 里写的是
`make ... Image.gz-dtb dtbs`，目前是显式的所以能跑；但
`arch/arm64/boot/Makefile` 里 `targets += Image.gz-dtb` 这行的清理/增量
语义就不完整了（`make clean` 不会删它）。

**建议修法**：
```make
# arch/arm64/Makefile
BOOT_TARGETS := Image Image.gz-dtb vmlinuz.efi image.fit
```
或者至少在 `arch/arm64/boot/.gitignore` 里补 `Image.gz-dtb`。

---

# B. 会导致启动失败

## B-1【致命】`msm8953-oppo-common.dtsi:435-442` — DSI PHY 仍为 `disabled`，DSI 无法 probe

**位置**：`arch/arm64/boot/dts/qcom/msm8953-oppo-common.dtsi:427-442`

```
427: &mdss {      status = "okay"; };
431: &mdp  {      status = "okay"; };
435: &mdss_dsi0 {
436: 	status = "okay";
437:
438: 	/*
439: 	 * 具体面板节点放在各机型 dts 里（r9s 是 EA8064，r9sk 是 R63452），
440: 	 * 因为它们 compatile 不同。
441: 	 */
442: };
```

**问题**：只打开了 `&mdss` / `&mdp` / `&mdss_dsi0`，**漏了 `&mdss_dsi0_phy`**。
`msm8953.dtsi:966-984` 里 phy 节点带 `status = "disabled"`。

我把 DTS 实际编译出来验证了（`cpp` + `scripts/dtc/dtc` 反编译）：

```
phy@1a94400 {
	compatible = "qcom,dsi-phy-14nm-8953";
	...
	status = "disabled";      ← 仍然是 disabled
	phandle = <0x1c>;
};
```

而 `mdss_dsi0` 的 `phys = <&mdss_dsi0_phy>` 指向它。

**后果链**：
1. `of_platform_device_create_pdata()` 对 `status="disabled"` 的节点
   直接 `return NULL`（`drivers/of/platform.c`，`of_device_is_available()` 检查），
   所以 **DSI PHY 的 platform_device 根本不会被创建**。
2. `dsi_get_phy()`（`drivers/gpu/drm/msm/dsi/dsi.c:25-60`）里
   `of_find_device_by_node(phy_node)` 返回 NULL →
   `DRM_DEV_ERROR("%s: phy driver is not ready\n")` → 返回 `-EPROBE_DEFER`。
3. `dsi_dev_probe()` 只对 `-ENODEV` 特殊处理放行（`dsi.c:170-176`），
   `-EPROBE_DEFER` 会原样返回 → **`msm_dsi` 永远 probe 不上**。
4. `mdp5` 的 component master 等不到 DSI → **整个 DRM 设备不 probe** →
   **屏幕永远不亮**，且因为没开 `simpledrm` 做兜底，就是黑屏。

**这个 bug 特别隐蔽**：CI 的「DTB 自检」只 grep `qcom,msm8953-dsi-ctrl`
和面板 compatible，`status="disabled"` 的节点**照样在 dtb 里**，所以自检
100% 通过。等于 CI 完全没覆盖到这一点。

**建议修法**：在 `msm8953-oppo-common.dtsi` 的 `&mdss_dsi0` 块后面补：

```dts
&mdss_dsi0_phy {
	status = "okay";

	/* 4.19 的 mdss_dsi_phy 用的是 PM8953 L6（1.8V VDDIO） */
	vddio-supply = <&pm8953_l6>;
};
```

同时建议**加强 CI 的 DTB 自检**，除了 grep compatible，还要断言
`status` 是 okay —— 例如反编译后用 python 检查
`dsi@1a94000`、`phy@1a94400`、`panel@0` 的 `status` 都不是 `"disabled"`。

---

## B-2【致命】`msm8953-oppo-r9s.dts:44-48` / `r9sk.dts:40-44` — 面板 graph 端点不成对

**位置**：
- `arch/arm64/boot/dts/qcom/msm8953-oppo-r9s.dts:44-48`
- `arch/arm64/boot/dts/qcom/msm8953-oppo-r9sk.dts:40-44`

```dts
44: port {
45: 	panel_in: endpoint {
46: 		remote-endpoint = <&mdss_dsi0_out>;
47: 	};
48: };
```

**问题**：`mdss_dsi0_out` 在 `msm8953.dtsi:955-957` 里是**空 endpoint**：

```dts
port@1 {
	reg = <1>;
	mdss_dsi0_out: endpoint {
	};                              ← 没有 remote-endpoint
};
```

dtc 直接报了这个：

```
Warning (graph_endpoint):
  /soc@0/display-subsystem@1a00000/dsi@1a94000/panel@0/port/endpoint:
  graph connection to node '.../dsi@1a94000/ports/port@1/endpoint'
  is not bidirectional
```

我把 dtb 反编译出来确认了，`port@1/endpoint` 里**只有 phandle 没有
remote-endpoint**：
```
port@1 {
	reg = <0x01>;
	endpoint {
		phandle = <0x25>;       ← 没有 remote-endpoint = <&panel_in>
	};
};
```

**后果**：`devm_drm_of_get_bridge()`（`drivers/gpu/drm/bridge/panel.c:500`）→
`drm_of_find_panel_or_bridge()`（`drivers/gpu/drm/drm_of.c:237`）→
`of_graph_get_remote_node(np, 1, 0)`。这个函数内部走
`of_graph_get_remote_endpoint()` 读 `remote-endpoint` 属性，读不到就
`return NULL` → `-ENODEV`。

然后 `dsi_bind()`（`dsi.c:118-140`）里：
```c
ext_bridge = devm_drm_of_get_bridge(&msm_dsi->pdev->dev, ..., 1, 0);
if (IS_ERR(ext_bridge))
	return PTR_ERR(ext_bridge);      ← -ENODEV → dsi_bind 直接失败
```
→ component bind 失败 → **DSI 起不来 → 屏幕不亮**。

上游所有用 panel 的板子都写了双向链接。对照 `msm8916-samsung-a3u-eur.dts:107-110`：
```dts
&mdss_dsi0_out {
	data-lanes = <0 1>;
	remote-endpoint = <&panel_in>;      ← 必须有这一句
};
```

**建议修法**：在两个机型 dts 里各补一段（**注意：`&mdss_dsi0_out` 在
`msm8953-oppo-common.dtsi` 里补一次即可，但 `remote-endpoint` 依赖
各机型的 `panel_in` label，所以要放在机型 dts 里**）：

```dts
/* msm8953-oppo-r9s.dts 与 msm8953-oppo-r9sk.dts 各加一份 */
&mdss_dsi0_out {
	remote-endpoint = <&panel_in>;
	data-lanes = <0 1 2 3>;
};
```

（`data-lanes` 按 4.19 的 `qcom,mdss-dsi-panel-width`/lane 配置填；
R63452 是 4 lane，EA8064 也是 4 lane。）

---

## B-3【致命】`msm8953-oppo-common.dtsi:512-528` — AK4376 挂在根节点下，不在 i2c_8 上

**位置**：`arch/arm64/boot/dts/qcom/msm8953-oppo-common.dtsi:512-528`

```dts
512: / {
513: 	/*
514: 	 * 耳机 DAC：AK4376 @ i2c_8 0x10
...
520: 	ak4376: audio-codec@10 {
521: 		compatible = "akm,ak4376";
522: 		reg = <0x10>;
523:
524: 		ak4376,pdn-gpio = <&tlmm 28 GPIO_ACTIVE_LOW>;
525: 		enable-gpios = <&tlmm 62 GPIO_ACTIVE_HIGH>;
526:
527: 		#sound-dai-cells = <0>;
528: 	};
```

**问题**：注释说"AK4376 @ i2c_8"，但节点实际是 **`/` 根节点的直接子节点**，
根本不在 `&i2c_8` 下面。`&i2c_8` 在 594-596 行被单独 `status = "okay"`，
但**下面没有任何子节点**。

我反编译 dtb 确认了：
```
i2c@7af8000 {
	compatible = "qcom,i2c-qup-v2.2.1";
	...
	status = "okay";
	/* 没有任何子节点 */
};

...

audio-codec@10 {                       ← 在根节点下
	compatible = "akm,ak4376";
	reg = <0x10>;
	...
};
```

**后果**：
1. `ak4376_i2c_probe()` **永远不会被调用** —— i2c 驱动只对 i2c 总线上的
   子节点 probe，根节点下的 `audio-codec@10` 没有 i2c client。
2. 更糟的是，dtc 还报了个额外警告：
   ```
   Warning (reg_format): /audio-codec@10:reg: property has invalid length
     (4 bytes) (#address-cells == 2, #size-cells == 2)
   ```
   因为根节点的 `#address-cells = <2>` / `#size-cells = <2>`（arm64 惯例），
   而 `reg = <0x10>` 只有 1 个 cell，**长度都不匹配**。
3. 声卡的 `primary-mi2s-rx-dai-link` 里 `codec { sound-dai = <&ak4376>; };`
   会因为找不到 component → `snd_soc_register_card()` 返回 `-EPROBE_DEFER`
   → **整个声卡注册不上**，连外放都没了。

对照 4.19（`oppo-msm8953/msm8953-mtp.dtsi:178`）：
```dts
&i2c_8 { /* BLSP2 QUP4 */
	ak4376@10 {
		compatible = "akm,ak4376";
		reg = <0x10>;
```
确认是在 `&i2c_8` 下。

**建议修法**：把整个 `ak4376` 节点搬进 `&i2c_8` 块里：

```dts
&i2c_8 {
	status = "okay";

	ak4376: audio-codec@10 {
		compatible = "akm,ak4376";
		reg = <0x10>;

		ak4376,pdn-gpio = <&tlmm 28 GPIO_ACTIVE_LOW>;
		enable-gpios = <&tlmm 62 GPIO_ACTIVE_HIGH>;

		#sound-dai-cells = <0>;
	};
};
```

注意还要确认 `&i2c_8` 的 pinctrl（`i2c_8_default` / `i2c_8_sleep`）
在 `msm8953.dtsi` 里是默认就带的（是的，`pinctrl-0 = <&i2c_8_default>` 已在
dtsi 里），所以不用额外加。

---

## B-4【低】`msm8953-oppo-common.dtsi:89-98` — `cont_splash_mem` 尺寸从 20 MiB 缩到 5.93 MiB

**位置**：`arch/arm64/boot/dts/qcom/msm8953-oppo-common.dtsi:89-98`

```dts
89: /* bootloader 交接的 framebuffer，标记为不可回收 */
90: cont_splash_mem: cont-splash@90001000 {
91: 	reg = <0x0 0x90001000 0x0 (1080 * 1920 * 3)>;
92: 	no-map;
93: };
```

**问题**：`msm8953.dtsi:310` 原本是 `0x13ff000`（≈ 20 MiB），
这里 `/delete-node/` 后重新声明成 `1080*1920*3 = 0x5EFC00`（≈ 5.93 MiB）。

如果 **bootloader 实际保留的 splash 区域是 20 MiB**，那我们只声明
5.93 MiB，剩下约 14 MiB 会被 kernel 当可用内存分配出去。一旦有东西
写进去，再回到 bootloader 显示就是花屏。

**这不是硬 bug（取决于 bootloader 真实布局），所以降到「低」。**
但如果按 51 行的注释"刻意不再声明 framebuffer 节点"，那这块内存
**根本没人用**，5.93 MiB 白占。

**建议修法**（二选一）：
- 既然不用 simpledrm，**整个 `cont_splash_mem` 节点删掉**
  （连同 90-93 行），省 5.93 MiB；
- 或者要保留给 bootloader 回显，就按 4.19 的 `0x13ff000` 写足。

---

## B-4b【无问题】reserved-memory 地址区间无重叠

**结论：用 dtc 反编译后手算全部 15 个区域，互不重叠，未发现问题。**

```
0x81800000 - 0x81802000  (  0.01 MiB)  zap@81800000
0x84300000 - 0x86300000  ( 32.00 MiB)  qseecom@84300000
0x86300000 - 0x86400000  (  1.00 MiB)  smem@86300000
0x86400000 - 0x86800000  (  4.00 MiB)  reserved@86400000
0x86c00000 - 0x8d600000  (106.00 MiB)  mpss@86c00000
0x8d600000 - 0x8e700000  ( 17.00 MiB)  adsp@8d600000
0x8e700000 - 0x8ee00000  (  7.00 MiB)  wcnss@8e700000
0x90000000 - 0x90001000  (  0.00 MiB)  dfps-data@90000000
0x90001000 - 0x905efc00  (  5.93 MiB)  cont-splash@90001000
0x91400000 - 0x91b00000  (  7.00 MiB)  venus@91400000
0x92000000 - 0x92100000  (  1.00 MiB)  mba@92000000
0xaefd2000 - 0xaf000000  (  0.18 MiB)  reserved@aefd2000
0xb0000000 - 0xb0800000  (  8.00 MiB)  ramoops@b0000000
0xeefe4000 - 0xef000000  (  0.11 MiB)  reserved@eefe4000
0xf2d00000 - 0xf2e80000  (  1.50 MiB)  rmtfs@f2d00000
```

顺带核对 `ramoops@b0000000` 内部尺寸：

```
console 0x400000 (4 MiB)
pmsg    0x100000 (1 MiB)
record  0x300000 (3 MiB)
-----------------------
合计    0x800000 (8 MiB)   == reg 的 size，完全吻合 ✅
```

`ftrace-size = <0>` / `ecc-size = <0>` 都是合法值 ✅。

---

## B-5【中】`msm8953-oppo-common.dtsi:140-158` — 两个触摸 regulator 没有上电时序，`rmi4` 是同时 enable

**位置**：`arch/arm64/boot/dts/qcom/msm8953-oppo-common.dtsi:140-158`

```dts
140: touch_1v8: regulator-touch-1v8 {
141: 	compatible = "regulator-fixed";
142: 	regulator-name = "touch_1v8";
143: 	regulator-min-microvolt = <1800000>;
144: 	regulator-max-microvolt = <1800000>;
145: 	gpio = <&tlmm 134 GPIO_ACTIVE_HIGH>;
146: 	enable-active-high;
147: 	startup-delay-us = <100>;
148: };
```

先说**不是问题的部分**（我核对过）：

- `gpio = <...>` 这个**单数属性名是合法的**。
  `drivers/regulator/fixed.c:321` 是
  `gpiod_get_optional(&pdev->dev, NULL, gflags)`，`con_id == NULL` 时
  gpiolib 会依次尝试 `"gpios"` 和 `"gpio"`。binding
  `fixed-regulator.yaml:59` 也明确列了 `gpio:` ✅。
- `enable-active-high` + `GPIO_ACTIVE_HIGH` 一起写，**不冲突** ——
  `fixed.c` 用 `enable-active-high` 决定 `GPIOD_OUT_HIGH/LOW`，
  gpiolib 再按 DT 旗标做极性转换，两者都表示"高有效"。**极性是对的**
  （4.19 的 `synaptics,enable1v8-gpio = <&tlmm 134 0x1>` 也是高有效）。
  只是写法冗余，建议删掉 `enable-active-high;` 其一以符合 upstream 风格。

**真正的问题（上电时序）**：`rmi4` 用的是
`drivers/input/rmi4/rmi_i2c.c:229-241` 的
`devm_regulator_bulk_get()` + `regulator_bulk_enable()`，
**两个 supply 是同时 enable 的**，没有任何先后。

而 4.19 的 vendor 驱动（`synaptics-s3320`）里，
`enable1v8` 和 `enable2v8` 是**分开两步、中间有延时**的。
`startup-delay-us = <100>` 表达的是"enable 后等 100µs 认为稳定"，
**不能表达"B 在 A 之后 N ms"**，`regulator-fixed` 也没有这个能力。

**后果**：触摸 IC（S3320）上电时序不对 → probe 时
`rmi_set_page(rmi_i2c, 0)` 读不到 → 触摸不工作。

**建议修法**（按风险从低到高）：
1. 先给两个 regulator 都加 `regulator-always-on`，让 bootloader 起的电
   不被关闭 —— 至少不打断已工作的状态。
2. 更稳的做法：**只保留 `touch_2v8`（模拟 2.8V）一个 regulator**，
   `vdd` / `vio` 都指向同一个 regulator，这样 `bulk_enable` 不会打乱时序，
   1.8V IO 由 PM8953 L6（`pm8953_l6`，已 `regulator-always-on`）常供。
3. 真机确认后如果必须分时，就得在面板/触摸驱动里自己用
   `devm_gpiod_get()` 控制两个脚（放弃 `regulator-fixed` 思路）。

---

# C. 会导致功能异常

## C-1【高】`msm8953-oppo_defconfig:1788` 与 `diag_defconfig:27` — `PSTORE_DEFLATE_COMPRESS` 符号不存在

**位置**：
- `arch/arm64/configs/msm8953-oppo_defconfig:1788`
- `arch/arm64/configs/msm8953-oppo-diag_defconfig:27`

```
CONFIG_PSTORE_DEFLATE_COMPRESS=y
```

**问题**：6.12 的 `fs/pstore/Kconfig` 里**没有** `PSTORE_DEFLATE_COMPRESS`。
只有 `PSTORE_COMPRESS`（`fs/pstore/Kconfig:24`）：

```
24: config PSTORE_COMPRESS
25: 	bool "Pstore compression (deflate)"
26: 	depends on PSTORE
27: 	select ZLIB_INFLATE
28: 	select ZLIB_DEFLATE
29: 	default y
```

`PSTORE_DEFLATE_COMPRESS` 是**老 4.x 的名字**（那时是 `PSTORE_ZLIB_COMPRESS` /
`PSTORE_LZO_COMPRESS` / `PSTORE_LZ4_COMPRESS` 三选一）。

**后果**：kconfig 读到不认识的 `CONFIG_PSTORE_DEFLATE_COMPRESS=y` 时，
会 `sym_lookup(..., 0)` 造一个 `S_UNKNOWN` 符号并设成 `S_BOOLEAN`
（`scripts/kconfig/confdata.c`），**静默丢弃**，只给个
`conf_warning("unexpected data: ...")`。最终 `.config` 里这一行消失。

**但用户的意图（压缩 pstore）其实已经满足了** —— `PSTORE_COMPRESS`
`default y`，只要 `PSTORE=y` 就自动开。所以**功能上没坏，但配置是"假"的**：
写这一行的人以为自己在控制压缩，其实没控制。

**建议修法**：把两处都改成
```
CONFIG_PSTORE_COMPRESS=y
```
（显式写出来，比依赖 default 更清楚）。

---

## C-2【高】`msm8953-oppo_defconfig:1712` — `USB_CONFIGFS_UEVENT` 符号不存在

**位置**：`arch/arm64/configs/msm8953-oppo_defconfig:1712`

```
CONFIG_USB_CONFIGFS_UEVENT=y
```

**问题**：6.12 里没有 `USB_CONFIGFS_UEVENT`，只有
`ANDROID_USB_CONFIGFS_UEVENT`（`drivers/usb/gadget/Kconfig:244`）：

```
244: config ANDROID_USB_CONFIGFS_UEVENT
245: 	bool
246: 	depends on USB_CONFIGFS
```

**后果**：静默丢弃。Android 的 `ueventd` 靠这个属性感知 USB 配置变化，
**不设的话 Android 用户态拿不到 USB 状态事件**（adb 可能时好时坏）。

**建议修法**：
```
CONFIG_ANDROID_USB_CONFIGFS_UEVENT=y
```

---

## C-3【高】`msm8953-oppo_defconfig:1757` — `CONFIG_MSM_RPM_SMD=y` 不存在（见 A-2）

**位置**：`arch/arm64/configs/msm8953-oppo_defconfig:1757`

```
CONFIG_MSM_RPM_SMD=y
```

同 A-2，这个符号在 6.12 树里**没有任何定义**，会被 kconfig 静默丢弃。
本意是"打开 bus scaling 的 RPM 后端"，实际上什么都开不了，
`msm_bus_rpm_smd.c` 永远编不进去。

**注意**：`msm8953-oppo-diag_defconfig` 里**没有**这一行（我核对过，
诊断配置只有 91 行、含 `QCOM_SMD_RPM` 而没有 `MSM_RPM_SMD`）。
所以这条只影响正常配置。

**建议修法**：删掉；同时按 A-2 的方案把
`drivers/soc/qcom/msm_bus/Makefile:7` 和
`msm_bus_core.h:295` 的 `CONFIG_MSM_RPM_SMD` 改成 `CONFIG_QCOM_SMD_RPM`。

---

## C-4【高】`drivers/soc/oppo/device_info.c:658-695` — `devinfo_probe()` 尾部是不可达死代码

**位置**：`drivers/soc/oppo/device_info.c:658-695`

```c
658: static int devinfo_probe(struct platform_device *pdev)
...
680: 	pentry = proc_create("mainboard", S_IRUGO, NULL, &mainboard_res_proc_fops);
681: 	if (!pentry)
682: 		pr_err("create prjVersion proc failed.\n");
683:
684: 	return ret;              ← 这里就返回了
685:
686: 	if (!parent) {           ← 死代码，永远不执行
687: 		parent = proc_mkdir ("devinfo", NULL);
688: 		if (!parent) {
689: 			pr_err("can't create devinfo proc\n");
690: 			ret = -ENOENT;
691: 		}
692: 	}
693:
694: 	pa_verify();             ← 死代码
695: }
```

**问题**：`return ret;` 之后还有 12 行代码。编译时会
`-Wunreachable-code`（如果开了）或至少是明确的 bug。

**后果**：
1. `/proc/devinfo/` 目录**永远不会被创建**（`proc_mkdir` 在死代码里）。
   但 `register_device_proc()`（113 行）自己会 lazily 建
   `parent = proc_mkdir("devinfo", NULL)`，所以 `/proc/devinfo/xxx`
   其实还是能出来 —— **只是 `devinfo_probe` 里那段建目录的代码白写了**。
2. `pa_verify()` **永远不会被调用** → PA（功放）的 device info
   （`/proc/devinfo/pa`）缺失。

**建议修法**：把 `return ret;` 移到函数末尾：

```c
	pentry = proc_create("mainboard", S_IRUGO, NULL, &mainboard_res_proc_fops);
	if (!pentry)
		pr_err("create mainboard proc failed.\n");   /* 顺手修 C-6 */

	if (!parent) {
		parent = proc_mkdir("devinfo", NULL);
		if (!parent) {
			pr_err("can't create devinfo proc\n");
			ret = -ENOENT;
		}
	}

	pa_verify();

	return ret;
}
```

注意：这个 bug 是**从 4.19 继承下来的**（4.19 的 `devinfo_probe` 一模一样），
不是 6.12 移植引入的。但既然在审，就该修。

---

## C-5【中】`drivers/soc/oppo/device_info.c:682` — 错误信息文案错（`prjVersion` vs `mainboard`）

**位置**：`drivers/soc/oppo/device_info.c:680-682`

```c
pentry = proc_create("mainboard", S_IRUGO, NULL, &mainboard_res_proc_fops);
if (!pentry)
	pr_err("create prjVersion proc failed.\n");
```

**问题**：这里建的是 `mainboard`，报错却说 `prjVersion`（那是
`oppo_project.c` 里的另一个 proc 节点）。典型的复制粘贴残留。

**建议修法**：改成 `pr_err("create mainboard proc failed.\n");`

---

## C-6【中】`drivers/power/supply/oppo/oppo_bq27541.c:37-41` — 用 stub 顶替真实功能，`oppo_vooc_get_allow_reading()` 恒返回 true

**位置**：`drivers/power/supply/oppo/oppo_bq27541.c:37-41`

```c
37: // FIXME: Hacks for compilation
38: bool oppo_vooc_get_allow_reading(void)
39: {
40: 	return true;
41: }
```

**问题**：`oppo_vooc_get_allow_reading()` 在 bq27541 里被调用 **8 次**
（143/172/201/323/350/383/411/448 行），全部是这种模式：

```c
if (oppo_vooc_get_allow_reading() == true) {
	/* 真正去读 I2C */
}
```

4.19 里这个函数由 `vooc_charger.c` 提供，**只有在 VOOC 芯片允许时才
返回 true**（VOOC 通信期间 I2C 总线被占用，必须停读）。这里恒返回 `true`
→ **VOOC 快充握手期间照样去读 bq27541 → I2C 冲突 → 电量跳变/读失败**。

而且注释自己写了 `// FIXME: Hacks for compilation`，说明作者知道这是 hack。

**建议修法**：
```c
/* include/linux/power/oppo_vooc.h 里已有 fw 相关声明，
 * 应该把 allow_reading 做成 vooc 驱动提供的真实实现 */
bool oppo_vooc_get_allow_reading(void);
```
在 `vooc_charger.c` 里实现（比如 `vooc->fast_charge_enabled ? false : true`，
或者跟踪 VOOC 通信窗口），bq27541 只 `#include` 并调用。
如果暂时要保留 stub，至少改成 `return !vooc_in_communication;`。

---

## C-7【中】`drivers/power/supply/oppo/oppo_bq27541.c:1183-1193` — `remove()` 没释放 `bq27541_di` / 没清 `new_client`

**位置**：`drivers/power/supply/oppo/oppo_bq27541.c:1183-1193`

```c
1183: static void bq27541_driver_remove(struct i2c_client *client)
1184: {
1185: 	fg_oppo_unregister();
1186:
1187: 	if (bq27541_di) {
1188: 		bq27541_cntl_cmd(bq27541_di->cmd_addr.subcmd_disable_dlog);
1189: 		udelay(66);
1190: 		bq27541_cntl_cmd(bq27541_di->cmd_addr.subcmd_disable_it);
1191: 		cancel_delayed_work_sync(&bq27541_di->hw_config);
1192: 	}
1193: 	/* void 函数无返回值（6.11+） */
1194: }
```

**问题**：
1. `bq27541_di` 是 `probe()` 里 `kzalloc()` 的（1206 行），`remove()` 里
   **没有 `kfree()`** → 每次 unbind/rebind 泄漏一个 `struct bms_bq27541`。
2. `new_client` 是全局 `static`（44 行），`remove()` 里**没有置 NULL**
   → remove 之后 `bq27541_read_i2c()` 等仍会拿野指针去
   `i2c_smbus_read_word_data()` → **use-after-free**。
3. `bq27541_di` 也没有置 NULL，同理。
4. `i2c_set_clientdata(client, di)` 在 probe 里设了，remove 里没清。

**建议修法**：
```c
static void bq27541_driver_remove(struct i2c_client *client)
{
	struct bms_bq27541 *di = i2c_get_clientdata(client);

	fg_oppo_unregister();

	if (di) {
		bq27541_cntl_cmd(di->cmd_addr.subcmd_disable_dlog);
		udelay(66);
		bq27541_cntl_cmd(di->cmd_addr.subcmd_disable_it);
		cancel_delayed_work_sync(&di->hw_config);
	}
	/* 清全局，避免 remove 后仍被访问 */
	new_client = NULL;
	bq27541_di = NULL;
	kfree(di);
}
```

注意：`hw_config` 是 `schedule_delayed_work(&di->hw_config, 0)` 调起来的，
里面第一件事就是 `fg_oppo_register(&bq27541_batt_gauge)`（674 行），
如果 work 还没跑完就 remove，`cancel_delayed_work_sync()` 是对的；
但 `bq27541_hw_config()` 里还有 `if (!bq27541_di) return;` 的空判断
**不存在**，应该在 work 里也加一层 `if (!bq27541_di) return;`。

---

## C-8【中】`drivers/power/supply/oppo/vooc_charger.c:196-201` — `vooc_probe()` 的 `exit:` 标签会掩盖错误，且没有错误路径清理

**位置**：`drivers/power/supply/oppo/vooc_charger.c:196-241`

```c
196: static int vooc_probe(struct i2c_client *client)
197: {
198: 	int rc = 0;
199:
200: 	struct oppo_vooc_device *vooc = devm_kzalloc(&client->dev, sizeof(*vooc), GFP_KERNEL);
201: 	if (!vooc) { rc = -ENOMEM; goto exit; }
...
241: exit:
242: 	return rc;
243: }
```

**问题**：
1. `goto exit` 之后只是 `return rc`，**什么清理也没做**。虽然 `vooc` 是
   `devm_kzalloc` 的（自动释放），但 `vooc_poll_wq` 是
   `alloc_workqueue()`（218 行）**不是 devm**，如果 probe 在
   `alloc_workqueue` 之后失败（实际上那之后没有失败点了，所以暂时无害），
   或者将来有人加代码，就会泄漏 workqueue。
2. 更实际的问题：`queue_delayed_work()`（232 行）在 probe 末尾调用，
   如果 `devm_gpiod_get` 某个失败，probe 返回错误，但
   **`vooc->lock` 已经 `mutex_init` 了、`INIT_DELAYED_WORK` 也做了**
   —— 这些是幂等的，问题不大。
3. **真正的 bug**：`vooc->dev = &client->dev`（205 行）之后，
   `vooc_poll_func()` 里用 `dev = vooc->dev` 打日志。如果 probe 失败，
   `vooc` 被 devm 释放，但**已经 queue 的 delayed work 不会被取消**
   （因为 `destroy_workqueue` 只在 `remove()` 里调）。
   → work 里 `vooc->dev` 是已释放内存 → **UAF**。

   不过这条要 probe 在 `queue_delayed_work` **之后**失败才成立，
   而实际上 `queue_delayed_work` 是最后一步、之后直接 `return rc`（rc=0），
   所以**当前代码路径不会触发**。属于"将来改代码就会踩"的隐患。

**建议修法**（防御性）：
```c
exit:
	if (rc) {
		if (vooc && vooc->vooc_poll_wq)
			destroy_workqueue(vooc->vooc_poll_wq);
	}
	return rc;
```

---

## C-9【中】`msm8953-oppo-common.dtsi:59-76` — `gpio-keys` 只给 gpio85 配了 pinctrl，gpio86 没配

**位置**：`arch/arm64/boot/dts/qcom/msm8953-oppo-common.dtsi:59-76`

```dts
59: gpio-keys {
60: 	compatible = "gpio-keys";
61:
62: 	pinctrl-names = "default";
63: 	pinctrl-0 = <&gpio_key_default>;
64:
65: 	key-volume-up {
66: 		gpios = <&tlmm 85 GPIO_ACTIVE_LOW>;
...
71: 	key-volume-down {
72: 		gpios = <&tlmm 86 GPIO_ACTIVE_LOW>;
```

**问题**：`gpio_key_default`（`msm8953.dtsi:609-614`）**只包含
`pins = "gpio85"`**，不含 gpio86。所以 `key-volume-down`（gpio86）
**没有配成 GPIO 功能 + 上拉**，会被 pinctrl 默认状态（可能是别的
function 或 no-pull）影响 → **音量下键可能不工作或误触发**。

对照 4.19 的 `msm8953-pinctrl.dtsi:399-416`：
```
gpio_key_active {
	pins = "gpio85", "gpio86";     ← 两个都有
```

**建议修法**：在 `&tlmm` 里新增一个节点，或者在 dts 里 override
`gpio_key_default`：

```dts
&gpio_key_default {
	pins = "gpio85", "gpio86";
};
```

（dtc 允许 override 已有节点的属性。注意这会在 `&tlmm` 里追加，
不会影响 potter/mido 等其它板子，因为它们在各自的 dts 里也 override 了。）

---

## C-10【中】`msm8953-oppo-common.dtsi:201-216` — rmi4 触摸屏用了 `syna,reset-delay-ms` 但没接 `reset-gpios`

**位置**：`arch/arm64/boot/dts/qcom/msm8953-oppo-common.dtsi:201-216`

```dts
201: touchscreen@20 {
202: 	reg = <0x20>;
203: 	compatible = "syna,rmi4-i2c";
204:
205: 	interrupt-parent = <&tlmm>;
206: 	interrupts = <17 IRQ_TYPE_EDGE_FALLING>;
207:
208: 	pinctrl-names = "default";
209: 	pinctrl-0 = <&ts_reset &ts_int>;
210:
211: 	vdd-supply = <&touch_2v8>;
212: 	vio-supply = <&touch_1v8>;
213:
214: 	syna,reset-delay-ms = <200>;
215: 	syna,startup-delay-ms = <500>;
216: };
```

**问题**：
1. **`reset-gpios` 没写**。`rmi4` 驱动在 6.12 里**根本不处理 `reset-gpios`**
   （我 grep 了 `drivers/input/rmi4/`，只有 `syna,reset-delay-ms` 在
   `rmi_driver.c:997` 被读，没有任何 reset gpio 的代码）。所以
   `syna,reset-delay-ms = <200>` 是**给谁看的？** —— 没有消费者，
   纯装饰。
2. `pinctrl-0 = <&ts_reset &ts_int>` 只是把 gpio16/17 配成 GPIO 功能，
   **没人去拉 gpio16**（因为没 `reset-gpios`）。所以 S3320
   的 reset 脚**一直没被复位过**，靠 bootloader 留下的状态。
   真机上大概率 probe 失败（`rmi_set_page(rmi_i2c, 0)` 读不到）。
3. `interrupts = <17 IRQ_TYPE_EDGE_FALLING>` 是对的（4.19 是
   `17 0x2008` = falling | pull-up），✓。
4. `vdd-supply` / `vio-supply` 都是 `regulator-fixed`，
   `devm_regulator_bulk_get()` 要求**两个都存在**，✓（都在 dts 里定义了）。

**建议修法**：
- 如果坚持用 mainline `rmi4`，**必须自己在 probe 前拉 reset**。
  mainline 没有这个能力，所以要么：
  (a) 在 dts 里加 `reset-gpios = <&tlmm 16 GPIO_ACTIVE_LOW>;` 并且
      **给 rmi4 打一个 patch**（或者写一个 `gpio-reset` 的
      `devm_gpiod_get()` 用 `devm_add_action_or_reset()` 拉高）；
  (b) 接受"不复位"这个现实，在注释里写清楚，并删掉误导性的
      `syna,reset-delay-ms`。
- 这条路（S3320 走 mainline rmi4）本身是**高风险未知**，
  dtsi 注释里也写了"需要真机确认"。建议在 README 里标成
  "触摸可能不工作"而不是"已移植"。

---

## C-11【低】`msm8953-oppo-common.dtsi:480-487` — LM3697 的 `led-sources = <2>` 索引基准可能是 1-based（vendor）vs 0-based（mainline）

**位置**：`arch/arm64/boot/dts/qcom/msm8953-oppo-common.dtsi:468-489`

```dts
480: backlight_led: led@0 {
481: 	reg = <0>;
482: 	led-sources = <2>;
483: 	max-current-milliamp = <18>;
484: 	label = "backlight";
485: 	function = LED_FUNCTION_BACKLIGHT;
486: 	default-trigger = "default-on";
487: };
```

先说**正确的部分**（dtsi 455-458 行的注释已经解释得很清楚，我核对过
`drivers/leds/leds-lm3697.c:226-233`，确认无误）：

- `reg = <0>` 对应 **CONTROL_A**（`leds-lm3697.c` 只接受 0/1）✅
- `led-sources` 表示用哪几路 HVLED 灯串 ✅
- 两者语义不同、不能混写 ✅

**待确认的点**：`led-sources = <2>` 在 mainline 里是
**0-based 的 HVLED 索引**（0/1/2 = 第 1/2/3 串），而 4.19 的 vendor
驱动里 `led-sources = <2>` 的语义是"第 3 路灯串"（也是 1-based 数到 3）。
两者**恰好数值相同但基准不同**：
- mainline 解读：`<2>` = 第 3 串（index 2）—— 与 vendor 意图一致 ✅
- 但如果 vendor 是 1-based，`<2>` 应该是"第 2 串"，而 mainline 会当成第 3 串

**需要真机看背光是否正常亮**才能定论，所以降为「低」。
`max-current-milliamp = <18>` / `default-trigger = "default-on"` 都是
mainline binding 支持的属性 ✅（`LEDS_TRIGGER_DEFAULT_ON` 在
defconfig:1699 已开）。

**建议修法**：真机确认亮度正常就保持现状；如果要保守，改成
`led-sources = <0 1 2>;`（三串全开，不依赖索引语义）。

---

# D. 代码质量问题

## D-1【高】`mm/vmstat.c:1256-1259` — QCOM 私有 node stats 插在 `nr_kernel_misc_reclaimable` 之后，但枚举里它们在**末尾**

**位置**：
- `include/linux/mmzone.h:253-256`（枚举定义）
- `mm/vmstat.c:1256-1259`（`vmstat_text[]` 文本）

**问题**：`vmstat_text[]` 的**每个元素必须与 `enum node_stat_item` 的
枚举值一一对应**（`vmstat_start()` 用 `v[i] = global_node_page_state_pages(i)`
按索引填，`vmstat_show()` 用 `vmstat_text[off]` 按索引打印）。顺序错了
就**整个 `/proc/vmstat` 的所有字段名错位**。

我把两个文件解析出来对比了。**枚举里的顺序**是：

```
...
38 NR_KERNEL_MISC_RECLAIMABLE
39 NR_FOLL_PIN_ACQUIRED
40 NR_FOLL_PIN_RELEASED
41 NR_KERNEL_STACK_KB
42 NR_KERNEL_SCS_KB          (CONFIG_SHADOW_CALL_STACK)
43 NR_PAGETABLE
44 NR_SECONDARY_PAGETABLE
45 NR_IOMMU_PAGES            (CONFIG_IOMMU_SUPPORT)
46 NR_SWAPCACHE              (CONFIG_SWAP)
47 PGPROMOTE_SUCCESS         (CONFIG_NUMA_BALANCING)
48 PGPROMOTE_CANDIDATE
49 PGDEMOTE_KSWAPD
50 PGDEMOTE_DIRECT
51 PGDEMOTE_KHUGEPAGED
52 NR_UNRECLAIMABLE_PAGES    ← QCOM 私有，在【末尾】
53 NR_ION_HEAP
54 NR_ION_HEAP_POOL
55 NR_GPU_HEAP
56 NR_VM_NODE_STAT_ITEMS
```

而 `vmstat_text[]` 的实际顺序是：

```
"nr_kernel_misc_reclaimable",   ← idx 38 ✓
"nr_unreclaimable_pages",       ← idx 39  ✗ 应该是 nr_foll_pin_acquired
"nr_ion_heap",                  ← idx 40  ✗ 应该是 nr_foll_pin_released
"nr_ion_heap_pool",             ← idx 41  ✗ 应该是 nr_kernel_stack
"nr_gpu_heap",                  ← idx 42  ✗ 应该是 nr_shadow_call_stack
"nr_foll_pin_acquired",         ← idx 43  ✗ 应该是 nr_page_table_pages
"nr_foll_pin_released",         ← idx 44  ✗
"nr_kernel_stack",              ← idx 45  ✗
"nr_shadow_call_stack",         ← idx 46  ✗
"nr_page_table_pages",          ← idx 47  ✗
"nr_sec_page_table_pages",      ← idx 48  ✗
"nr_iommu_pages",               ← idx 49  ✗
"nr_swapcached",                ← idx 50  ✗
"pgpromote_success",            ← idx 51  ✗ 应该是 pgdemote_kswapd
...
```

**从 idx 39 开始全部错位 4 位。**

注释里写的是"追加在末尾，保持既有统计项编号不变"，**枚举确实追加在末尾了**，
但 `vmstat_text[]` 没跟着挪 —— 它是插在 `nr_kernel_misc_reclaimable`
后面的（1256-1260 行），而不是末尾。

**后果**：`/proc/vmstat` 的字段名从 `NR_KERNEL_MISC_RECLAIMABLE`（idx 38）
**之后整体错位 4 项**：

| 枚举 idx | 枚举项 | `vmstat_text[idx]` 实际内容 | 应该是 |
|---|---|---|---|
| 38 | `NR_KERNEL_MISC_RECLAIMABLE` | `nr_kernel_misc_reclaimable` | ✅ |
| 39 | `NR_FOLL_PIN_ACQUIRED` | `nr_unreclaimable_pages` | ❌ |
| 40 | `NR_FOLL_PIN_RELEASED` | `nr_ion_heap` | ❌ |
| 41 | `NR_KERNEL_STACK_KB` | `nr_ion_heap_pool` | ❌ |
| 42 | `NR_KERNEL_SCS_KB` | `nr_gpu_heap` | ❌ |
| 43 | `NR_PAGETABLE` | `nr_foll_pin_acquired` | ❌ |
| 44 | `NR_SECONDARY_PAGETABLE` | `nr_foll_pin_released` | ❌ |
| 52 | `NR_UNRECLAIMABLE_PAGES` | `nr_dirty_threshold` 区域 | ❌ |
| … | … | … | ❌ |

具体可见的症状：
- `/proc/vmstat` 里 `nr_kernel_stack` 那几行显示的是 `nr_gpu_heap` /
  `nr_foll_pin_*` 的**数值**，名字张冠李戴；
- `nr_page_table_pages` 显示的是 `nr_kernel_stack` 的值；
- `pgdemote_*` 之后的所有条目（`nr_dirty_threshold` 等
  `enum vm_stat_item` 区段）也一起错位；
- 真正的 `nr_unreclaimable_pages` / `nr_ion_heap` / `nr_gpu_heap`
  **永远不会被打印**（它们的文本被挪用到了前面）。

**注意：这不会 panic。** 因为 4 个字符串插在中间、末尾少了 4 个，
总数不变，`BUILD_BUG_ON(ARRAY_SIZE(vmstat_text) < NR_VMSTAT_ITEMS)`
照样通过，所有索引也都在数组边界内。**只是所有数字对应错名字。**

危害在于"数据看起来正常但是错的" —— bring-up 阶段拿 `/proc/vmstat`
判断内存泄漏/回收行为时会被彻底误导，比直接 panic 更难查。

**验证方式**：我用 python 分别解析了 `include/linux/mmzone.h` 的
`enum node_stat_item` 和 `mm/vmstat.c` 的 `vmstat_text[]`（去掉
`#ifdef` / 注释，按顺序取字符串），逐一比对索引，确认从 idx 39 起
偏移 4 位。CI 里可以复用同样的脚本做回归检查。

**建议修法**：把 `mm/vmstat.c:1256-1259` 的这 4 行（加注释）**移到**
`nr_dirty_threshold` 之前（也就是 `pgdemote_khugepaged` 之后、
`/* system-wide enum vm_stat_item counters */` 之前）：

```c
	"pgdemote_kswapd",
	"pgdemote_direct",
	"pgdemote_khugepaged",
	/* QCOM 私有 node stats（见 include/linux/mmzone.h 的追加项）*/
	"nr_unreclaimable_pages",
	"nr_ion_heap",
	"nr_ion_heap_pool",
	"nr_gpu_heap",
	/* system-wide enum vm_stat_item counters */
	"nr_dirty_threshold",
```

**同时建议加回归检查**：现有断言只有
`BUILD_BUG_ON(ARRAY_SIZE(vmstat_text) < NR_VMSTAT_ITEMS)`，只管总数、
**查不出顺序错位**（这次就是被它放过去的）。两个可行做法：

- **轻量**：在 `vmstat_text[]` 每个元素上方加 `/* NR_XXX */` 行注释，
  人工 review 时能一眼比对；
- **可靠（推荐）**：在 CI 里加一个 python 脚本，解析
  `include/linux/mmzone.h` 的枚举顺序和 `mm/vmstat.c` 的字符串顺序，
  不一致就 fail。这个项目的 CI 已经有 DTB 自检的 python 脚本了，
  加这一个成本很低。

---

## D-2【高】`include/linux/power_supply.h` — 追加了 148 个私有属性，但 `power_supply_attrs[]` 完全没注册

**位置**：
- `include/linux/power_supply.h:196-350`（148 个 `POWER_SUPPLY_PROP_*`）
- `drivers/power/supply/power_supply_sysfs.c:146-225`（`power_supply_attrs[]`）

**问题**：`power_supply_attrs[]` 只覆盖了枚举的前 **78** 项
（最大 index 77 = `POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN`）。
而枚举有 **224** 项。也就是说 idx 77 之后的 **146 个属性全都没有
sysfs 条目**。

（先澄清一个我自己绕了一圈的点：`include/linux/power_supply.h:120` 是
`POWER_SUPPLY_PROP_VOLTAGE_**MAX**_DESIGN`，`:204` 是
`POWER_SUPPLY_PROP_VOLTAGE_**MIN**_DESIGN`。**名字不同，没有枚举重名，
不存在编译错误。** 我用 python 解析 224 项做过重名检查，`dups` 为空。）

**后果**：

1. 用户在 `/sys/class/power_supply/battery/` 下**看不到**
   `capacity_raw` / `charge_counter_ext` / `battery_type` 等 OPPO/QPNP
   需要的属性。`power_supply_attr_is_visible()`（`power_supply_sysfs.c:361`）
   是按 `__power_supply_attrs[]` 遍历的，而那个数组就是
   `power_supply_attrs[]` 的展开，只有 78 项。

2. **越界读**：`add_prop_uevent()`（`power_supply_sysfs.c:430-433`）：

```c
pwr_attr = &power_supply_attrs[prop];
dev_attr = &pwr_attr->dev_attr;
ret = power_supply_show_property((struct device *)dev, dev_attr, prop_buf);
```

`power_supply_attrs[]` 是用**指定初始化器**定义的
（`power_supply_sysfs.c:31-38` 的 `_POWER_SUPPLY_ATTR`）：

```c
#define _POWER_SUPPLY_ATTR(_name, _text, _len)	\
[POWER_SUPPLY_PROP_ ## _name] = { ... }
```

最大索引是 77（`VOLTAGE_MIN_DESIGN`），所以**数组实际长度只有 78**。

**这不是理论风险，是当前配置下必然触发的**：我确认
`drivers/power/supply/qcom/qpnp-smbcharger.c:5672` 的 `properties[]`
里就有 `POWER_SUPPLY_PROP_REAL_TYPE`，而它的枚举值是 **82**
（idx 77 之后的第 5 个）> 78 → **越界读 5 项**（约 280 字节）。
`qpnp-fg.c` 里也有更多（`CHARGE_NOW_RAW` / `CAPACITY_RAW` 等）。

`add_prop_uevent(dev, 82, prop_buf)` → `power_supply_attrs + 82`
越界 5 项。读到的 5 × 56 字节是 `power_supply_attrs` 之后紧邻的
`.data` / `.bss` 内容 → `prop_name` / `dev_attr` 是野值。

具体后果：
- `power_supply_show_property()` 用野 `dev_attr` 去取 `dev_attr_psp()`
  → 大概率读到垃圾 `prop` 编号 → 继续越界；
- `add_uevent_var(env, "POWER_SUPPLY_%s=%s", pwr_attr->prop_name, ...)`
  用野指针 `prop_name` → **strlen/strcpy 崩在野地址** → `BUG: unable to
  handle kernel paging request`。

**触发条件**：`CONFIG_QPNP_SMBCHARGER=y`（defconfig:1739 已开）
+ 设备 probe 成功 + 用户态读一次 `uevent`（Android 的 `healthd` /
`ueventd` 开机就会读）。**开机路径上就会炸。**

3. `set_bit()` 那一行**不会越界**（我一开始算错了）：
   `psy_drv_properties` 是
   `unsigned long [POWER_SUPPLY_ATTR_CNT / sizeof(unsigned long) + 1]`，
   注意这里的 `sizeof(unsigned long)` 是 **8**，所以是 `[78/8+1] = [10]`，
   共 **10 × 64 = 640 bit**。`set_bit(223, ...)` 在 640 bit 之内 ✅
   （虽然原作者的 `/ sizeof(unsigned long)` 本意应该是
   `BITS_PER_LONG`，但恰好因为 78/8+1=10 而够用，属于"歪打正着"，
   如果属性数量涨到 640 以上就会溢出 —— 见下方建议）。

**所以真正的崩溃点是 `add_prop_uevent()` 的数组越界读，不是栈溢出。**

**建议修法**（必须做，二选一）：

**方案 A（推荐）**：把 `power_supply_attrs[]` 补齐到 224 项。
最简单是加宏批量生成：
```c
#define POWER_SUPPLY_ATTR_QCOM(_name) \
	[POWER_SUPPLY_PROP_ ## _name] = { .attr_name = #_name }
...
/* 在 power_supply_attrs[] 末尾 */
POWER_SUPPLY_ATTR_QCOM(VOLTAGE_MIN_DESIGN),
POWER_SUPPLY_ATTR_QCOM(USB_HC),
...
```
（需要写 146 行，但机械且安全）

**方案 B（临时兜底，1 行，先保命）**：在 `add_prop_uevent()` 开头加边界检查：

```c
static int add_prop_uevent(const struct device *dev, struct kobj_uevent_env *env,
			   enum power_supply_property prop, char *prop_buf)
{
	int ret = 0;
	struct power_supply_attr *pwr_attr;
	struct device_attribute *dev_attr;
	char *line;

	if (prop >= ARRAY_SIZE(power_supply_attrs))     /* ← 加这两行 */
		return 0;

	pwr_attr = &power_supply_attrs[prop];
	...
```

`power_supply_attr_is_visible()` 也要加：

```c
	if (attrno >= POWER_SUPPLY_ATTR_CNT ||
	    !power_supply_attrs[attrno].prop_name)
		return 0;
```

这只防崩溃，属性还是不可见。

**我建议 A（补齐 146 项），因为 B 只是把崩溃藏起来，属性缺失的问题
（Android health HAL 拿不到 `charge_counter` / `battery_type`）依然存在，
而且以后还会有人踩。**

---

## D-3【中】`drivers/platform/oppo/Kconfig:60-66` — help 文本引用了不存在的路径

**位置**：`drivers/platform/oppo/Kconfig`（`config MACH_16061` 的 help）

```
config MACH_16061
	...
	help
	  Support for A57 (MSM8940 / PMI8950) variants.
	  板级 DTS 见 arch/arm64/boot/dts/vendor/qcom/oppo-msm8937/。
```

**问题**：`arch/arm64/boot/dts/vendor/qcom/oppo-msm8937/` 在 6.12-test 分支
**不存在**（那是 4.19 分支的路径）。我确认过
`git ls-tree 6.12-test arch/arm64/boot/dts/` 只有 `qcom/`，没有 `vendor/`。

**建议修法**：改成 "本分支尚未移植 16061 的 DTS"，或者删掉这句。

---

## D-4【中】`msm8953-oppo-common.dtsi:176-183` — 触摸屏注释重复了两次（内容几乎一样）

**位置**：`arch/arm64/boot/dts/qcom/msm8953-oppo-common.dtsi:176-200`

```
176: /*
177:  * 触摸屏：Synaptics，i2c_3 0x20，中断 gpio17、复位 gpio16。
178:  * （potter 是中断 65 / 复位 64，R9s 不同，见 4.19 的
179:  *   oppo-msm8953/msm8953-mtp.dtsi 与 msm8953-pinctrl.dtsi:387）
180:  *
181:  * 注意：4.19 原厂驱动是 synaptics-s3320 私有驱动，mainline 这边只能
182:  * 先挂通用 rmi4。S3320 是否走标准 RMI4 协议需要真机确认；若不兼容，
183:  * 触摸会是这台机器上第一个要补 vendor 驱动的地方。
184:  */
...
188: 	/*
189: 	 * 触摸屏：Synaptics（16027 是 S3320，16 引脚 reset / 17 引脚 irq）。
190: 	 *
191: 	 * 4.19 vendor 驱动是私有的 synaptics-s3320，mainline 这边只能挂通用
192: 	 * rmi4。S3320 是否走标准 RMI4 协议需要真机确认 —— 这是第一个
193: 	 * 上机时要看的点。
194: 	 *
195: 	 * 供电（来自 4.19 oppo-msm8953/msm8953-mtp.dtsi 的 s3320 节点）：
...
```

**问题**：176-184 行和 188-200 行说的是**同一件事**（触摸屏是 S3320、
mainline 只能用 rmi4、需要真机确认），重复了。这是编辑时没有清理的残留。

**建议修法**：删掉 176-184 那段外层注释（保留 188-200 那段更详细的），
或者反过来。

---

## D-5【中】`msm8953-oppo-common.dtsi:24-30` — `qseecom_mem` / `cont_splash_mem` 的 delete-then-redeclare 顺序有坑

**位置**：`arch/arm64/boot/dts/qcom/msm8953-oppo-common.dtsi:24-30, 78-133`

```dts
29: /delete-node/ &cont_splash_mem;
30: /delete-node/ &qseecom_mem;
...
84: qseecom_mem: qseecom@84300000 { ... };
...
90: cont_splash_mem: cont-splash@90001000 { ... };
```

**问题**：`/delete-node/` 之后**重新用同名 label 声明**，在 dtc 里是合法的
（我实测编译通过），但这是**高危模式**：
- 如果将来 `msm8953.dtsi` 改了 `cont_splash_mem` 的名字，`/delete-node/`
  会报 `FATAL ERROR: Label or path cont_splash_mem not found`，**编译失败**。
- 更容易踩的是：`/delete-node/` 之后重新声明，**必须保证重新声明的
  顺序在 delete 之后**（现在是，✓），但如果有人移动代码块就会变成
  "delete 一个正在被引用的节点"。

**建议修法**：不用 delete + redeclare，改成**直接 override**：

```dts
/* msm8953.dtsi 里的节点已经存在，只改 reg 就行 */
&qseecom_mem {
	reg = <0x0 0x84300000 0x0 0x2000000>;
};

&cont_splash_mem {
	reg = <0x0 0x90001000 0x0 (1080 * 1920 * 3)>;
};
```

这样既不改 label，也不需要 delete，更安全。

---

## D-6【已排除】`.github/workflows/build-6.12.yml` — `CFG` 跨 step 传递

**位置**：`.github/workflows/build-6.12.yml`（`配置内核` / `打包产物` /
`打包 AnyKernel3` / `输出构建信息` 四个 step）

我一开始怀疑这里有问题，逐行核对后**确认是对的**，记录在此以免重复排查：

- `CFG` 在「配置内核」step 里用
  `echo "CFG=${CFG}" >> "$GITHUB_ENV"` 写入，**对后续所有 step 可见** ✅
  （这正是修 `set -u` 下 "CFG: unbound variable" 的正确做法，
  step 内的注释也解释了这一点）。
- `VARIANT`（`打包产物`）与 `SUF`（`打包 AnyKernel3`）逻辑完全一致 ——
  都是 `diag` → `-diag`，否则空。不冲突 ✅。
- `KREL` 在每个需要的 step 里都重新
  `make -s ... kernelrelease` 算了一遍，没有跨 step 复用局部变量 ✅。
- `打包产物` 里的 `chmod +x ci/mkflashable.sh` 虽然排在 `ls -la out/`
  之后，但 **同一个 job 的 workspace 在 step 之间是持久的**，
  而且 `git ls-tree` 显示 `ci/mkflashable.sh` 本来就是 `100755`，
  这一步是冗余保险，无害 ✅。

**未发现问题。**（唯一可改进的是：如果「配置内核」step 提前失败，
`${CFG}` 在 `set -u` 下会报 `unbound variable` 而不是更清楚的错误 ——
但那种情况下 workflow 已经停了，不影响正确性。）

---

## D-7【中】`ci/mkflashable.sh:44-52` — `rm -rf ak3-tpl` 依赖当前工作目录；`sed` 改 `anykernel.sh` 无校验

**位置**：`ci/mkflashable.sh:44-52`

```bash
OUT="${1:-out}"
ZIP_NAME="${2:-}"

# 转成绝对路径：下面会 cd 进 ak3-tpl，相对路径会失效
OUT="$(cd "$OUT" && pwd)"          ← 这一句在 OUT 不存在时会失败
[ -f "$OUT/Image.gz-dtb" ] || {
  echo "缺少 $OUT/Image.gz-dtb" >&2
  ...
  exit 1
}
...
rm -rf ak3-tpl
git clone --depth 1 https://github.com/osm0sis/AnyKernel3.git ak3-tpl
cd ak3-tpl
```

**问题 1**：`OUT="$(cd "$OUT" && pwd)"` 在 `set -euo pipefail` 下，
如果 `out/` 目录**不存在**，`cd` 失败 → 整个脚本退出，**但错误信息是
`cd: no such file or directory: out`**，而不是脚本自己的
"缺少 out/Image.gz-dtb"。可读性差，但不是功能 bug。

**问题 2（真 bug）**：`rm -rf ak3-tpl` 在**仓库根目录**执行。
如果用户本地恰好有个叫 `ak3-tpl` 的目录（比如上次跑剩下的），会被删掉 ——
这是预期行为。但如果用户**在别的目录**跑这个脚本（脚本没有 `cd` 到
仓库根），`rm -rf ak3-tpl` 会删掉**当前目录**下同名目录，可能误删。

**建议修法**：脚本开头加 `cd "$(git rev-parse --show-toplevel)"`，
或者把 `ak3-tpl` 放到 `$OUT/../.ak3-tpl` 之类的位置。

**问题 3（低）**：`git clone` 没有重试/兜底。CI 上 GitHub 抽风时整个
打包 step 失败。可以加 `--depth 1 --single-branch`（已有 `--depth 1`）
并配合 `retry`，属于健壮性改进，不是 bug。

**问题 4（中）**：`sed -i 's|^BLOCK=.*|BLOCK=...|' anykernel.sh`
如果 AnyKernel3 上游改了 `anykernel.sh` 的变量名（比如从 `BLOCK=` 改成
`BLOCK=;` 带分号），`sed` 会**静默不匹配**，然后打包出来的 zip 里
`anykernel.sh` 还是默认的 `BLOCK=/dev/block/bootdevice/by-name/boot;`
（刚好一样，所以现在没事）—— 但如果上游改成别的分区名，就会刷错分区。
**建议加校验**：

```bash
grep -q '^BLOCK=/dev/block/bootdevice/by-name/boot;' anykernel.sh || {
  echo "anykernel.sh 的 BLOCK 没改成功" >&2
  exit 1
}
```

---

## D-8【中】`ci/targets` — 误提交的垃圾文件

**位置**：`ci/targets`

内容只有一行：
```
dtbfix-1793363110
```

**问题**：这明显是某次调试留下的临时文件（`dtbfix-<timestamp>`），
被 `git add` 进去了。它不属于项目内容。

**建议修法**：`git rm ci/targets`。

---

## D-9【中】`REVIEW-6.12.md` 里提到的 `FLASH-GUIDE.md` / `README-6.12-android.md` 与代码不一致

**位置**：`README-6.12-android.md`（多个地方）

**问题**：
1. README 里说"屏幕 1080×1920 **simple-framebuffer**"（第 71 行左右的表格），
   但 `msm8953-oppo-common.dtsi:40-52` 的注释明确说**已经改成真正的
   MDSS/DSI 链路，不再用 simpledrm**。文档和代码矛盾。
2. README 里说"背光 PMI8950 WLED"（表格），但 dts 里实际用的是
   **LM3697 @ i2c_2 0x36**（`msm8953-oppo-common.dtsi:468-489`）。
   WLED 节点（`pmi8950_wled`）虽然 `status = "okay"`，但两个面板驱动
   都不用它（`panel-oppo-*.c` 里没有 backlight 引用，注释也说了
   "两个 OPPO 面板驱动都把亮度写死在初始化序列里"）。
   所以**实际亮的是 LM3697，不是 WLED**。
3. README 的"已验证"段里写 `Image.gz-dtb 16,269,506 bytes`，
   但没有说明 **dtb 是否真的被拼进去了**。考虑到 A-1 的 bug，
   那个数字很可能是"Image.gz + 0 个 dtb"的尺寸。

**建议修法**：把 README 里这几处同步成当前代码状态，
尤其是"屏幕"和"背光"两行。

---

## D-10【中】`drivers/soc/oppo/oppo_project.c:283` — `ioremap(QFPROM_RAW_SERIAL_NUM, 4)` 用了物理地址常量，没走 DT/nvmem

**位置**：`drivers/soc/oppo/oppo_project.c:283-289`

```c
283: serialID_addr = ioremap(QFPROM_RAW_SERIAL_NUM, 4);
284: if (serialID_addr) {
285: 	g_SerialID = __raw_readl(serialID_addr);
286: 	iounmap(serialID_addr);
```

**问题**：`QFPROM_RAW_SERIAL_NUM` 是 `0x000a0128`（硬编码物理地址）。
`ioremap()` 在 6.x 里对**非内存区域**的映射需要 `ioremap()` 返回的
指针用 `readl()` 读（`__raw_readl()` 不保证字节序和屏障）。
而且 QFPROM 在 6.x 里应该走 `nvmem` 框架（`drivers/nvmem/qfprom.c`）。

这不是"会崩"的 bug（`ioremap` 成功就能读），但是**架构上过时**，
且如果 `CONFIG_NVMEM_QFPROM` 也开了，会有两个驱动抢同一块 Phys。

**建议修法**：改成用 `nvmem_device_read()`，或者至少在注释里说明
为什么不用 nvmem。这一项是"从 4.19 继承"的，优先级低。

---

## D-11【低】`msm8953-oppo-common.dtsi:225-232` — `pmi8950_wled` 开着但没人用（实际背光是 LM3697）

**位置**：`arch/arm64/boot/dts/qcom/msm8953-oppo-common.dtsi:225-232`

```dts
225: &pmi8950_wled {
226: 	qcom,current-limit-microamp = <25000>;
227: 	qcom,num-strings = <3>;
228: 	qcom,external-pfet;
229: 	qcom,cabc;
230:
231: 	status = "okay";
232: };
```

**先说不是问题的部分**：`qcom,external-pfet` / `qcom,cabc` 是**无值
boolean**，写法正确（`Documentation/devicetree/bindings/leds/backlight/
qcom-wled.yaml:49,119` 就是 boolean）。注释说"参数照抄 potter"，
而 potter 确实一样（`msm8953-motorola-potter.dts:133-140`）。✅
`current-limit-microamp = <25000>` / `num-strings = <3>` 也与 potter 一致 ✅。

**真正的问题（低）**：R9s/R9sk 实际背光是 **LM3697 @ i2c_2 0x36**
（468-489 行），而 `pmi8950_wled` **没有任何 consumer** ——
两个面板驱动（`panel-oppo-ea8064.c` / `panel-oppo-jdi-r63452.c`）
都**不引用** WLED backlight（面板驱动注释里也说了"亮度写死在初始化
序列的 0x51/0x53 里，不导出 backlight 设备"）。

所以这个 `status = "okay"` 只是白白 probe 了一个没人用的 WLED 设备，
而且它会去申请 PMI8950 的 WLED 寄存器/iommu 资源。

**建议修法**（二选一）：
- 如果确定用 LM3697，把这里改成 `status = "disabled";`（或者整段删掉）；
- 如果想保留 WLED 做备选，在注释里写清楚"当前未使用，仅作备选"。

---

# 附：明确「未发现问题」的检查项

以下项目我逐条核对过，**没有发现问题**：

## DTS/DTB

- **reserved-memory 重叠**：用 dtc 反编译后手算全部 15 个区域，**无重叠**
  （详见 B-4b 的地址表）。`ramoops` 的 `console+pmsg+record = 0x800000`
  与 `reg` 的 size 完全相等，✓。
- **regulator label 引用**：`pm8953_l1..l23` / `lab` / `ibb` 全部在
  `drivers/regulator/qcom_smd-regulator.c:1122-1155` 的
  `rpm_pm8953_regulators[]` 里存在（l1/l2/l3/l5/l6/l7/l8/l9/l10/l11/l12/l13/
  l15/l16/l17/l19/l22/l23 都有）。`pm8953_s1/s3/s4` 也有。✓
  （注意 `pm8953_l4` / `l14` / `l18` / `l20` / `l21` 没在 dts 里声明，
  但驱动里有，不影响。）
- **i2c 设备地址 vs 4.19**：
  | 设备 | 6.12 dts | 4.19 dts | 一致？ |
  |---|---|---|---|
  | bq27541 | i2c_5 0x55 | i2c_5 0x55 | ✅ |
  | stm8s-vooc | i2c_5 0x26 | i2c_5 0x26 | ✅ |
  | ak4376 | **根节点 0x10** | i2c_8 0x10 | ❌ 见 B-3 |
  | lm3697 | i2c_2 0x36 | i2c_2 0x36 | ✅ |
  | 触摸屏 | i2c_3 0x20 | i2c_3 0x20 | ✅ |
- **model / compatible 字符串**：
  - 6.12: `model = "OPPO R9s"` / `compatible = "oppo,r9s", "qcom,msm8953"`
  - 4.19: `model = "Qualcomm Technologies, Inc. MSM8953 + PMI8950 MTP"` /
    `compatible = "qcom,msm8953-mtp", "qcom,msm8953", "qcom,mtp"`
  - **不一致**，但 6.12 这边是**有意**改成 OPPO 机型的（4.19 那个字符串
    是 CAF 的通用 MTP 名）。**`qcom,msm-id = <293 0>` 和
    `qcom,board-id = <8 0 16017>` / `<8 0 16027>` 与 4.19 完全一致** ✅
    —— 这三个才是 ABL 选 DTB 用的，model 字符串不参与匹配。
- **`status` 遗漏**：`&mdss` / `&mdp` / `&mdss_dsi0` / `&lpass` /
  `&usb3` / `&i2c_2/3/5/8` / `&sdhc_1/2` / `&hsusb_phy` 都设了
  `okay`。**唯一漏的是 `&mdss_dsi0_phy`**（见 B-1）。
- **gpio 号 vs 4.19**：音量键 85/86 ✅、触摸 irq 17 / reset 16 ✅、
  SD cd 133 ✅、面板 reset 131 ✅、LM3697 enable 46 ✅、
  VOOC switch 50/9 / reset 94 / clock 89 / data 90 ✅、
  touch enable 134/132 ✅、ak4376 pdn 28 / enable 62 ✅
  （有一处**语义不一致**：4.19 的 `ak4376,pdn-gpio = <&tlmm 28 0x00>` 里
  `0x00` = `GPIO_ACTIVE_HIGH`，而 6.12 的 dtsi:524 写的是
  `GPIO_ACTIVE_LOW`，极性反了。但因为 `ak4376.c:1488` 用的是
  legacy `of_get_named_gpio(np, "ak4376,pdn-gpio", 0)`，该 API
  **只返回 gpio 编号、不解析 flags**，所以**功能上没影响**。
  建议仍改成 `GPIO_ACTIVE_HIGH` 以保持语义一致 —— 归入 D-15 的
  注释/代码一致性讨论。）

## Kconfig / defconfig

- **defconfig 里所有 `CONFIG_` 符号存在性**：把 1738 个符号
  （`msm8953-oppo_defconfig`）和 240 个（diag）逐一和全部 1826 个
  Kconfig 文件里提取的 20705 个符号做了差集。
  defconfig 里**只有 3 个符号不存在**：
  - `MSM_RPM_SMD`（见 A-2）
  - `PSTORE_DEFLATE_COMPRESS`（见 C-1）
  - `USB_CONFIGFS_UEVENT`（见 C-2）
  diag 里**3 个**：`PSTORE_DEFLATE_COMPRESS` / `QCOM_RPMCC` /
  `USB_CONFIGFS_F_ADB`。
  - `QCOM_RPMCC` 不存在（6.12 只有 `QCOM_CLK_RPM` / `QCOM_CLK_SMD_RPM`），
    **这是一个新的遗漏**（见 D-12）。
  - `USB_CONFIGFS_F_ADB` 不存在（6.12 只有 `USB_CONFIGFS_F_FS`），
    **另一个遗漏**（见 D-12）。
- **新 Kconfig 的依赖链自洽性**：
  - `drivers/power/supply/oppo/Kconfig`：`BATTERY_GAUGE_OPPO_BQ27541`
    `depends on I2C` —— `I2C` 由 `CONFIG_DRM=y` **select** 上来
    （`drivers/gpu/drm/Kconfig:16` 有 `select I2C`），所以**实际可见**。
    但这是**隐式依赖**：如果哪天关了 DRM，触摸和 bq27541 会一起消失。
    建议显式加 `CONFIG_I2C=y` 到 defconfig。
    `CHARGER_OPPO_VOOC` **没有 depends on I2C** —— 但它用
    `devm_gpiod_get_array()` 和 `module_i2c_driver`，**必须**有 I2C。
    这是一个**真实的 Kconfig 缺陷**（见 D-13）。
  - `drivers/soc/oppo/Kconfig`：`OPPO_DEVICE_INFO` `default y if MACH_OPPO`，
    `MACH_OPPO` 由 `MACH_16017`/`MACH_16027` **select**，链路成立 ✅。
    但 `drivers/soc/oppo/Makefile:1` 是
    `obj-$(CONFIG_MACH_OPPO) += oppo_project.o boot_mode.o`，
    和 Kconfig 里 `OPPO_DEVICE_INFO` 控制 `device_info.o` 是**两套符号**，
    逻辑上没问题（`MACH_OPPO` 控制 project/boot_mode，
    `OPPO_DEVICE_INFO` 控制 device_info），但 `default y if MACH_OPPO`
    意味着 **`OPPO_DEVICE_INFO` 永远跟着 `MACH_OPPO`**，
    那 defconfig 里再写 `CONFIG_OPPO_DEVICE_INFO=y` 是多余的。✅ 不算错。
  - `drivers/platform/oppo/Kconfig`：`MACH_OPPO` / `MACH_16017` /
    `MACH_16027` / `MACH_OPPO_MSM8953` / `MACH_OPPO_MSM8937` 都
    `depends on ARCH_QCOM`，`MACH_16017` select `MACH_OPPO_MSM8953`
    select `MACH_OPPO`。链路自洽 ✅。
    **但整个 `drivers/platform/oppo/` 没有 Makefile**（见 A-3）。

## 新移植驱动

- **i2c probe/remove 签名**：
  - `oppo_bq27541.c:1200` `static int bq27541_driver_probe(struct i2c_client *client)` ✅
  - `oppo_bq27541.c:1183` `static void bq27541_driver_remove(...)` ✅
  - `vooc_charger.c:196` `static int vooc_probe(struct i2c_client *client)` ✅
  - `vooc_charger.c:236` `static void vooc_remove(...)` ✅
  - `ak4376.c:1653` `static int ak4376_i2c_probe(struct i2c_client *i2c)` ✅
  - `ak4376.c:1701` `static void ak4376_i2c_remove(...)` ✅
  全部符合 6.3+/6.11+ 的新签名。
- **`proc_ops`**：`device_info.c:106` / `oppo_project.c:82` 起的 7 个
  `struct proc_ops` 全部把 `.read` → `.proc_read`、`.llseek` → `.proc_lseek`、
  `.release` → `.proc_release`、`.open` → `.proc_open` ✅。
  `device_info.c:653` 的 `mainboard_res_proc_fops` 也是 `proc_ops` ✅。
- **`platform_driver.remove` 返回 void**：
  - `device_info.c:702` `static void devinfo_remove(...)` ✅
  - `msm8953-oppo-sndcard.c` 没有 `.remove`（用 `devm_snd_soc_register_card`
    自动清理）✅
- **`timer_setup` / `init_timer`**：新驱动里**没有**任何 `timer_list`
  用法（只用 `delayed_work`）✅。
- **`class_create`**：新驱动里**没有** `class_create` ✅。
- **`EXPORT_SYMBOL` 对照调用方**：
  - `get_boot_mode` / `qpnp_is_power_off_charging` / `qpnp_is_charger_reboot`
    → `boot_mode.h` 里有声明，**但全树没有任何调用方**（grep 只找到
    `boot_mode.c` 自己）。属于"导出了没人用"，不算错。
  - `fg_oppo_register` / `fg_oppo_unregister` → 定义在
    `qpnp-fg.c:8710/8722`，**有 `EXPORT_SYMBOL`** ✅，
    调用方 `oppo_bq27541.c:674/1185` ✅。
    **但**：`oppo_bq27541.c:47-48` 又**重复声明**了这两个函数
    （`int fg_oppo_register(struct oppo_gauge_driver *);`），
    而 `qpnp-fg.c` 里 `fg_oppo_register` 的完整定义在
    `#ifdef CONFIG_BATTERY_GAUGE_OPPO` 里。
    **如果 `CONFIG_QPNP_FG=n` 但 `CONFIG_BATTERY_GAUGE_OPPO=y`**
    （defconfig 里两个都是 =y，所以现在没事），会 **链接失败**
    （undefined reference to `fg_oppo_register`）。
    建议在 `oppo_bq27541.c` 里加 `#if IS_ENABLED(CONFIG_QPNP_FG)` 保护。
  - `qtee_shmbridge_*` → 6 个 `EXPORT_SYMBOL` 都有 ✅。
  - `scm_call2` / `scm_call2_noretry` / `scm_call2_atomic` /
    `scm_get_version` → 都有 `EXPORT_SYMBOL` ✅。
  - `register_device_proc` → **定义在 `device_info.c:113`，但没有
    `EXPORT_SYMBOL`**。调用方 `oppo_bq27541.c:1088` 在**另一个模块**里
    （`BATTERY_GAUGE_OPPO_BQ27541` vs `OPPO_DEVICE_INFO`）。
    因为两个都是 `=y`（built-in），链接时不走 symbol 表，所以现在**能过**。
    但如果哪天把其中一个改成 `=m`，**就会链接失败**。
    建议加 `EXPORT_SYMBOL_GPL(register_device_proc);`（见 D-14）。
- **内存泄漏 / 空指针 / 错误路径**：见 C-4 ~ C-8。
- **`gpiod` API**：
  - `vooc_charger.c:36-38` 的 `gpiod_set_array_value_cansleep(ndescs,
    desc, NULL, bitmap)` —— 6.x 签名是
    `(unsigned int array_size, struct gpio_desc **desc_array,
     struct gpio_array *array_info, unsigned long *value_bitmap)`，
    **4 参数版本 ✅ 正确**（4.19 是 3 参数）。
    用 `DECLARE_BITMAP(bitmap, 4)` + `bitmap_fill(bitmap, ndescs)` 构造
    bitmap 也是对的 ✅。
  - `ak4376.c` 混用 **legacy `gpio_request`/`gpio_direction_output`**
    和 **`devm_gpiod_get_optional`** —— legacy API 在 6.12 里**还在**
    （`include/linux/gpio.h`），但**已 deprecated**，且
    `CONFIG_GPIOLIB_LEGACY` 将来会删。现在能编过 ✅。
    `ak4376_parse_dt()` 里注释明确说了"不要再 gpio_request"，
    **但 `ak4376_probe()` 里又 `gpio_request()` 了** —— 这是**两处
    代码矛盾**（注释说不要，代码在做）。不过实际是
    `parse_dt` 只解析、`probe` 申请，**逻辑是对的**，只是注释写得
    像是"整个文件都不该 gpio_request"，容易误导。
    **这一项标为 D-15（注释与代码矛盾）**。

## MM / vmstat / fork / mm_types_task

- **`resident_page_types[]` vs `NR_MM_COUNTERS`**：
  `mm_types_task.h:36` 加了 `MM_UNRECLAIMABLE`（在 `NR_MM_COUNTERS` 之前），
  `kernel/fork.c:159` 同步加了 `NAMED_ARRAY_INDEX(MM_UNRECLAIMABLE)`。
  `BUILD_BUG_ON_MSG(ARRAY_SIZE(resident_page_types) != NR_MM_COUNTERS, ...)`
  会通过 ✅（5 项 vs 5 项）。
- **`NR_VM_NODE_STAT_ITEMS` 相关数组**：`vm_zone_stat[]` /
  `vm_node_stat[]`（`mm/vmstat.c:164`）都是按枚举值开的 ✅。
  `vm_node_stat_diff[]` / `vm_stat[]`（`mmzone.h:783/1496`）也是 ✅。
- **`vmstat_text[]`**：**顺序错位**，见 D-1（这是唯一的问题）。
- **`include/linux/power_supply.h` 私有属性重名**：
  我把 224 项全解析了一遍，**没有重名** ✅。
  （`POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN` 只出现一次，120 行是
  `VOLTAGE_MAX_DESIGN`，不冲突。）
  **但 `power_supply_attrs[]` 没跟上**，见 D-2。

## CI / 打包

- **YAML 语法**：`python3 -c "import yaml; yaml.safe_load(...)"` 通过 ✅。
  `jobs` 是 `build` / `release` 两个，结构正确。
- **`set -euo pipefail` 下的裸 grep**：
  CI 里所有 `grep` 都加了 `|| true` 或包在 `$(... || true)` 里：
  - 行 148 `grep -E '...' .config || true` ✅
  - 行 168 `grep -E "^CONFIG_${sym}=" .config | head -1 || true` ✅
  - 行 201 `grep -E '...' .config || true` ✅
  - 行 243 `v="$(grep -E ... .config || true)"` ✅
  - 行 259 `v="$(grep -E ... .config || true)"` ✅
  - DTB 自检里 `dtc ... | grep -q "$pat"` —— **这里没有 `|| true`**，
    但它是 `if` 的条件，`set -e` 对 `if` 条件**不生效**，✅。
  - `ci/mkflashable.sh` 里没有裸 grep，✅。
  **未发现问题。**
- **未定义变量 / 跨 step 变量**：`CFG` 通过 `GITHUB_ENV` 传递 ✅，
  `KREL` 在每个 step 里重新 `make kernelrelease` 算 ✅，
  `VARIANT` 只在本 step 用 ✅。
  **唯一隐患**：`打包产物` step 里 `${CFG}` 在 `set -u` 下如果
  「配置内核」step 失败会 `unbound variable`，但那种情况下
  workflow 已经停了，不会走到这里。**未发现真问题。**
- **`arch/arm64/boot/Makefile` 的 `Image.gz-dtb` 规则**：
  **有 bug**，见 A-1。
- **`dtb-$(CONFIG_ARCH_QCOM) += msm8953-oppo-r9s.dtb`**：
  `arch/arm64/boot/dts/qcom/Makefile:71-72` 已经加了 ✅。

## 其它

- **遗留 `TODO` / `FIXME` / `XXX`**：
  - `oppo_bq27541.c:37` `// FIXME: Hacks for compilation` —— 见 C-6
  - `vooc_charger.c:84` `// FIXME: May also be returning 1` ——
    `chip_reply_info = 2` 是硬编码，注释说可能是 1。**这是真问题**
    （VOOC 协议回复值错了快充不工作），标为 D-16。
  - `vooc_charger.c:107` `// TODO!`、`:122` `// TODO: Investigate...`、
    `:144` `// TODO: Implement firmware update` —— 都是未完成功能，
    但代码里已经处理了（`break`），不算 bug。
  - `msm8953-oppo-common.dtsi:504` `FIXME(unverified)` —— 见 B-3
  - `panel-oppo-ea8064.c:140` `FIXME(unverified)` —— 见 D-17
- **注释与代码矛盾**：
  - `msm8953-oppo-common.dtsi:514` 说 "AK4376 @ i2c_8"，代码在根节点 → **B-3**
  - `msm8953-oppo-common.dtsi:40-52` 说"不再用 simpledrm"，
    `README-6.12-android.md` 说"simple-framebuffer" → **D-9**
  - `devices/soc/oppo/device_info.c:696` 报错说 `prjVersion` 其实是 `mainboard` → **C-6**
  - `ak4376.c:1498-1510` 注释说"不要再 gpio_request"，`probe` 里又做了 → D-15
- **误提交的临时文件**：`ci/targets`（见 D-8）。`.orig` / `.rej` / `~` /
  `.bak` / `.swp` 全树 `git ls-tree` 检查过，**没有** ✅。

---

# 附录：D-12 ~ D-17（补充问题）

## D-12【高】`arch/arm64/configs/msm8953-oppo-diag_defconfig:42,91` — `QCOM_RPMCC` 与 `USB_CONFIGFS_F_ADB` 符号不存在

**位置**：
- `arch/arm64/configs/msm8953-oppo-diag_defconfig:42` — `CONFIG_QCOM_RPMCC=y`
- `arch/arm64/configs/msm8953-oppo-diag_defconfig:91` — `CONFIG_USB_CONFIGFS_F_ADB=y`

**问题**：
- 6.12 里没有 `QCOM_RPMCC`，RPM clock controller 的符号是
  `QCOM_CLK_RPM`（`drivers/clk/qcom/Kconfig:121`）和
  `QCOM_CLK_SMD_RPM`（`:133`）。`qcom,rpmcc-msm8953` 的 compatible 在
  `drivers/clk/qcom/clk-smd-rpm.c:1219`，由 `QCOM_CLK_SMD_RPM` 驱动。
- 6.12 里没有 `USB_CONFIGFS_F_ADB`，只有 `USB_CONFIGFS_F_FS`
  （`drivers/usb/gadget/Kconfig:384`）。Android 的 adb 走 **f_fs**
  （userspace adbd 直接操作 `/dev/usb-ffs/adb`），没有独立的 f_adb。

**后果**：两个符号**静默丢弃**。后果：
- `QCOM_RPMCC` 丢了 → **诊断内核拿不到 RPM 时钟**（`rpmcc` 是
  `xo_board`、`sleep_clk` 等时钟源），可能起不来。
- `USB_CONFIGFS_F_ADB` 丢了 → **诊断内核没 adb**，这直接违背了
  诊断内核"能接 adb 抓日志"的设计目标。

**建议修法**：
```
CONFIG_QCOM_CLK_SMD_RPM=y
CONFIG_USB_CONFIGFS_F_FS=y
```

## D-13【中】`drivers/power/supply/oppo/Kconfig` — `CHARGER_OPPO_VOOC` 缺 `depends on I2C`

**位置**：`drivers/power/supply/oppo/Kconfig`

```
config CHARGER_OPPO_VOOC
	bool "OPPO VOOC charging technology"
	help
	  Support for OPPO VOOC charging technology.
```

**问题**：`vooc_charger.c` 是 `module_i2c_driver()`，**必须**有 I2C。
但没有 `depends on I2C`。当前能编是因为 `CONFIG_DRM=y` 会
`select I2C`（`drivers/gpu/drm/Kconfig:16`）。如果哪天关了 DRM，
VOOC 驱动会 `error: implicit declaration of function 'i2c_...'`。

**建议修法**：
```kconfig
config CHARGER_OPPO_VOOC
	bool "OPPO VOOC charging technology"
	depends on I2C
```

同时建议在 defconfig 里显式加 `CONFIG_I2C=y`（现在靠 DRM select，
是隐式依赖，见上文）。

## D-14【中】`drivers/soc/oppo/device_info.c:113` — `register_device_proc()` 没有 `EXPORT_SYMBOL`

**位置**：`drivers/soc/oppo/device_info.c:113`

**问题**：定义在 `device_info.o`（受 `CONFIG_OPPO_DEVICE_INFO` 控制），
被 `oppo_bq27541.o`（受 `CONFIG_BATTERY_GAUGE_OPPO_BQ27541` 控制）调用。
两者现在是不同的 object，但因为都是 `=y`（built-in），链接时
`register_device_proc` 是全局符号，能解析 ✅。

**但如果任何一个改成 `=m`，就会 undefined reference**。
而且 `device_info.c` 里没有任何 `EXPORT_SYMBOL`，所以
`oppo_bq27541.o` 如果作为模块，**一定链接失败**。

**建议修法**：在 `device_info.c:113` 的实现后面加
```c
EXPORT_SYMBOL_GPL(register_device_proc);
```
（`include/soc/oppo/device_info.h:28` 已有声明，不用改。）

## D-15【低】`sound/soc/codecs/ak4376.c:1498-1510` 注释与代码矛盾

**位置**：`sound/soc/codecs/ak4376.c:1498-1510`（注释）与 `:1520-1545`（代码）

注释说：
```
1498:  * 注意：这里**不要**再 gpio_request()。
1499:  * ak4376_probe() 里本来就已经对同一个 priv_pdn_en 做了
1500:  *     gpio_request(ak4376->priv_pdn_en, "ak4376 pdn");
...
1510:  * 结果就是完全没有声音。这里只解析 DT，申请交给 probe。
```

代码 `ak4376_probe()`（1520 行）里确实 `gpio_request()` 了 ✅。
**所以逻辑是对的**，注释是在解释"为什么 parse_dt 里不要再申请一次"。

但注释放在 `ak4376_parse_dt()` 函数体里，而它描述的是 `probe` 的行为，
读者容易误以为"整个文件都不该 gpio_request"。**属于注释位置不当**，
不是 bug。建议把注释移到函数上方的函数头注释里。

## D-16【中】`drivers/power/supply/oppo/vooc_charger.c:84` — `chip_reply_info = 2` 硬编码，注释自己都不确定

**位置**：`drivers/power/supply/oppo/vooc_charger.c:82-84`

```c
82: static void vooc_poll_func(struct work_struct *work)
83: {
84: 	int i, data = 0, bit;
85: 	int chip_reply_info = 2; // FIXME: May also be returning 1
```

**问题**：VOOC 协议里 STM8S 需要收到 MCU 的回复值（2 或 1），
这个值决定了快充是否启动。硬编码成 2 且注释说"也可能是 1"，
意味着**快充大概率不工作**（如果真机需要 1，快充永远协商失败）。

**建议修法**：这个值应该从 DT 读（`of_property_read_u32(np,
"oppo,chip-reply-info", &chip_reply_info)`），在
`msm8953-oppo-common.dtsi` 的 `stm8s_vooc` 节点里给个默认值。
或者至少让它在 `vooc_probe()` 里可配。

## D-17【低】`drivers/gpu/drm/panel/panel-oppo-ea8064.c:140` — `vneg` supply 是 optional 但 DT 没给

**位置**：`drivers/gpu/drm/panel/panel-oppo-ea8064.c:140-145`（注释）
与 `msm8953-oppo-r9s.dts:42-43`（只给了 `avdd` / `vci`）

面板驱动注释说：
> VNEG(ibb) 是可选的负压轨，FIXME(unverified)：dts 给了 vneg-supply 才使能

而 dts 只给了：
```dts
vci-supply = <&pm8953_l6>;
avdd-supply = <&lab>;
```

**问题**：AMOLED 的 VNEG（负压）**通常是必需的**（EA8064 datasheet 里
VSP/VSN 都要）。不给的话面板可能能亮但**有闪烁/显示异常**。

**但**：这条是面板驱动的**有意设计**（`devm_regulator_get_optional`），
注释也说了"避免无证据直接上负压轨"。所以**现在不接是保守选择**，
不算 bug。建议在 README 里标为"待真机确认"。

---

# 修复优先级建议

1. **A-1**（`dtbs` 无规则）—— 不修 CI 就编不过。
2. **A-2**（`MSM_RPM_SMD` 不存在）—— 不修 bus scaling 全废。
3. **B-1**（DSI PHY disabled）—— 不修屏幕永远不亮。
4. **B-3**（AK4376 挂错总线）—— 不修声卡注册不上。
5. **B-2**（graph 端点不成对）—— 不修 DSI bind 失败。
6. **D-2**（`power_supply_attrs[]` 只有 78 项，`qpnp-smbcharger` 已把
   `POWER_SUPPLY_PROP_REAL_TYPE`（idx 82）放进 `properties[]`）——
   不修开机读 uevent 就**数组越界 panic**。
7. **D-1**（`vmstat_text[]` 顺序错位 4 项）—— 不修 `/proc/vmstat` 名字全错。
8. **D-12**（诊断内核的 `QCOM_RPMCC` / `USB_CONFIGFS_F_ADB`）——
   诊断内核拿不到时钟/没 adb，等于白做。
9. 其余 C/D 类。

---

*报告生成时间：2026-10-30*
*审查者：自动化静态分析（无本地编译、无源码修改）*
