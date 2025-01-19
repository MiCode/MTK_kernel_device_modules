/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2023 MediaTek Inc.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM touch

#if !defined(_TRACE_TOUCH_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_TOUCH_H

#include <linux/tracepoint.h>
#include <linux/ktime.h>

TRACE_EVENT(touch_event,
    TP_PROTO(int64_t time1, int64_t time2, int64_t time3),
    TP_ARGS(time1, time2, time3),
    TP_STRUCT__entry(
        __field(int64_t, time1)
        __field(int64_t, time2)
        __field(int64_t, time3)
    ),
    TP_fast_assign(
        __entry->time1 = time1;
        __entry->time2 = time2;
        __entry->time3 = time3;
    ),
    TP_printk("touch report irq time=%lld ns, report time=%lld ns, diff time=%lld ns",
              __entry->time1,
              __entry->time2,
              __entry->time3)
);

#endif /* _TRACE_TOUCH_H */

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE touch_trace
#include <trace/define_trace.h>