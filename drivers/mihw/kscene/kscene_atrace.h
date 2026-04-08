#ifndef KSCENE_ATRACE_H
#define KSCENE_ATRACE_H
#include <linux/string.h>
#include <linux/kernel.h>

#define TRACE_BUFFER_LEN  128

__attribute__((unused))
static noinline int kscene_trace_mark_u64(const char* name, u64 val)
{
	char buf[TRACE_BUFFER_LEN] = {0};
    int pid = current->pid;

	snprintf(buf, TRACE_BUFFER_LEN, "B|%d|KSCENE: %s = %llu\n", pid, name, val);
	trace_puts(buf);

    memset(buf, 0, TRACE_BUFFER_LEN);
	snprintf(buf, TRACE_BUFFER_LEN, "E|%d\n", pid);
	trace_puts(buf);
	return 0;
}

#endif
