// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2008-2012, OPPO Mobile Comm Corp., Ltd
 * Copyright (c) 2025 Ricky Cheung <rcheung844@gmail.com>
 */

#ifndef _OPPO_GAUGE_H_
#define _OPPO_GAUGE_H_

extern int bq27541_device_type;

/**
 * struct oppo_gauge_driver - represent an OPPO fuel gauge device driver
 * @capacity: Function returning battery capacity
 * @temp: Function returning battery temperature
 * @charge_now: Function returning battery charge in uAh
 * @current_now: Function returning battery current in uA
 * @voltage_now: Function returning battery voltage in uV
 */
struct oppo_gauge_driver {
	int (*capacity)(void);
	int (*temp)(void);
	int (*charge_now)(void);
	int (*current_now)(void);
	int (*voltage_now)(void);
};

#endif /* _OPPO_GAUGE_H */
