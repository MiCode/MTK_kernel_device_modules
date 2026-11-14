// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include "include/game.h"
#include <linux/string.h>
#include <linux/compiler.h>
#include <linux/kernel.h>
#include <linux/sched/clock.h>
#ifndef CREATE_TRACE_POINTS
#define CREATE_TRACE_POINTS
#endif
#include "game_trace_event.h"
#include "game.h"

void loom_main_trace(const char *fmt, ...)
{
	char log[256];
	va_list args;
	int len;

	if (!trace_loom_main_trace_enabled())
		return;

	va_start(args, fmt);
	len = vsnprintf(log, sizeof(log), fmt, args);

	if (unlikely(len == 256))
		log[255] = '\0';
	va_end(args);
	trace_loom_main_trace(log);
}

void game_main_trace(const char *fmt, ...)
{
	char log[256];
	va_list args;
	int len;

	if (!trace_game_main_trace_enabled())
		return;

	va_start(args, fmt);
	len = vsnprintf(log, sizeof(log), fmt, args);

	if (unlikely(len == 256))
		log[255] = '\0';
	va_end(args);
	trace_game_main_trace(log);
}

static void __game_systrace_print(int type, char *buf)
{
	switch (type) {
	case GAME_DEBUG_MANDATORY:
		trace_game_main_trace(buf);
		break;
	}
}

static int game_systrace_enabled(int type)
{
	int ret = 1;

	switch (type) {
	case GAME_DEBUG_MANDATORY:
		if (!trace_game_main_trace_enabled())
			ret = 0;
		break;
	default:
		break;
	}
	return ret;
}

void game_systrace_c(int type, pid_t pid, unsigned long long bufID,
	int val, const char *fmt, ...)
{
	char log[256];
	va_list args;
	int len;
	char buf2[256];

	if (!game_systrace_enabled(type))
		return;

	memset(log, ' ', sizeof(log));
	va_start(args, fmt);
	len = vsnprintf(log, sizeof(log), fmt, args);
	va_end(args);

	if (unlikely(len < 0))
		return;
	else if (unlikely(len == 256))
		log[255] = '\0';

	if (!bufID) {
		len = snprintf(buf2, sizeof(buf2), "C|%d|%s|%d\n", pid, log, val);
	} else {
		len = snprintf(buf2, sizeof(buf2), "C|%d|%s|%d|0x%llx\n",
			pid, log, val, bufID);
	}
	if (unlikely(len < 0))
		return;
	else if (unlikely(len == 256))
		buf2[255] = '\0';

	__game_systrace_print(type, buf2);
}
EXPORT_SYMBOL(game_systrace_c);

void game_systrace_b(int type, pid_t tgid, const char *fmt, ...)
{
	char log[256];
	va_list args;
	int len;
	char buf2[256];

	if (!game_systrace_enabled(type))
		return;

	memset(log, ' ', sizeof(log));
	va_start(args, fmt);
	len = vsnprintf(log, sizeof(log), fmt, args);
	va_end(args);

	if (unlikely(len < 0))
		return;
	else if (unlikely(len == 256))
		log[255] = '\0';

	len = snprintf(buf2, sizeof(buf2), "B|%d|%s\n", tgid, log);

	if (unlikely(len < 0))
		return;
	else if (unlikely(len == 256))
		buf2[255] = '\0';

	__game_systrace_print(type, buf2);
}
EXPORT_SYMBOL(game_systrace_b);

void game_systrace_e(int type)
{
	char buf2[256];
	int len;

	if (!game_systrace_enabled(type))
		return;

	len = snprintf(buf2, sizeof(buf2), "E\n");

	if (unlikely(len < 0))
		return;
	else if (unlikely(len == 256))
		buf2[255] = '\0';

	__game_systrace_print(type, buf2);
}
EXPORT_SYMBOL(game_systrace_e);

void game_print_trace(const char *fmt, ...)
{
	char log[256];
	va_list args;
	int len;

	if (!trace_game_main_trace_enabled())
		return;

	va_start(args, fmt);
	len = vsnprintf(log, sizeof(log), fmt, args);

	if (unlikely(len == 256))
		log[255] = '\0';
	va_end(args);
	trace_game_main_trace(log);
}
EXPORT_SYMBOL(game_print_trace);

unsigned long long game_get_time(void)
{
	unsigned long long temp;

	preempt_disable();
	temp = cpu_clock(smp_processor_id());
	preempt_enable();

	return temp;
}

