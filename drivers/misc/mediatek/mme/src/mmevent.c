// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/miscdevice.h>
#include <linux/file.h>

#include <linux/atomic.h>

#include <linux/vmalloc.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/debugfs.h>
#include <linux/interrupt.h>
#include <linux/rtc.h>

#include <linux/ftrace.h>

#include "mmevent.h"

#define MIN(x, y)   ((x) <= (y) ? (x) : (y))

static int bmme_init_buffer;

static struct mme_header_t mme_header = {
	.module_number = MME_MODULE_MAX,
	.module_info_size = sizeof(struct mme_module_t),
	.pid_info_size = sizeof(struct pid_info_t),
	.unit_size = sizeof(struct mme_unit_t),
};

static char *p_mme_invalid_addr = "mme-error-invalid-addr";

bool mme_debug_on;

unsigned int g_flag_shifts[MME_DATA_MAX] = {0, 3, 6, 9, 12, 15, 18, 21, 24, 27, 30, 33, 36, 39, 42};
EXPORT_SYMBOL(g_flag_shifts);

unsigned long long mmevent_log(unsigned int length, unsigned long long flag,
								unsigned int module, unsigned int type,
								unsigned int log_level)
{
	unsigned int unit_size = _MME_UNIT_NUM(length);
	unsigned long flags = 0;
	bool condition = ((module < MME_MODULE_MAX) &
					(type < MME_BUFFER_INDEX_MAX) &
					(unit_size < MME_MAX_UNIT_NUM)) &&
					(g_ring_buffer_units[module][type] > 0) &&
					mme_globals[module].start;
	if (condition) {
		struct mme_unit_t *p_ring_buffer = p_mme_ring_buffer[module][type];
		unsigned long long index = (atomic_add_return(unit_size,
							(atomic_t *)&(mme_globals[module].write_pointer[type])) -
							unit_size);
		if ((mme_globals[module].write_pointer[type]) > g_ring_buffer_units[module][type]) {
			if (index >= (mme_globals[module].buffer_units[type] - unit_size))
				index = 0;

			spin_lock_irqsave(&g_ring_buffer_spinlock[module][type], flags);
			if ((mme_globals[module].write_pointer[type]) > g_ring_buffer_units[module][type])
				atomic_set((atomic_t *)&(mme_globals[module].write_pointer[type]), 0);

			spin_unlock_irqrestore(&g_ring_buffer_spinlock[module][type], flags);
		}
		p_ring_buffer[index].data = sched_clock();
		p_ring_buffer[index + 1].data = (flag | ((unsigned long long)unit_size<<FLAG_UNIT_NUM_OFFSET) |
								FLAG_HEADER_DATA |
								((unsigned long long)log_level<<FLAG_LOG_LEVEL_OFFSET) |
								((unsigned long long)module<<FLAG_MODULE_TOKEN_OFFSET));
		return (unsigned long long)(&(p_ring_buffer[index + 2]));
	}

	if (unit_size >= MME_MAX_UNIT_NUM)
		MMEERR("start:%d,unit_size:%d", mme_globals[module].start, unit_size);
	return 0;
}
EXPORT_SYMBOL(mmevent_log);


// ------------------------------ buffer init ---------------------------------------------------

unsigned int g_ring_buffer_units[MME_MODULE_MAX][MME_BUFFER_INDEX_MAX] = {0};
EXPORT_SYMBOL(g_ring_buffer_units);

struct mme_unit_t *p_mme_ring_buffer[MME_MODULE_MAX][MME_BUFFER_INDEX_MAX] = {0};
EXPORT_SYMBOL(p_mme_ring_buffer);

spinlock_t g_ring_buffer_spinlock[MME_MODULE_MAX][MME_BUFFER_INDEX_MAX] ={0};
EXPORT_SYMBOL(g_ring_buffer_spinlock);

struct mme_module_t mme_globals[MME_MODULE_MAX] = {0};
EXPORT_SYMBOL(mme_globals);

bool mme_register_buffer(unsigned int module, char *module_name, unsigned int type,
						unsigned int buffer_size)
{
	DEFINE_SPINLOCK(t_spinlock);

	if (module >= MME_MODULE_MAX || type >= MME_BUFFER_INDEX_MAX || buffer_size > 15*1024*1024) {
		MMEERR("register fail, module:%d, type:%d, buffer_size:%d",
				module, type, buffer_size);
		return false;
	}

	MMEMSG("module:%d,module_name:%s,type:%d,buffer_size:%d",
			module, module_name, type, buffer_size);
	mme_globals[module].module = module;
	memset(mme_globals[module].module_name, 0, MME_MODULE_NAME_LEN);
	if (module_name)
		memcpy(mme_globals[module].module_name, module_name, strlen(module_name));
	mme_globals[module].write_pointer[type] = 0;
	mme_globals[module].buffer_bytes[type] = ((buffer_size + (PAGE_SIZE - 1)) &
											(~(PAGE_SIZE - 1)));
	mme_globals[module].buffer_units[type] = mme_globals[module].buffer_bytes[type] /
											MME_UNIT_SIZE;
	g_ring_buffer_units[module][type] = mme_globals[module].buffer_units[type] -
										RESERVE_BUFFER_UNITS;

	if(!p_mme_ring_buffer[module][type]) {
		p_mme_ring_buffer[module][type] =
#if IS_ENABLED(CONFIG_MTK_USE_RESERVED_EXT_MEM)
			(struct mme_unit_t *)
			extmem_malloc_page_align(
				mme_globals[module].buffer_bytes[type]);
#else
			vmalloc(mme_globals[module].buffer_bytes[type]);
#endif
	}

	if (!p_mme_ring_buffer[module][type]) {
		MMEMSG("Failed to allocate memory for ring buffer\n");
		return false;
	}
	memset((void *)(p_mme_ring_buffer[module][type]), 0, mme_globals[module].buffer_bytes[type]);
	g_ring_buffer_spinlock[module][type] = t_spinlock;
	mme_globals[module].enable = 1;
	bmme_init_buffer = 1;

	return true;
}
EXPORT_SYMBOL(mme_register_buffer);

void mme_release_buffer(unsigned int module, unsigned int type)
{

	if (p_mme_ring_buffer[module][type]) {
		vfree(p_mme_ring_buffer[module][type]);
		p_mme_ring_buffer[module][type] = 0;
	}

	mme_globals[module].write_pointer[type] = 0;
	mme_globals[module].buffer_bytes[type] = 0;
	mme_globals[module].buffer_units[type] = 0;
	g_ring_buffer_units[module][type] = 0;
}
EXPORT_SYMBOL(mme_release_buffer);

static void mme_release_all_buffer(void)
{
	unsigned int module, type;

	for (module=0; module<MME_MODULE_MAX; module++) {
		for (type=0; type<MME_BUFFER_INDEX_MAX; type++) {
			if (p_mme_ring_buffer[module][type]) {
				vfree(p_mme_ring_buffer[module][type]);
				p_mme_ring_buffer[module][type] = 0;
			}
		}
	}
}

static void mme_log_pause(void)
{
	unsigned int module;

	for (module=0; module<MME_MODULE_MAX; module++) {
		MMEMSG("%s before pause, module:%d,start:%d",
				__func__, module, mme_globals[module].start);
		mme_globals[module].start = 0;
	}
}

static void mme_log_start(void)
{
	unsigned int module;

	for (module=0; module<MME_MODULE_MAX; module++)
		mme_globals[module].start = 1;
}

static void mme_scale_buffer(unsigned int module, unsigned int scale_value)
{
	unsigned int type;
	char *module_name = mme_globals[module].module_name;

	MMEMSG("module:%d, scale_value:%d", module, scale_value);
	mme_log_pause();
	for (type = 0; type < MME_BUFFER_INDEX_MAX; type++) {
		if (mme_globals[module].buffer_bytes[type] != 0) {
			unsigned int new_buffer_size = mme_globals[module].buffer_bytes[type] *
											scale_value;

			mme_release_buffer(module, type);
			mme_register_buffer(module, module_name, type, new_buffer_size);
		}
	}
	mme_log_start();
}

// -------------------------------------- Register Dump callback -----------------------------------

static mme_dump_callback dump_callback_table[MME_MODULE_MAX] = {0};

void mme_register_dump_callback(unsigned int module, mme_dump_callback func)
{
	MMEMSG("module:%d", module);
	if (module < MME_MODULE_MAX)
		dump_callback_table[module] = func;
}
EXPORT_SYMBOL(mme_register_dump_callback);
// ---------------------------------------- Dump section -------------------------------------------

#define MME_DUMP_BLOCK_SIZE (1024*4)
#define MME_MODULE_DUMP_SIZE (1024*4)
#define STRING_BUFFER_LEN 1024
#define INVALIDE_EVENT -1
#define SUCCESS 1

static unsigned char mme_dump_block[MME_DUMP_BLOCK_SIZE];
static unsigned int s_str_buffer_dump_pointer;
static struct pid_info_t *p_pid_buffer;
static char *p_dump_buffer[MME_MODULE_MAX] = {0};

static unsigned int mme_fill_dump_block(void *p_src, void *p_dst,
	unsigned int *p_src_pos, unsigned int *p_dst_pos,
	unsigned int src_size, unsigned int dst_size)
{
	unsigned int src_left = src_size - *p_src_pos;
	unsigned int dst_left = dst_size - *p_dst_pos;

	if ((src_left == 0) || (dst_left == 0))
		return 0;

	if (src_left < dst_left) {
		memcpy(((unsigned char *)p_dst) + *p_dst_pos,
			((unsigned char *)p_src) + *p_src_pos, src_left);
		*p_src_pos += src_left;
		*p_dst_pos += src_left;
		return src_left;
	}

	memcpy(((unsigned char *)p_dst) + *p_dst_pos,
		((unsigned char *)p_src) + *p_src_pos, dst_left);
	*p_src_pos += dst_left;
	*p_dst_pos += dst_left;
	return dst_left;
}

static bool check_log_flag(unsigned long long flag, unsigned int *p_code_region_num)
{
	unsigned int unit_size = (flag & FLAG_UNIT_NUM_MASK) >> FLAG_UNIT_NUM_OFFSET;
	unsigned int module = (flag & FLAG_MODULE_TOKEN_MASK) >> FLAG_MODULE_TOKEN_OFFSET;
	unsigned int log_level = (flag & FLAG_LOG_LEVEL_MASK) >> FLAG_LOG_LEVEL_OFFSET;
	unsigned int data_size = sizeof(char *) + MME_PID_SIZE;
	unsigned int data_unit_size = 0;
	unsigned int code_region_num = 0;
	unsigned int stack_region_num = 0;
	unsigned int i = 0;

	if((flag & FLAG_HEADER_DATA_MASK)!=FLAG_HEADER_DATA) {
		MMEERR("invalid flag header, flag:%lld", flag);
		return false;
	}

	if (module >= MME_MODULE_MAX) {
		MMEERR("invalid flag module token, flag:%lld", flag);
		return false;
	}

	if (unit_size < (MME_HEADER_UNIT_SIZE +
					DIV_ROUND_UP((MME_FMT_SIZE + MME_PID_SIZE), MME_UNIT_SIZE))) {
		MMEERR("invalid unit_size, unit_size:%d", unit_size);
		return false;
	}

	if (log_level >= LOG_LEVEL_MAX) {
		MMEERR("invalid flag log level, flag:%lld", flag);
		return false;
	}

	for (i=0; i< MME_DATA_MAX; i++) {
		unsigned int shift = g_flag_shifts[i];
		unsigned int type = (flag >> shift) & 0x7;

		if (type == DATA_FLAG_INVALID)
			break;

		if (type == DATA_FLAG_RESERVED) {
			MMEERR("Invalid flag type: DATA_FLAG_RESERVED");
			return false;
		}

		if (type == DATA_FLAG_CODE_REGION_STRING)
			code_region_num++;

		if (type == DATA_FLAG_STACK_REGION_STRING)
			stack_region_num++;

		data_size += data_size_table[type];
	}

	data_unit_size = _MME_UNIT_NUM(data_size);

	if ((stack_region_num==0 && unit_size != data_unit_size) ||
		unit_size < data_unit_size) {
		MMEERR("invalid unit_size, unit_size:%d, data_unit_size:%d, stack_region_num:%d",
				unit_size, data_unit_size, stack_region_num);
		return false;
	}

	*p_code_region_num = code_region_num;
	return true;
}

static bool is_valid_addr(char *p)
{
	if (p == NULL)
		return false;

	if ((sizeof(p) > sizeof(int)) && ((unsigned long long)p>>60) != 0xF)
		return false;

	return true;
}

static bool is_valid_index(unsigned int index, struct mme_unit_t *p_ring_buffer,
							unsigned int buffer_units)
{
	unsigned int i, sum = 0;

	if (index >= (buffer_units - MME_HEADER_UNIT_SIZE))
		return false;

	if (p_ring_buffer[index].data == 0 && p_ring_buffer[index+1].data == 0) {
		for (i=index+2; i<MIN(buffer_units, index+20); i++)
			sum += p_ring_buffer[i].data;

		return (sum != 0);
	}
	return true;
}

static int get_string_buffer(unsigned int index, char *p_buffer, unsigned int *p_buf_pos,
							unsigned int *p_mme_unit_size, struct mme_unit_t *p_ring_buffer,
							unsigned int buffer_units)
{
	unsigned int data_size = 0, unit_size = 0, src_pos = 0;
	unsigned int code_region_num = 0, i = 0;
	unsigned long long flag = 0, time=0, p = 0;
	char *p_str;

	*p_buf_pos = 0;

	if (!(is_valid_index(index, p_ring_buffer, buffer_units))) {
		*p_mme_unit_size = buffer_units - index;
		MMEINFO("invalid index, index:%d,unit_size:%d", index, unit_size);
		return INVALIDE_EVENT;
	}

	time = p_ring_buffer[index].data;
	flag = p_ring_buffer[index + 1].data;

	unit_size = (flag & FLAG_UNIT_NUM_MASK) >> FLAG_UNIT_NUM_OFFSET;
	MMEINFO("dump string index:%d, flag:%lld,unit_size:%d, buffer_units:0x%x",
			index, flag, unit_size, buffer_units);

	if (time==0 ||
		!check_log_flag(flag, &code_region_num) ||
		(index + unit_size) >= buffer_units) {
		MMEINFO("invalid event, index:%d,flag:%lld,time:%lld,unit_size:%d",
				index, flag, time, unit_size);
		*p_mme_unit_size = 1;
		return INVALIDE_EVENT;
	}

	p = (unsigned long long)&(p_ring_buffer[index + MME_HEADER_UNIT_SIZE]);
	p +=  MME_PID_SIZE; // PID
	data_size += MME_PID_SIZE;
	// dump format
	p_str = *((char **)p);
	if (!is_valid_addr(p_str)) {
		*p_mme_unit_size = 2;
		MMEERR("invalid format p_str, index:%d, p_str:%p", index, p_str);
		return INVALIDE_EVENT;
	}
	MMEINFO("format addr:%p, format:%s, len:%zu", p_str, p_str, strlen(p_str));
	mme_fill_dump_block(p_str, p_buffer, &src_pos, p_buf_pos, strlen(p_str), STRING_BUFFER_LEN);
	*p_buf_pos += 1; // string end with 0
	data_size += sizeof(char *);
	p += sizeof(char *);

	if (code_region_num == 0) {
		*p_mme_unit_size = unit_size;
		return SUCCESS;
	}

	// dump code region string data
	for (i=0; i< MME_DATA_MAX; i++) {
		unsigned int shift = g_flag_shifts[i];
		unsigned int type = (flag >> shift) & 0x7;
		int type_size = data_size_table[type];

		if (type == DATA_FLAG_INVALID)
			break;

		if (type == DATA_FLAG_CODE_REGION_STRING) {
			p_str = *((char **)p);
			src_pos = 0;
			if (is_valid_addr(p_str)) {
				MMEINFO("str data addr:%p, str data:%s, len:%zu",
						p_str, p_str, strlen(p_str));
				mme_fill_dump_block(p_str, p_buffer, &src_pos, p_buf_pos, strlen(p_str),
									STRING_BUFFER_LEN);
			} else {
				MMEERR("invalid code region addr, index:%d, p_str:%p, i:%d", index, p_str, i);
				mme_fill_dump_block(p_mme_invalid_addr, p_buffer, &src_pos, p_buf_pos,
									strlen(p_mme_invalid_addr), STRING_BUFFER_LEN);
			}
			*p_buf_pos += 1; // string end with 0
		}

		if (type == DATA_FLAG_STACK_REGION_STRING) {
			p_str = (char *)p;
			if (is_valid_addr(p_str))
				type_size = _ALIGN_4_BYTES(strlen(p_str)+1);
			else
				MMEERR("invalid stack region addr, index:%d, p_str:%p, i:%d", index, p_str, i);
		}

		data_size += type_size;
		p += type_size;
	}

	*p_mme_unit_size = _MME_UNIT_NUM(data_size);
	MMEINFO("END, data_size = %d, unit size = %d, buf_pos:%d",
			data_size, *p_mme_unit_size, *p_buf_pos);
	return SUCCESS;
}

/**
 * @p_src: source buffer
 * @p_dst: destination buffer
 * @src_size: size of source buffer
 * @dst_size: size of destination buffer
 * @p_total_pos: file position
 * @p_region_base: total buffer size in front
 * @p_dst_pos: current position in destination buffer
 */
static void mme_dump_buffer(void *p_src, void *p_dst, unsigned int src_size, unsigned int dst_size,
					unsigned int *p_total_pos, unsigned int *p_region_base, unsigned int *p_dst_pos)
{
	unsigned int src_pos = 0;

	if (*p_total_pos < (*p_region_base + src_size)) {
		src_pos = *p_total_pos - *p_region_base;

		mme_fill_dump_block(p_src,
					mme_dump_block,
					&src_pos, p_dst_pos,
					src_size,
					dst_size);
		*p_total_pos = *p_region_base + src_size;
		s_str_buffer_dump_pointer = 0; // init as zero
	}
	*p_region_base += src_size;
}

static void get_pid_info(struct mme_unit_t *p_ring_buffer, unsigned int buffer_units,
						struct pid_info_t *p_pid_buffer, unsigned int *p_pid_count)
{
	unsigned long long flag = 0, time=0, p = 0;
	unsigned int code_region_num = 0, unit_size = 0, index=0;
	unsigned int pid, i;

	for (index = 0; index < buffer_units;) {
		if (!(is_valid_index(index, p_ring_buffer, buffer_units)))
			break;

		time = p_ring_buffer[index].data;
		flag = p_ring_buffer[index + 1].data;
		unit_size = (flag & FLAG_UNIT_NUM_MASK) >> FLAG_UNIT_NUM_OFFSET;

		if (time==0 || !check_log_flag(flag, &code_region_num) || (index + unit_size) >= buffer_units) {
			index += 1;
			continue;
		}

		p = (unsigned long long)&(p_ring_buffer[index + MME_HEADER_UNIT_SIZE]);
		pid = *((unsigned int *)p);
		if (pid >= 0) {
			for (i = 0; i < *p_pid_count; i++) {
				if (p_pid_buffer[i].pid == pid)
					break;
			}
			if (i == *p_pid_count) {
				struct task_struct *task;
				unsigned int cpu;

				p_pid_buffer[i].pid = pid;
				if (pid == 0) {
					for_each_possible_cpu(cpu) {
						sprintf(p_pid_buffer[i].name, "swapper/%d", cpu);
						break;
					}
				} else {
					task = find_task_by_vpid(pid);
					if (task != NULL)
						get_task_comm(p_pid_buffer[i].name, task);
				}

				MMEINFO("pid:%d, pid_name:%s", pid, p_pid_buffer[i].name);
				*p_pid_count += 1;
			}
		}
		index += unit_size;
	}
}

static void mme_init_process_info(struct pid_info_t *p_pid_buffer, unsigned int *p_pid_count)
{
	unsigned int module, type;

	for (module=0; module<MME_MODULE_MAX; module++) {
		if (mme_globals[module].enable) {
			for (type=0; type<MME_BUFFER_INDEX_MAX; type++) {
				get_pid_info(p_mme_ring_buffer[module][type], mme_globals[module].buffer_bytes[type],
							p_pid_buffer, p_pid_count);
			}
		}
	}
}

/**
 * @p_dst: destination buffer
 * @dst_size: size of destination buffer
 * @p_ring_buffer: pointer to ring buffer
 * @buffer_units: number of units in ring buffer
 * @p_total_index: total index of ring buffer in file
 * @p_base_index: total index in front
 * @p_dst_pos: current position in destination buffer
 */
static void mme_dump_buffer_string(void *p_dst, unsigned int dst_size,
					struct mme_unit_t *p_ring_buffer,
					unsigned int buffer_units, unsigned int *p_total_index,
					unsigned int *p_base_index, unsigned int *p_dst_pos)
{
	char buffer[STRING_BUFFER_LEN];
	unsigned int src_pos = 0;
	unsigned int buffer_size = 0;
	unsigned int mme_unit_size = 0;
	unsigned int index;

	if (*p_total_index < (*p_base_index + buffer_units)) {
		for(index = (*p_total_index - *p_base_index); index < buffer_units;) {
			mme_unit_size = 0;
			src_pos = 0;
			buffer_size = 0;
			memset(buffer, 0, STRING_BUFFER_LEN);

			get_string_buffer(index, buffer, &buffer_size, &mme_unit_size, p_ring_buffer, buffer_units);
			MMEINFO("index:%d,buffer_size:%d,mme_unit_size:%d,dst_pos:%d",
					index, buffer_size, mme_unit_size, *p_dst_pos);

			if (buffer_size > 0) {
				if (*p_dst_pos + buffer_size > dst_size) {
					MMEINFO("not enough space in dump block,index:%d,dst_pos:%d,buffer_size:%d",
							index, *p_dst_pos, buffer_size);
					*p_dst_pos = dst_size;
					return;
				}

				mme_fill_dump_block(buffer,
						p_dst,
						&src_pos, p_dst_pos,
						buffer_size,
						dst_size);

				if (*p_dst_pos == dst_size) {
					MMEINFO("block_pos == MME_DUMP_BLOCK_SIZE,index:%d", index);
					s_str_buffer_dump_pointer += mme_unit_size;
					return;
				}
			}
			index += mme_unit_size;
			s_str_buffer_dump_pointer += mme_unit_size;
		}
		*p_total_index = *p_base_index + buffer_units;
	}
	*p_base_index += buffer_units;
}

static void mme_init_android_time(void)
{
	struct rtc_time tm;
	struct timespec64 tv = { 0 };

	mme_header.kernel_time = sched_clock();
	ktime_get_real_ts64(&tv);
	rtc_time64_to_tm(tv.tv_sec, &tm);

	mme_header.android_time.year = tm.tm_year + 1900;
	mme_header.android_time.month = tm.tm_mon + 1;
	mme_header.android_time.day = tm.tm_mday;
	mme_header.android_time.hour = tm.tm_hour;
	mme_header.android_time.minute = tm.tm_min;
	mme_header.android_time.second = tm.tm_sec;
	mme_header.android_time.microsecond = (unsigned int)(tv.tv_nsec / 1000);
}

static void mme_get_dump_buffer(unsigned int start, unsigned long *p_addr,
	unsigned int *p_size)
{
	unsigned int total_pos = start;
	unsigned int total_index = 0;
	unsigned int base_index = 0;
	unsigned int block_pos = 0;
	unsigned int region_base = 0;
	unsigned int module, type;
	unsigned int pid_count = 0;

	*p_addr = (unsigned long)mme_dump_block;
	*p_size = MME_DUMP_BLOCK_SIZE;

	if (!bmme_init_buffer) {
		MMEERR("RingBuffer is not initialized");
		*p_size = 0;
		return;
	}

	memset(mme_dump_block, 0, MME_DUMP_BLOCK_SIZE);

	if (total_pos == 0) {
		unsigned int dump_size = 0;

		mme_init_android_time();
		p_pid_buffer = kcalloc(MME_PROCESS_SIZE, sizeof(struct pid_info_t), GFP_KERNEL);
		if (p_pid_buffer) {
			mme_init_process_info(p_pid_buffer, &pid_count);
			mme_header.pid_number = ALIGN(pid_count, 4);
		}

		for (module=0; module<MME_MODULE_MAX; module++) {
			if (mme_globals[module].enable) {
				if (dump_callback_table[module]) {
					p_dump_buffer[module] = kzalloc(MME_MODULE_DUMP_SIZE, GFP_KERNEL);
					if (p_dump_buffer[module]) {
						dump_size = 0;
						dump_callback_table[module](p_dump_buffer[module],
								MME_MODULE_DUMP_SIZE, &dump_size);
						mme_globals[module].module_dump_bytes = ALIGN(dump_size, 16);
						MMEMSG("module:%d,dump_size:%d", module, dump_size);
					}

				}
			}
		}
	}

	mme_dump_buffer(&mme_header, mme_dump_block, sizeof(mme_header),
					MME_DUMP_BLOCK_SIZE, &total_pos, &region_base, &block_pos);
	if (block_pos == MME_DUMP_BLOCK_SIZE)
		return;

	mme_dump_buffer(&mme_globals, mme_dump_block, sizeof(mme_globals),
					MME_DUMP_BLOCK_SIZE, &total_pos, &region_base, &block_pos);
	if (block_pos == MME_DUMP_BLOCK_SIZE)
		return;

	mme_dump_buffer(p_pid_buffer, mme_dump_block, mme_header.pid_number * sizeof(struct pid_info_t),
					MME_DUMP_BLOCK_SIZE, &total_pos, &region_base, &block_pos);
	if (block_pos == MME_DUMP_BLOCK_SIZE)
		return;

	kfree(p_pid_buffer);
	p_pid_buffer = 0;


	for (module=0; module<MME_MODULE_MAX; module++) {
		if (mme_globals[module].enable) {
			mme_dump_buffer(p_dump_buffer[module], mme_dump_block, mme_globals[module].module_dump_bytes,
							MME_DUMP_BLOCK_SIZE, &total_pos, &region_base, &block_pos);
			if (block_pos == MME_DUMP_BLOCK_SIZE)
				return;

			kfree(p_dump_buffer[module]);
			p_dump_buffer[module] = 0;

			for (type=0; type<MME_BUFFER_INDEX_MAX; type++) {
				MMEINFO("module=%d,type=%d,total_index:0x%x,region_base:0x%x,block_pos:0x%x",
						module, type, total_index, region_base, block_pos);
				mme_dump_buffer(p_mme_ring_buffer[module][type], mme_dump_block,
						mme_globals[module].buffer_bytes[type],
						MME_DUMP_BLOCK_SIZE, &total_pos, &region_base, &block_pos);
				if (block_pos == MME_DUMP_BLOCK_SIZE)
					return;
			}
		}
	}

	total_index = s_str_buffer_dump_pointer;
	MMEINFO("dump string, total_pos:%d, region_base:%d, total_index:0x%x", total_pos, region_base, total_index);

	for (module=0; module<MME_MODULE_MAX; module++) {
		if (mme_globals[module].enable) {
			for (type=0; type<MME_BUFFER_INDEX_MAX; type++) {
				MMEINFO("dump buffer string, module:%d, type:%d", module, type);
				mme_dump_buffer_string(mme_dump_block, MME_DUMP_BLOCK_SIZE,
										p_mme_ring_buffer[module][type],
										mme_globals[module].buffer_units[type],
										&total_index, &base_index, &block_pos);
				if (block_pos == MME_DUMP_BLOCK_SIZE)
					return;
			}
		}
	}
	*p_size = block_pos;
}

static ssize_t mmevent_dbgfs_buffer_read(struct file *file, char __user *buf,
	size_t size, loff_t *ppos)
{
	unsigned int copy_size = 0;
	unsigned int total_copy = 0;
	unsigned long addr;

	if (!bmme_init_buffer) {
		MMEERR("RingBuffer is not initialized");
		return -EFAULT;
	}

	if (*ppos == 0)
		mme_log_pause();

	MMEINFO("size=%ld ppos=%lld", (unsigned long)size, *ppos);
	while (size > 0) {
		mme_get_dump_buffer(*ppos, &addr, &copy_size);
		if (copy_size == 0) {
			mme_log_start();
			break;
		}
		if (size >= copy_size) {
			size -= copy_size;
		} else {
			copy_size = size;
			size = 0;
		}

		if (copy_to_user(buf + total_copy, (void *)addr, copy_size)) {
			MMEERR("fail to copytouser total_copy=%u", total_copy);
			break;
		}
		*ppos += copy_size;
		total_copy += copy_size;
	}

	return total_copy;
}

// ------------------------------- Driver Section ---------------------------------------------
/* Debug FS begin */
static struct dentry *g_p_debug_fs_dir;
static struct dentry *g_p_debug_fs_buffer;

static void process_dbg_cmd(char *cmd)
{
	unsigned long value;

	MMEMSG("cmd:%s", cmd);
	if (strncmp(cmd, "mme_debug_on:", 13) == 0) {
		char *p = (char *)cmd + 13;

		if (kstrtoul(p, 10, &value) == 0 && 0 != value)
			mme_debug_on = 1;
		else
			mme_debug_on = 0;
		MMEMSG("mme_log_on=%d\n", mme_debug_on);
	} else if (strncmp(cmd, "scale_ring_buffer:", 18) == 0) {
		char *p = (char *)cmd + 18;
		char *scale_str = NULL;
		unsigned long scale_value = 0;
		unsigned long module = 0;

		p += strspn(p, " ");
		scale_str = strchr(p, ' ');
		MMEMSG("p:%s, scale_str:%s", p, scale_str);
		if (scale_str) {
			*scale_str = '\0';
			if (kstrtoul(p, 10, &module) == 0 && module < MME_MODULE_MAX) {
				if (kstrtoul(scale_str + 1, 10, &scale_value) == 0 && scale_value < 100)
					mme_scale_buffer(module, scale_value);
			}
		}

		MMEMSG("scale_ring_buffer, module:%lu, scale_value:%lu", module, scale_value);
	} else {
		MMEMSG("invalid mme debug command: %s\n",
			cmd != NULL ? cmd : "(empty)");
	}
}

static long mmevent_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg)
{
	return 0;
}

static int mmevent_release(struct inode *inode, struct file *file)
{
	return 0;
}

static int mmevent_open(struct inode *inode, struct file *file)
{
	return 0;
}

static ssize_t mmevent_read(struct file *file, char __user *data, size_t len, loff_t *ppos)
{
	return 0;
}

static ssize_t mmevent_write(struct file *file, const char __user *data, size_t len, loff_t *ppos)
{
	ssize_t ret;
	size_t length = len;
	char cmd_buf[128];

	if (length > 127)
		length = 127;
	ret = length;

	if (copy_from_user(&cmd_buf, data, length))
		return -EFAULT;

	cmd_buf[length] = 0;
	process_dbg_cmd(cmd_buf);

	return ret;
}

static int mmevent_mmap(struct file *file, struct vm_area_struct *vma)
{
	return 0;

}

const struct file_operations mmevent_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = mmevent_ioctl,
	.open = mmevent_open,
	.release = mmevent_release,
	.read = mmevent_read,
	.write = mmevent_write,
	.mmap = mmevent_mmap,
};

static const struct file_operations mmevent_dbgfs_buffer_fops = {
	.read = mmevent_dbgfs_buffer_read,
	.llseek = generic_file_llseek,
};

struct miscdevice *mme_dev;

static int mmevent_probe(void)
{
	int ret = 0;

	g_p_debug_fs_dir = debugfs_create_dir("mme", NULL);
	if (g_p_debug_fs_dir) {
		g_p_debug_fs_buffer =
			debugfs_create_file("buffer", 0400,
								g_p_debug_fs_dir, NULL,
								&mmevent_dbgfs_buffer_fops);
	}

	mme_dev = kzalloc(sizeof(*mme_dev), GFP_KERNEL);
	if (!mme_dev)
		return -ENOMEM;

	mme_dev->minor = MISC_DYNAMIC_MINOR;
	mme_dev->name = "mme";
	mme_dev->fops = &mmevent_fops;
	mme_dev->parent = NULL;
	ret = misc_register(mme_dev);
	if (ret) {
		MMEERR("mme: failed to register misc device.\n");
		kfree(mme_dev);
		mme_dev = NULL;
		return ret;
	}
	mme_log_start();
	return 0;
}

static int __init mme_init(void)
{
	mmevent_probe();
	return 0;
}

static void __exit mme_exit(void)
{
	mme_release_all_buffer();
}

/* Driver specific end */
module_init(mme_init);
module_exit(mme_exit);
MODULE_AUTHOR("Zhongchao Xia <zhongchao.xia@mediatek.com>");
MODULE_DESCRIPTION("MME Driver");
MODULE_LICENSE("GPL");
