/**
 * Copyright 2008-2013 OPPO Mobile Comm Corp., Ltd, All rights reserved.
 * VENDOR_EDIT:
 * FileName:devinfo.h
 * ModuleName:devinfo
 * Author: wangjc
 * Create Date: 2013-10-23
 * Description:add interface to get device information.
 * History:
   <version >  <time>  <author>  <desc>
   1.0		2013-10-23	wangjc	init
*/

#ifndef _DEVICE_INFO_H
#define _DEVICE_INFO_H

enum {
	MAINBOARD_RESOURCE0 = 0,
	MAINBOARD_RESOURCE1 = 1,
	MAINBOARD_RESOURCE2 = 2,
};

struct manufacture_info {
	char *version;
	char *manufacture;
};

int register_device_proc(char *name, char *version, char *manufacture);

#ifdef CONFIG_MACH_OPPO
#include <linux/types.h>

/*
 * Camera module vendor info.
 *
 * In the 4.9 OPPO kernel these live in the single camera driver
 * (techpack/camera-legacy-m).  4.19 builds three drivers that all bind
 * "qcom,camera" (techpack/camera-legacy, techpack/camera-legacy-n and
 * drivers/media/platform/msm/camera_v2), so every symbol shared between them
 * is defined exactly once in drivers/soc/oppo/device_info.c and referenced
 * through these declarations, otherwise the link would fail with
 * "multiple definition of ...".
 *
 * Note: register_device_proc() stores the pointers it is given without
 * copying them, so the sensor name strings handed to it are kzalloc()'ed by
 * the caller and deliberately never freed.
 */
struct int_string_pair {
	uint16_t value;
	char string[20];
};

/*
 * The arrays must be declared with their real dimension, not as an
 * incomplete type: the camera drivers call ARRAY_SIZE() on them, which does
 * not work on an "extern struct foo bar[]" declaration.
 */
#define CAM_MODULE_INFO_COUNT	6
#define CAM_SENSOR_INFO_COUNT	10

extern struct int_string_pair cam_module_info[CAM_MODULE_INFO_COUNT];
extern struct int_string_pair cam_sensor_info[CAM_SENSOR_INFO_COUNT];
extern int size_of_module_list;
extern int size_of_sensor_list;
extern uint8_t deviceInfo_register_value; /* bit 1: front, bit 0: rear */
extern uint16_t rear_module;
extern uint16_t rear_sensor;
extern uint16_t front_module;
extern uint16_t front_sensor;
#endif /* CONFIG_MACH_OPPO */

#endif /* _DEVICE_INFO_H */
