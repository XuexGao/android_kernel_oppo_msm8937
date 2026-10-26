/*
 * Copyright (c) 2016, The CyanogenMod Project
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <asm/setup.h>

#include <linux/export.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <soc/qcom/smem.h>

#include <soc/oppo/boot_mode.h>

static int ftm_mode __ro_after_init = MSM_BOOT_MODE__NORMAL;
static int __init board_mfg_mode_init(char *str)
{
	if (!strncmp(str, "ftmrecovery", 5))
		ftm_mode = MSM_BOOT_MODE__RECOVERY;

	pr_debug("%s: oppo_ftm_mode=%d\n", __func__, ftm_mode);

	return 1;
}
__setup("oppo_ftm_mode=", board_mfg_mode_init);

int get_boot_mode(void)
{
	return ftm_mode;
}
EXPORT_SYMBOL(get_boot_mode);

char boot_mode[16] __ro_after_init;
bool qpnp_is_power_off_charging(void)
{
	return !strcmp(boot_mode, "charger");
}
EXPORT_SYMBOL(qpnp_is_power_off_charging);

char charger_reboot[16];
bool qpnp_is_charger_reboot(void)
{
	pr_err("%s charger_reboot:%s\n", __func__, charger_reboot);
	return !strcmp(charger_reboot, "1");
}
EXPORT_SYMBOL(qpnp_is_charger_reboot);

static int __init oppo_charger_reboot(void)
{
	int i;
    char * substr = strstr(boot_command_line, "oppo_charger_present="); 
    if (NULL == substr)
		return 0;
    substr += strlen("oppo_charger_present=");
	for (i = 0; substr[i] != ' '; i++) {
        charger_reboot[i] = substr[i];
    }
    charger_reboot[i] = '\0';

    pr_info("%s: oppo_charger_present=%s\n", __func__, charger_reboot);
	return 1;
}

arch_initcall(oppo_charger_reboot);
