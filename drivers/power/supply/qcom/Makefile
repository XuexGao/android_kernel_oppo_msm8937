# SPDX-License-Identifier: GPL-2.0-only
obj-$(CONFIG_QPNP_SMBCHARGER)	+= qpnp-smbcharger.o
obj-$(CONFIG_QPNP_FG)		+= qpnp-fg.o

# batterydata 解析（电量计读电池曲线要用）
qpnp-fg-y := qpnp-fg.o batterydata-lib.o
obj-$(CONFIG_QPNP_SMBCHARGER) += pmic-voter.o
