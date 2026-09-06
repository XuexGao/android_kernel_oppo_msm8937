#ifndef __KSU_H_KLOG
#define __KSU_H_KLOG

#include <linux/printk.h>

#ifdef pr_fmt
#undef pr_fmt
#define pr_fmt(fmt) "KernelSU: " fmt
#endif

/*
 * CONFIG_KSU_QUIET_LOG: keep the whole feature set out of the kernel log.
 *
 * This file is force-included into every object of the feature set (see
 * Makefile: ccflags-y += -include ...), so folding the pr_*() family into
 * no_printk() here covers every call site, including the ones in the files
 * that patch core kernel code.
 *
 * no_printk() still runs the arguments through printk(), so format strings
 * keep being type checked and -Wformat-invalid stays live; nothing reaches
 * the ring buffer. Without this, a plain "adb shell dmesg" (or anything that
 * can read pstore/console) prints the feature name and one line per hooked
 * syscall, which is the cheapest way to tell this kernel from a stock build.
 *
 * Turn the option off when debugging the device over serial.
 */
#ifdef CONFIG_KSU_QUIET_LOG

#undef pr_emerg
#undef pr_alert
#undef pr_crit
#undef pr_err
#undef pr_warning
#undef pr_warn
#undef pr_notice
#undef pr_info
#undef pr_cont
#undef pr_emerg_once
#undef pr_alert_once
#undef pr_crit_once
#undef pr_err_once
#undef pr_warn_once
#undef pr_notice_once
#undef pr_info_once

#define pr_emerg(fmt, ...)	no_printk(KERN_EMERG pr_fmt(fmt), ##__VA_ARGS__)
#define pr_alert(fmt, ...)	no_printk(KERN_ALERT pr_fmt(fmt), ##__VA_ARGS__)
#define pr_crit(fmt, ...)	no_printk(KERN_CRIT pr_fmt(fmt), ##__VA_ARGS__)
#define pr_err(fmt, ...)	no_printk(KERN_ERR pr_fmt(fmt), ##__VA_ARGS__)
#define pr_warning(fmt, ...)	no_printk(KERN_WARNING pr_fmt(fmt), ##__VA_ARGS__)
#define pr_warn			pr_warning
#define pr_notice(fmt, ...)	no_printk(KERN_NOTICE pr_fmt(fmt), ##__VA_ARGS__)
#define pr_info(fmt, ...)	no_printk(KERN_INFO pr_fmt(fmt), ##__VA_ARGS__)
/* pr_cont() does not take pr_fmt(), it continues the previous line. */
#define pr_cont(fmt, ...)	no_printk(KERN_CONT fmt, ##__VA_ARGS__)

#define pr_emerg_once(fmt, ...)	no_printk(KERN_EMERG pr_fmt(fmt), ##__VA_ARGS__)
#define pr_alert_once(fmt, ...)	no_printk(KERN_ALERT pr_fmt(fmt), ##__VA_ARGS__)
#define pr_crit_once(fmt, ...)	no_printk(KERN_CRIT pr_fmt(fmt), ##__VA_ARGS__)
#define pr_err_once(fmt, ...)	no_printk(KERN_ERR pr_fmt(fmt), ##__VA_ARGS__)
#define pr_warn_once(fmt, ...)	no_printk(KERN_WARNING pr_fmt(fmt), ##__VA_ARGS__)
#define pr_notice_once(fmt, ...) no_printk(KERN_NOTICE pr_fmt(fmt), ##__VA_ARGS__)
#define pr_info_once(fmt, ...)	no_printk(KERN_INFO pr_fmt(fmt), ##__VA_ARGS__)

#endif /* CONFIG_KSU_QUIET_LOG */

#endif
