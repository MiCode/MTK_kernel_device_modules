/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __HQ_PRINTK__
#define __HQ_PRINTK__

#include <linux/printk.h>

#ifdef KLOG_MODNAME
#undef KLOG_MODNAME
#define KLOG_MODNAME ""
#endif

#define TAG                     "[HQ_CHG_DEFAULT]" // [VENDOR_MODULE_SUBMODULE]
#define hq_err(fmt, ...)        pr_err(TAG "[%s]:" fmt, __func__, ##__VA_ARGS__)
#define hq_warn(fmt, ...)       pr_warn(TAG "[%s]:" fmt, __func__, ##__VA_ARGS__)
#define hq_notice(fmt, ...)     pr_notice(TAG "[%s]:" fmt, __func__, ##__VA_ARGS__)
#define hq_info(fmt, ...)       pr_info(TAG "[%s]:" fmt, __func__, ##__VA_ARGS__)
#define hq_debug(fmt, ...)      pr_debug(TAG "[%s]:" fmt, __func__, ##__VA_ARGS__)

#endif
