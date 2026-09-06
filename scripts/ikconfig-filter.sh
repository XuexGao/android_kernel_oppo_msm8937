#!/bin/sh
# SPDX-License-Identifier: GPL-2.0
#
# Pre-filter for the .config blob that gets embedded into /proc/config.gz.
#
# The kernel ships the exact .config used for the build through
# CONFIG_IKCONFIG_PROC, which means every symbol added on top of the base tree
# is published to userspace in plain text. This filter drops those lines from
# the *published copy only* - the .config that drives the build is not modified,
# and every other symbol keeps its real value so the exposed copy still reads
# like a normal build of this tree.
#
# Usage: scripts/ikconfig-filter.sh <path to .config>

if [ $# -ne 1 ] || [ ! -f "$1" ]; then
	echo "usage: $0 <kconfig-file>" >&2
	exit 1
fi

# KSU / SUSFS / KPM: the whole optional feature set, both the "CONFIG_X=y" and
# the "# CONFIG_X is not set" forms.
grep -Ev '^#? *CONFIG_(KSU|SUSFS|KPM)[A-Z0-9_]*([ =]|$)' "$1"
