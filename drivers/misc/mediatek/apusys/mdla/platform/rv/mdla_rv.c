// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/of_device.h>
#include <linux/debugfs.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>

#include <common/mdla_power_ctrl.h>

#include <utilities/mdla_debug.h>
#include <utilities/mdla_util.h>
#include <utilities/mdla_ipi.h>

#include <platform/mdla_plat_api.h>

#include "mdla_rv.h"

struct mdla_plat_ip_ops {
	int (*dbgfs_usage)(struct seq_file *s, void *data);
	int (*dump_dbg_mem)(struct seq_file *s, void *data);
	void (*rv_configuration)(void);
	void (*alloc_dbg_mem)(void);
	void (*free_dbg_mem)(void);
};

/* remove after lk/atf bootup flow ready */
#define LK_BOOT_RDY 0

#define DBGFS_USAGE_NAME    "help"
#define DBGFS_MEM_NAME      "dbg_mem"

static struct mdla_dev *mdla_plat_devices;

static DEFINE_MUTEX(dbg_mem_mutex);

#define DEFINE_IPI_DBGFS_ATTRIBUTE(name, TYPE_0, TYPE_1, fmt)		\
static int name ## _set(void *data, u64 val)				\
{									\
	mdla_ipi_send(TYPE_0, TYPE_1, val);				\
	*(u64 *)data = val;						\
	return 0;							\
}									\
static int name ## _get(void *data, u64 *val)				\
{									\
	mdla_ipi_recv(TYPE_0, TYPE_1, val);				\
	*(u64 *)data = *val;						\
	return 0;							\
}									\
static int name ## _open(struct inode *i, struct file *f)		\
{									\
	__simple_attr_check_format(fmt, 0ull);				\
	return simple_attr_open(i, f, name ## _get, name ## _set, fmt);	\
}									\
static const struct file_operations name ## _fops = {			\
	.owner	 = THIS_MODULE,						\
	.open	 = name ## _open,					\
	.release = simple_attr_release,					\
	.read	 = debugfs_attr_read,					\
	.write	 = debugfs_attr_write,					\
}

DEFINE_IPI_DBGFS_ATTRIBUTE(ulog,           MDLA_IPI_ULOG,           0, "0x%llx\n");
DEFINE_IPI_DBGFS_ATTRIBUTE(timeout,        MDLA_IPI_TIMEOUT,        0, "%llu\n");
DEFINE_IPI_DBGFS_ATTRIBUTE(pwrtime,        MDLA_IPI_PWR_TIME,       0, "%llu\n");
DEFINE_IPI_DBGFS_ATTRIBUTE(cmd_check,      MDLA_IPI_CMD_CHECK,      0, "%llx\n");
DEFINE_IPI_DBGFS_ATTRIBUTE(pmu_trace,      MDLA_IPI_TRACE_ENABLE,   0, "%llx\n");
DEFINE_IPI_DBGFS_ATTRIBUTE(C1,             MDLA_IPI_PMU_COUNT,      1, "0x%llx\n");
DEFINE_IPI_DBGFS_ATTRIBUTE(C2,             MDLA_IPI_PMU_COUNT,      2, "0x%llx\n");
DEFINE_IPI_DBGFS_ATTRIBUTE(C3,             MDLA_IPI_PMU_COUNT,      3, "0x%llx\n");
DEFINE_IPI_DBGFS_ATTRIBUTE(C4,             MDLA_IPI_PMU_COUNT,      4, "0x%llx\n");
DEFINE_IPI_DBGFS_ATTRIBUTE(C5,             MDLA_IPI_PMU_COUNT,      5, "0x%llx\n");
DEFINE_IPI_DBGFS_ATTRIBUTE(C6,             MDLA_IPI_PMU_COUNT,      6, "0x%llx\n");
DEFINE_IPI_DBGFS_ATTRIBUTE(C7,             MDLA_IPI_PMU_COUNT,      7, "0x%llx\n");
DEFINE_IPI_DBGFS_ATTRIBUTE(C8,             MDLA_IPI_PMU_COUNT,      8, "0x%llx\n");
DEFINE_IPI_DBGFS_ATTRIBUTE(C9,             MDLA_IPI_PMU_COUNT,      9, "0x%llx\n");
DEFINE_IPI_DBGFS_ATTRIBUTE(C10,            MDLA_IPI_PMU_COUNT,     10, "0x%llx\n");
DEFINE_IPI_DBGFS_ATTRIBUTE(C11,            MDLA_IPI_PMU_COUNT,     11, "0x%llx\n");
DEFINE_IPI_DBGFS_ATTRIBUTE(C12,            MDLA_IPI_PMU_COUNT,     12, "0x%llx\n");
DEFINE_IPI_DBGFS_ATTRIBUTE(C13,            MDLA_IPI_PMU_COUNT,     13, "0x%llx\n");
DEFINE_IPI_DBGFS_ATTRIBUTE(C14,            MDLA_IPI_PMU_COUNT,     14, "0x%llx\n");
DEFINE_IPI_DBGFS_ATTRIBUTE(C15,            MDLA_IPI_PMU_COUNT,     15, "0x%llx\n");
DEFINE_IPI_DBGFS_ATTRIBUTE(preempt_times,  MDLA_IPI_PREEMPT_CNT,    0, "%llu\n");
DEFINE_IPI_DBGFS_ATTRIBUTE(force_pwr_on,   MDLA_IPI_FORCE_PWR_ON,   0, "%llu\n");
DEFINE_IPI_DBGFS_ATTRIBUTE(profiling,      MDLA_IPI_PROFILE_EN,     0, "%llu\n");
DEFINE_IPI_DBGFS_ATTRIBUTE(dump_cmdbuf_en, MDLA_IPI_DUMP_CMDBUF_EN, 0, "%llu\n");
DEFINE_IPI_DBGFS_ATTRIBUTE(info,           MDLA_IPI_INFO,           0, "%llu\n");
DEFINE_IPI_DBGFS_ATTRIBUTE(dbg_brk,        MDLA_IPI_HALT_STA,       0, "0x%llx\n");
DEFINE_IPI_DBGFS_ATTRIBUTE(dbg_options,    MDLA_IPI_DBG_OPTIONS,    0, "0x%llx\n");
DEFINE_IPI_DBGFS_ATTRIBUTE(flog,           MDLA_IPI_FW_LOG_LV,      0, "%llu\n");

struct mdla_dbgfs_ipi_file {
	int type0;
	int type1;
	u32 sup_mask;			/* 1'b << IP major version */
	umode_t mode;
	char *str;
	const struct file_operations *fops;
	u64 val;				/* update by ipi/debugfs */
};

static struct mdla_dbgfs_ipi_file ipi_dbgfs_file[] = {
	{MDLA_IPI_PWR_TIME,       0, 0x2C, 0660,  "poweroff_time",        &pwrtime_fops, 0},
	{MDLA_IPI_TIMEOUT,        0, 0x6C, 0660,        "timeout",        &timeout_fops, 0},
	{MDLA_IPI_ULOG,           0, 0x2C, 0660,           "ulog",           &ulog_fops, 0},
	{MDLA_IPI_CMD_CHECK,      0, 0x04, 0660,      "cmd_check",      &cmd_check_fops, 0},
	{MDLA_IPI_TRACE_ENABLE,   0, 0x2C, 0660,      "pmu_trace",      &pmu_trace_fops, 0},
	{MDLA_IPI_PMU_COUNT,      1, 0x04, 0660,             "c1",             &C1_fops, 0},
	{MDLA_IPI_PMU_COUNT,      2, 0x04, 0660,             "c2",             &C2_fops, 0},
	{MDLA_IPI_PMU_COUNT,      3, 0x04, 0660,             "c3",             &C3_fops, 0},
	{MDLA_IPI_PMU_COUNT,      4, 0x04, 0660,             "c4",             &C4_fops, 0},
	{MDLA_IPI_PMU_COUNT,      5, 0x04, 0660,             "c5",             &C5_fops, 0},
	{MDLA_IPI_PMU_COUNT,      6, 0x04, 0660,             "c6",             &C6_fops, 0},
	{MDLA_IPI_PMU_COUNT,      7, 0x04, 0660,             "c7",             &C7_fops, 0},
	{MDLA_IPI_PMU_COUNT,      8, 0x04, 0660,             "c8",             &C8_fops, 0},
	{MDLA_IPI_PMU_COUNT,      9, 0x04, 0660,             "c9",             &C9_fops, 0},
	{MDLA_IPI_PMU_COUNT,     10, 0x04, 0660,            "c10",            &C10_fops, 0},
	{MDLA_IPI_PMU_COUNT,     11, 0x04, 0660,            "c11",            &C11_fops, 0},
	{MDLA_IPI_PMU_COUNT,     12, 0x04, 0660,            "c12",            &C12_fops, 0},
	{MDLA_IPI_PMU_COUNT,     13, 0x04, 0660,            "c13",            &C13_fops, 0},
	{MDLA_IPI_PMU_COUNT,     14, 0x04, 0660,            "c14",            &C14_fops, 0},
	{MDLA_IPI_PMU_COUNT,     15, 0x04, 0660,            "c15",            &C15_fops, 0},
	{MDLA_IPI_PREEMPT_CNT,    0, 0x6C, 0660,  "preempt_times",  &preempt_times_fops, 0},
	{MDLA_IPI_FORCE_PWR_ON,   0, 0x2C, 0660,   "force_pwr_on",   &force_pwr_on_fops, 0},
	{MDLA_IPI_PROFILE_EN,     0, 0x68, 0660,      "profiling",      &profiling_fops, 0},
	{MDLA_IPI_DUMP_CMDBUF_EN, 0, 0x6C, 0660, "dump_cmdbuf_en", &dump_cmdbuf_en_fops, 0},
	{MDLA_IPI_INFO,           0, 0x6C, 0660,           "info",           &info_fops, 0},
	{MDLA_IPI_HALT_STA,       0, 0x28, 0660,        "dbg_brk",        &dbg_brk_fops, 0},
	{MDLA_IPI_DBG_OPTIONS,    0, 0x60, 0660,    "dbg_options",    &dbg_options_fops, 0},
	{MDLA_IPI_FW_LOG_LV,      0, 0x40, 0660,      "fw_log_lv",           &flog_fops, 0},
	{NF_MDLA_IPI_TYPE_0,      0, 0x00,    0,             NULL,                 NULL, 0}
};



static struct mdla_rv_mem boot_mem;
static struct mdla_rv_mem main_mem;

#define DEFAULT_DBG_SZ 0x1000
#define DEFAULT_RV_DBG_SZ 0x2000
static struct mdla_rv_mem dbg_mem;
static struct mdla_rv_mem backup_mem;
static struct mdla_rv_mem rv_dbg_mem;

/*****************************************************************************
 *                          Static Common Functions                          *
 *****************************************************************************/

static char *mdla_plat_get_ipi_str(int idx)
{
	u32 i;

	for (i = 0; ipi_dbgfs_file[i].str != NULL; i++) {
		if (ipi_dbgfs_file[i].type0 == idx)
			return ipi_dbgfs_file[i].str;
	}
	return "unknown";
}

static void mdla_plat_dummy_ip_ops(void)
{
}

static int mdla_plat_unknown_dbgfs_usage(struct seq_file *s, void *data)
{
	seq_puts(s, "\n---------- unknown usage ----------\n");
	return 0;
}

static int mdla_plat_alloc_mem(struct mdla_rv_mem *m, unsigned int size)
{
	struct device *dev;

	if (mdla_plat_devices && mdla_plat_devices[0].dev)
		dev = mdla_plat_devices[0].dev;
	else
		return -ENXIO;

	m->buf = dma_alloc_coherent(dev, size, &m->da, GFP_KERNEL);
	if (m->buf == NULL || m->da == 0) {
		dev_info(dev, "%s() dma_alloc_coherent fail\n", __func__);
		return -1;
	}

	m->size = size;
	memset(m->buf, 0, size);

	return 0;
}

static void mdla_plat_free_mem(struct mdla_rv_mem *m)
{
	struct device *dev;

	if (mdla_plat_devices && mdla_plat_devices[0].dev)
		dev = mdla_plat_devices[0].dev;
	else
		return;

	if (m->buf && m->da && m->size) {
		dma_free_coherent(dev, m->size, m->buf, m->da);
		m->buf  = NULL;
		m->size = 0;
		m->da   = 0;
	}
}

static void mdla_plat_alloc_up_dbg_mem(u32 size)
{
	mdla_plat_alloc_mem(&rv_dbg_mem, size);
}

static void mdla_plat_alloc_fw_backup_mem(u32 size)
{
	/* backup size * core num * preempt lv */
	mdla_plat_alloc_mem(&backup_mem, size);
}

static void mdla_plat_alloc_dbg_mem(u32 size)
{
	mdla_plat_alloc_mem(&dbg_mem, size);
}

static void mdla_plat_alloc_dbg_mem_by_rv(void)
{
	u64 size = 0;

	mdla_ipi_recv(MDLA_IPI_ADDR, MDLA_IPI_ADDR_DBG_DATA_SZ, &size);
	if (size)
		mdla_plat_alloc_mem(&dbg_mem, (u32)size);
}

static int mdla_rv_dbg_mem_show_nothing(struct seq_file *s, void *data)
{
	seq_puts(s, "No debug data!\n");

	return 0;
}

static int mdla_rv_dbg_mem_dump_binary(struct seq_file *s, void *data)
{
	struct mdla_rv_mem *m = &dbg_mem;

	if (!m->buf || !m->da || !m->size) {
		seq_puts(s, "No debug data!\n");
		return 0;
	}

	seq_write(s, m->buf, m->size);

	return 0;
}

static int mdla_rv_dbg_mem_show(struct seq_file *s, void *data)
{
	u32 i = 0, *buf;
	struct mdla_rv_mem *m = &dbg_mem;

	if (!m->buf || !m->da || !m->size) {
		seq_puts(s, "No debug data!\n");
		return 0;
	}

	buf = (u32 *)m->buf;

	for (i = 0; i < m->size / 4; i += 4) {
		seq_printf(s, "0x%08x: %08x %08x %08x %08x\n",
				4 * i,
				buf[i],
				buf[i + 1],
				buf[i + 2],
				buf[i + 3]);
	}

	return 0;
}

static int mdla_rv_dbg_mem_open(struct inode *inode, struct file *file)
{
	return single_open(file, mdla_rv_dbg_mem_show, inode->i_private);
}

static void mdla_plat_set_rv_backup_dbg_mem(void)
{
	if (boot_mem.da && main_mem.da) {
		mdla_verbose("%s(): send ipi for fw addr(0x%08x, 0x%08x)\n", __func__,
				(u32)boot_mem.da, (u32)main_mem.da);
		mdla_ipi_send(MDLA_IPI_ADDR, MDLA_IPI_ADDR_BOOT, (u64)boot_mem.da);
		mdla_ipi_send(MDLA_IPI_ADDR, MDLA_IPI_ADDR_BOOT_SZ, (u64)boot_mem.size);
		mdla_ipi_send(MDLA_IPI_ADDR, MDLA_IPI_ADDR_MAIN, (u64)main_mem.da);
		mdla_ipi_send(MDLA_IPI_ADDR, MDLA_IPI_ADDR_MAIN_SZ, (u64)main_mem.size);
	}

	if (backup_mem.da) {
		mdla_ipi_send(MDLA_IPI_ADDR, MDLA_IPI_ADDR_BACKUP_DATA, (u64)backup_mem.da);
		mdla_ipi_send(MDLA_IPI_ADDR, MDLA_IPI_ADDR_BACKUP_DATA_SZ, (u64)backup_mem.size);
	}

	if (rv_dbg_mem.da) {
		mdla_ipi_send(MDLA_IPI_ADDR, MDLA_IPI_ADDR_RV_DATA, (u64)rv_dbg_mem.da);
		mdla_ipi_send(MDLA_IPI_ADDR, MDLA_IPI_ADDR_RV_DATA_SZ, (u64)rv_dbg_mem.size);
	}

	if (dbg_mem.da) {
		mdla_ipi_send(MDLA_IPI_ADDR, MDLA_IPI_ADDR_DBG_DATA, (u64)dbg_mem.da);
		mdla_ipi_send(MDLA_IPI_ADDR, MDLA_IPI_ADDR_DBG_DATA_SZ, (u64)dbg_mem.size);
	}
}

static void mdla_plat_get_and_set_rv_dbg_mem(void)
{

	if (boot_mem.da && main_mem.da) {
		mdla_verbose("%s(): send ipi for fw addr(0x%08x, 0x%08x)\n", __func__,
				(u32)boot_mem.da, (u32)main_mem.da);
		mdla_ipi_send(MDLA_IPI_ADDR, MDLA_IPI_ADDR_BOOT, (u64)boot_mem.da);
		mdla_ipi_send(MDLA_IPI_ADDR, MDLA_IPI_ADDR_BOOT_SZ, (u64)boot_mem.size);
		mdla_ipi_send(MDLA_IPI_ADDR, MDLA_IPI_ADDR_MAIN, (u64)main_mem.da);
		mdla_ipi_send(MDLA_IPI_ADDR, MDLA_IPI_ADDR_MAIN_SZ, (u64)main_mem.size);
	}

	mdla_plat_alloc_dbg_mem_by_rv();

	if (dbg_mem.da) {
		mdla_ipi_send(MDLA_IPI_ADDR, MDLA_IPI_ADDR_DBG_DATA, (u64)dbg_mem.da);
		mdla_ipi_send(MDLA_IPI_ADDR, MDLA_IPI_ADDR_DBG_DATA_SZ, (u64)dbg_mem.size);
	}
}

static void mdla_plat_get_ip_ver_from_rv(void)
{
	u64 ver = 0;

	mdla_ipi_recv(MDLA_IPI_INFO, MDLA_IPI_INFO_HW_VER, &ver);
	if (get_major_num(ver) != 0)
		mdla_dbg_set_version((u32)ver);
}

static ssize_t mdla_rv_dbg_mem_write(struct file *flip,
		const char __user *buffer,
		size_t count, loff_t *f_pos)
{
	char *buf;
	u32 size;
	int ret;

	buf = kzalloc(count + 1, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	ret = copy_from_user(buf, buffer, count);
	if (ret)
		goto out;

	buf[count] = '\0';

	if (kstrtouint(buf, 10, &size) != 0) {
		count = -EINVAL;
		goto out;
	}

	if (size > 0x200000 || size < 0x10)
		goto out;

	mutex_lock(&dbg_mem_mutex);
	mdla_plat_free_mem(&dbg_mem);

	if (mdla_plat_alloc_mem(&dbg_mem, size) == 0) {
		mdla_ipi_send(MDLA_IPI_ADDR, MDLA_IPI_ADDR_DBG_DATA, (u64)dbg_mem.da);
		mdla_ipi_send(MDLA_IPI_ADDR, MDLA_IPI_ADDR_DBG_DATA_SZ, (u64)dbg_mem.size);
	}
	mutex_unlock(&dbg_mem_mutex);

out:
	kfree(buf);
	return count;
}

static const struct file_operations mdla_rv_dbg_mem_fops = {
	.open = mdla_rv_dbg_mem_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.write = mdla_rv_dbg_mem_write,
};

/*****************************************************************************
 *                          Static IP Functions                              *
 *****************************************************************************/
static void mdla_plat_v6_aee_handler(u32 type, u64 val)
{
	if (type == MDLA_IPI_MICROP_MSG_TIMEOUT) {
		mdla_aee_exception("MDLA", "MDLA timeout : reset core map = 0x%llx, features(%lld) = 0x%llx, 0x%llx,",
								val & 0xf,
								(val >> 4) & 0xf,
								(val >> 8) & 0xfff,
								(val >> 20) & 0xfff);
	} else if (type == MDLA_IPI_MICROP_MSG_DBG_CHECK_FAILED) {
		u32 idx;
		struct msg_type {
			u64 val;
			char *msg;
		} dbg_info[] = {
			{ 0x10000, "Command buffer is NULL" },
			{ 0x10001, "Command buffer number error" },
			{ 0x10002, "Command buffer size error" },
			{ 0x10003, "CE abort ack timeout" },
			{ 0x10004, "Power off in abort state" },
			{ 0x10005, "Pattern OP version mismatched" }
		};
		u32 num = sizeof(dbg_info) / sizeof(struct msg_type);

		for (idx = 0; idx < num; idx++) {
			if (dbg_info[idx].val == val) {
				mdla_aee_exception("MDLA", "MDLA check failed: %s", dbg_info[idx].msg);
				break;
			}
		}
		if (idx == num)
			mdla_aee_exception("MDLA", "MDLA check failed (%llu)", val);
	} else if (type == MDLA_IPI_MICROP_MSG_CMD_FAILED) {
		mdla_aee_exception("MDLA", "MDLA cmd failed (%llu)", val);
	} else if (type == MDLA_IPI_MICROP_MSG_NPUMMU_TF) {
		mdla_aee_exception("MDLA", "NPUMMU TF (core: %llu, thread: %llu)",
								(val >> 16) & 0xf,
								val & 0xf);
	} else if (type == MDLA_IPI_MICROP_MSG_DLA_ERROR) {
		mdla_aee_exception("DLA", "DLA Error");
	}
}

static int mdla_plat_v2_dbgfs_usage(struct seq_file *s, void *data)
{
	seq_puts(s, "\n---------- Command timeout setting ----------\n");
	seq_printf(s, "echo [ms(dec)] > /d/mdla/%s\n", ipi_dbgfs_file[MDLA_IPI_TIMEOUT].str);

	seq_puts(s, "\n-------- Set delay time of power off --------\n");
	seq_printf(s, "echo [ms(dec)] > /d/mdla/%s\n", ipi_dbgfs_file[MDLA_IPI_PWR_TIME].str);

	seq_puts(s, "\n------------- Set uP debug log mask -------------\n");
	seq_printf(s, "echo [mask(hex)] > /d/mdla/%s\n", mdla_plat_get_ipi_str(MDLA_IPI_ULOG));
	seq_printf(s, "\tMDLA_DBG_DRV         = 0x%x\n", 1U << V2_DBG_DRV);
	seq_printf(s, "\tMDLA_DBG_CMD         = 0x%x\n", 1U << V2_DBG_CMD);
	seq_printf(s, "\tMDLA_DBG_PMU         = 0x%x\n", 1U << V2_DBG_PMU);
	seq_printf(s, "\tMDLA_DBG_PERF        = 0x%x\n", 1U << V2_DBG_PERF);
	seq_printf(s, "\tMDLA_DBG_TIMEOUT     = 0x%x\n", 1U << V2_DBG_TIMEOUT);
	seq_printf(s, "\tMDLA_DBG_PWR         = 0x%x\n", 1U << V2_DBG_PWR);
	seq_printf(s, "\tMDLA_DBG_MEM         = 0x%x\n", 1U << V2_DBG_MEM);
	seq_printf(s, "\tMDLA_DBG_IPI         = 0x%x\n", 1U << V2_DBG_IPI);

	seq_puts(s, "\n--------------- power control ---------------\n");
	seq_printf(s, "echo [0|1] > /d/mdla/%s\n", mdla_plat_get_ipi_str(MDLA_IPI_FORCE_PWR_ON));
	seq_puts(s, "\t0: force power down\n");
	seq_puts(s, "\t1: force power up\n");

	seq_puts(s, "\n------------- show information ------------------\n");
	seq_printf(s, "echo [item] > /d/mdla/%s\n", mdla_plat_get_ipi_str(MDLA_IPI_INFO));
	seq_printf(s, "\t%d: show register value\n", MDLA_IPI_INFO_REG);
	seq_printf(s, "\t%d: show the last cmdbuf (if dump_cmdbuf_en != 0)\n",
				MDLA_IPI_INFO_CMDBUF);

	seq_puts(s, "\n----------- allocate debug memory ---------------\n");
	seq_printf(s, "echo [size(dec)] > /d/mdla/%s\n", DBGFS_MEM_NAME);

	return 0;
}

static int mdla_plat_v3_dbgfs_usage(struct seq_file *s, void *data)
{
	seq_puts(s, "\n---------- Command timeout setting ----------\n");
	seq_printf(s, "echo [ms(dec)] > /d/mdla/%s\n", ipi_dbgfs_file[MDLA_IPI_TIMEOUT].str);

	seq_puts(s, "\n-------- Set delay time of power off --------\n");
	seq_printf(s, "echo [ms(dec)] > /d/mdla/%s\n", ipi_dbgfs_file[MDLA_IPI_PWR_TIME].str);

	seq_puts(s, "\n----------- Set uP debug log mask -----------\n");
	seq_printf(s, "echo [mask(hex)] > /d/mdla/%s\n", mdla_plat_get_ipi_str(MDLA_IPI_ULOG));
	seq_printf(s, "\tMDLA_DBG_DRV     = 0x%x\n", 1U << V3_DBG_DRV);
	seq_printf(s, "\tMDLA_DBG_CMD     = 0x%x\n", 1U << V3_DBG_CMD);
	seq_printf(s, "\tMDLA_DBG_PMU     = 0x%x\n", 1U << V3_DBG_PMU);
	seq_printf(s, "\tMDLA_DBG_PERF    = 0x%x\n", 1U << V3_DBG_PERF);
	seq_printf(s, "\tMDLA_DBG_TIMEOUT = 0x%x\n", 1U << V3_DBG_TIMEOUT);
	seq_printf(s, "\tMDLA_DBG_PWR     = 0x%x\n", 1U << V3_DBG_PWR);
	seq_printf(s, "\tMDLA_DBG_MEM     = 0x%x\n", 1U << V3_DBG_MEM);
	seq_printf(s, "\tMDLA_DBG_IPI     = 0x%x\n", 1U << V3_DBG_IPI);
	seq_printf(s, "\tMDLA_DBG_QUEUE   = 0x%x\n", 1U << V3_DBG_QUEUE);
	seq_printf(s, "\tMDLA_DBG_LOCK    = 0x%x\n", 1U << V3_DBG_LOCK);
	seq_printf(s, "\tMDLA_DBG_TMR     = 0x%x\n", 1U << V3_DBG_TMR);
	seq_printf(s, "\tMDLA_DBG_FW      = 0x%x\n", 1U << V3_DBG_FW);

	seq_puts(s, "\n--------------- power control ---------------\n");
	seq_printf(s, "echo [0|1] > /d/mdla/%s\n", mdla_plat_get_ipi_str(MDLA_IPI_FORCE_PWR_ON));
	seq_puts(s, "\t0: force power down and reset command queue\n");
	seq_puts(s, "\t1: force power up and keep power on\n");

	seq_puts(s, "\n--------------- profile control ---------------\n");
	seq_printf(s, "echo [0|1] > /d/mdla/%s\n", mdla_plat_get_ipi_str(MDLA_IPI_PROFILE_EN));
	seq_puts(s, "\t0: stop profiling\n");
	seq_puts(s, "\t1: start to profile\n");

	seq_puts(s, "\n------------- show information -------------\n");
	seq_printf(s, "echo [item] > /d/mdla/%s\n", mdla_plat_get_ipi_str(MDLA_IPI_INFO));
	seq_puts(s, "and then cat /proc/apusys_logger/seq_log\n");
	seq_printf(s, "\t%d: show power status\n", MDLA_IPI_INFO_PWR);
	seq_printf(s, "\t%d: show register value\n", MDLA_IPI_INFO_REG);
	seq_printf(s, "\t%d: show the last cmdbuf (if dump_cmdbuf_en != 0)\n",
				MDLA_IPI_INFO_CMDBUF);
	seq_printf(s, "\t%d: show profiling result\n", MDLA_IPI_INFO_PROF);

	seq_puts(s, "\n----------- allocate debug memory -----------\n");
	seq_printf(s, "echo [size(dec)] > /d/mdla/%s\n", DBGFS_MEM_NAME);

	return 0;
}

static int mdla_plat_v5_dbgfs_usage(struct seq_file *s, void *data)
{
	seq_puts(s, "\n---------- Command timeout setting ----------\n");
	seq_printf(s, "echo [ms(dec)] > /d/mdla/%s\n", ipi_dbgfs_file[MDLA_IPI_TIMEOUT].str);

	seq_puts(s, "\n-------- Set delay time of power off --------\n");
	seq_printf(s, "echo [ms(dec)] > /d/mdla/%s\n", ipi_dbgfs_file[MDLA_IPI_PWR_TIME].str);

	seq_puts(s, "\n----------- Set uP debug log mask -----------\n");
	seq_printf(s, "echo [mask(hex)] > /d/mdla/%s\n", mdla_plat_get_ipi_str(MDLA_IPI_ULOG));
	seq_printf(s, "\tMDLA_DBG_DRV       = 0x%x\n", 1U << V5_DBG_DRV);
	seq_printf(s, "\tMDLA_DBG_CMD_LOW   = 0x%x\n", 1U << V5_DBG_CMD_LOW);
	seq_printf(s, "\tMDLA_DBG_CMD_HIGH  = 0x%x\n", 1U << V5_DBG_CMD_HIGH);
	seq_printf(s, "\tMDLA_DBG_PMU       = 0x%x\n", 1U << V5_DBG_PMU);
	seq_printf(s, "\tMDLA_DBG_PERF      = 0x%x\n", 1U << V5_DBG_PERF);
	seq_printf(s, "\tMDLA_DBG_TASK      = 0x%x\n", 1U << V5_DBG_TASK);
	seq_printf(s, "\tMDLA_DBG_PWR       = 0x%x\n", 1U << V5_DBG_PWR);
	seq_printf(s, "\tMDLA_DBG_MEM       = 0x%x\n", 1U << V5_DBG_MEM);
	seq_printf(s, "\tMDLA_DBG_IPI       = 0x%x\n", 1U << V5_DBG_IPI);
	seq_printf(s, "\tMDLA_DBG_QUEUE     = 0x%x\n", 1U << V5_DBG_QUEUE);
	seq_printf(s, "\tMDLA_DBG_LOCK      = 0x%x\n", 1U << V5_DBG_LOCK);
	seq_printf(s, "\tMDLA_DBG_TMR       = 0x%x\n", 1U << V5_DBG_TMR);
	seq_printf(s, "\tMDLA_DBG_FW        = 0x%x\n", 1U << V5_DBG_FW);
	seq_printf(s, "\tMDLA_DBG_ISR       = 0x%x\n", 1U << V5_DBG_ISR);
	seq_printf(s, "\tMDLA_DBG_UTIL      = 0x%x\n", 1U << V5_DBG_UTIL);
	seq_printf(s, "\tMDLA_DBG_CMD_QUEUE = 0x%x\n", 1U << V5_DBG_CMD_QUEUE);

	seq_puts(s, "\n--------------- power control ---------------\n");
	seq_printf(s, "echo [0|1] > /d/mdla/%s\n", mdla_plat_get_ipi_str(MDLA_IPI_FORCE_PWR_ON));
	seq_puts(s, "\t0: force power down and reset command queue\n");
	seq_puts(s, "\t1: force power up and keep power on\n");

	seq_puts(s, "\n--------------- profile control ---------------\n");
	seq_printf(s, "echo [0|1] > /d/mdla/%s\n", mdla_plat_get_ipi_str(MDLA_IPI_PROFILE_EN));
	seq_puts(s, "\t0: stop profiling\n");
	seq_puts(s, "\t1: start to profile\n");

	seq_puts(s, "\n------------- show information -------------\n");
	seq_printf(s, "echo [item] > /d/mdla/%s\n", mdla_plat_get_ipi_str(MDLA_IPI_INFO));
	seq_puts(s, "and then cat /proc/apusys_logger/seq_log\n");
	seq_printf(s, "\t%d: show power status\n", MDLA_IPI_INFO_PWR);
	seq_printf(s, "\t%d: show register value\n", MDLA_IPI_INFO_REG);
	seq_printf(s, "\t%d: show the last cmdbuf (if dump_cmdbuf_en != 0)\n",
				MDLA_IPI_INFO_CMDBUF);
	seq_printf(s, "\t%d: show profiling result\n", MDLA_IPI_INFO_PROF);

	seq_puts(s, "\n----------- allocate debug memory -----------\n");
	seq_printf(s, "echo [size(dec)] > /d/mdla/%s\n", DBGFS_MEM_NAME);

	seq_puts(s, "\n----------- set debug options (after Liber) -----------\n");
	seq_printf(s, "echo [mask(hex))] > /d/mdla/%s\n", mdla_plat_get_ipi_str(MDLA_IPI_DBG_OPTIONS));
	seq_puts(s, "\tDump cmdbuf in seq log while CMD hang            = 0x1\n");
	seq_puts(s, "\tEnable external HSE check while free HSE         = 0x10\n");
	seq_puts(s, "\tEnable internal HSE check while CMD done         = 0x20\n");
	seq_puts(s, "\tShow backup data in seq/player log if preemption = 0x100\n");

	return 0;
}

static int mdla_plat_v6_dbgfs_usage(struct seq_file *s, void *data)
{
	seq_puts(s, "\n-------------- Command timeout setting ---------------\n");
	seq_printf(s, "echo [us(dec)] > /d/mdla/%s\n", ipi_dbgfs_file[MDLA_IPI_TIMEOUT].str);

	seq_puts(s, "\n------------------ Profile control -------------------\n");
	seq_printf(s, "echo [0|1] > /d/mdla/%s\n", mdla_plat_get_ipi_str(MDLA_IPI_PROFILE_EN));
	seq_puts(s, "\t0: stop profiling\n");
	seq_puts(s, "\t1: start to profile\n");

	seq_puts(s, "\n------------- Show information in uP log -------------\n");
	seq_printf(s, "echo [0|1] > /d/mdla/%s\n", mdla_plat_get_ipi_str(MDLA_IPI_DUMP_CMDBUF_EN));
	seq_puts(s, "\tprint command buffer each time a prepare request is received\n");
	seq_printf(s, "echo [item] > /d/mdla/%s\n", mdla_plat_get_ipi_str(MDLA_IPI_INFO));
	seq_puts(s, "and then cat /proc/apusys_logger/seq_log\n");
	seq_printf(s, "\t%2d: show register value\n", MDLA_IPI_INFO_REG);
	seq_printf(s, "\t%2d: show profiling result\n", MDLA_IPI_INFO_PROF);
	seq_puts(s, "\t10: show HDSCE SRAM\n");
	seq_puts(s, "\t12: show TF status\n");
	seq_puts(s, "\t13: show snapshot data (available only when mdla is powered on)\n");
	seq_puts(s, "\t14: show fw status/backup data (available only when mdla is powered on)\n");
	seq_puts(s, "\t17: show TCU data\n");

	seq_puts(s, "\n----------------- Set debug options ------------------\n");
	seq_printf(s, "echo [mask(hex))] > /d/mdla/%s\n", mdla_plat_get_ipi_str(MDLA_IPI_DBG_OPTIONS));
	seq_puts(s, "\tDisable preemption                               = 0x0001\n");
	seq_puts(s, "\tPreempt once                                     = 0x0002\n");
	seq_puts(s, "\tForce FW always cold boot                        = 0x0008\n");
	seq_puts(s, "\tDump cmdbuf in seq log while CMD hang            = 0x0010\n");
	seq_puts(s, "\tAlways dump mdla registers before power off      = 0x0040\n");
	seq_puts(s, "\tDoesn't initialize DCM/PI/.. configurations      = 0x0100\n");
	seq_puts(s, "\tDisable engine DCM                               = 0x0200\n");
	seq_puts(s, "\tDisable stash                                    = 0x0400\n");
	seq_puts(s, "\tForce assert when mdla exception. (need unlock)  = 0x0800\n");
	seq_puts(s, "\t    unlock cmd: echo 4409527 > /d/mdla/info\n");

	seq_puts(s, "\n--------------- Set firmware log level ---------------\n");
	seq_printf(s, "echo [log_lv] > /d/mdla/%s\n", mdla_plat_get_ipi_str(MDLA_IPI_FW_LOG_LV));
	seq_puts(s, "\t0: off\n");
	seq_puts(s, "\t1: error\n");
	seq_puts(s, "\t2: info\n");
	seq_puts(s, "\t3: debug\n");

	return 0;
}

static void mdla_plat_v2_rv_configuration(void)
{
	mdla_plat_set_rv_backup_dbg_mem();
}

static void mdla_plat_v3_rv_configuration(void)
{
	mdla_plat_set_rv_backup_dbg_mem();
}

static void mdla_plat_v5_rv_configuration(void)
{
	mdla_plat_set_rv_backup_dbg_mem();
}

static void mdla_plat_v6_rv_configuration(void)
{
	mdla_plat_get_and_set_rv_dbg_mem();
	mdla_plat_get_ip_ver_from_rv();
	mdla_ipi_register_aee_handling(mdla_plat_v6_aee_handler);
}

static void mdla_plat_v2_alloc_dbg_mem(void)
{
	mdla_plat_alloc_dbg_mem(DEFAULT_DBG_SZ + 0x1000 * mdla_util_get_core_num());
}

static void mdla_plat_v3_alloc_dbg_mem(void)
{
	u32 nr_core_ids = mdla_util_get_core_num();

	mdla_plat_alloc_dbg_mem(DEFAULT_DBG_SZ + 0x1000 * nr_core_ids);
	/* backup size * core num * preempt lv */
	mdla_plat_alloc_fw_backup_mem(1024 * nr_core_ids * 4);
	mdla_plat_alloc_up_dbg_mem(DEFAULT_RV_DBG_SZ);
}

static void mdla_plat_v5_alloc_dbg_mem(void)
{
	u32 nr_core_ids = mdla_util_get_core_num();

	mdla_plat_alloc_dbg_mem(DEFAULT_DBG_SZ + 0x1000 * nr_core_ids);
	/* backup size * core num * preempt lv */
	mdla_plat_alloc_fw_backup_mem(1024 * nr_core_ids * 4);
	mdla_plat_alloc_up_dbg_mem(DEFAULT_RV_DBG_SZ);
}

static void mdla_plat_v6_alloc_dbg_mem(void)
{
	/* do nothing. allocate memory after obtaining the size through ipi on deferred kthread */
}

static void mdla_plat_v2_free_dbg_mem(void)
{
	mdla_plat_free_mem(&dbg_mem);
}

static void mdla_plat_v3_free_dbg_mem(void)
{
	mdla_plat_free_mem(&dbg_mem);
	mdla_plat_free_mem(&backup_mem);
	mdla_plat_free_mem(&rv_dbg_mem);
}

static void mdla_plat_v5_free_dbg_mem(void)
{
	mdla_plat_free_mem(&dbg_mem);
	mdla_plat_free_mem(&backup_mem);
	mdla_plat_free_mem(&rv_dbg_mem);
}

static void mdla_plat_v6_free_dbg_mem(void)
{
	mdla_plat_free_mem(&dbg_mem);
}


struct mdla_plat_ip_ops dummp_ops = {
	.dbgfs_usage      = mdla_plat_unknown_dbgfs_usage,
	.dump_dbg_mem     = mdla_rv_dbg_mem_show_nothing,
	.rv_configuration = mdla_plat_dummy_ip_ops,
	.alloc_dbg_mem    = mdla_plat_dummy_ip_ops,
	.free_dbg_mem     = mdla_plat_dummy_ip_ops,
};

struct mdla_plat_ip_ops v2_ops = {
	.dbgfs_usage      = mdla_plat_v2_dbgfs_usage,
	.dump_dbg_mem     = mdla_rv_dbg_mem_show,
	.rv_configuration = mdla_plat_v2_rv_configuration,
	.alloc_dbg_mem    = mdla_plat_v2_alloc_dbg_mem,
	.free_dbg_mem     = mdla_plat_v2_free_dbg_mem,
};

struct mdla_plat_ip_ops v3_ops = {
	.dbgfs_usage      = mdla_plat_v3_dbgfs_usage,
	.dump_dbg_mem     = mdla_rv_dbg_mem_show,
	.rv_configuration = mdla_plat_v3_rv_configuration,
	.alloc_dbg_mem    = mdla_plat_v3_alloc_dbg_mem,
	.free_dbg_mem     = mdla_plat_v3_free_dbg_mem,
};

struct mdla_plat_ip_ops v5_ops = {
	.dbgfs_usage      = mdla_plat_v5_dbgfs_usage,
	.dump_dbg_mem     = mdla_rv_dbg_mem_show,
	.rv_configuration = mdla_plat_v5_rv_configuration,
	.alloc_dbg_mem    = mdla_plat_v5_alloc_dbg_mem,
	.free_dbg_mem     = mdla_plat_v5_free_dbg_mem,
};

struct mdla_plat_ip_ops v6_ops = {
	.dbgfs_usage      = mdla_plat_v6_dbgfs_usage,
	.dump_dbg_mem     = mdla_rv_dbg_mem_dump_binary,
	.rv_configuration = mdla_plat_v6_rv_configuration,
	.alloc_dbg_mem    = mdla_plat_v6_alloc_dbg_mem,
	.free_dbg_mem     = mdla_plat_v6_free_dbg_mem,
};

static struct mdla_plat_ip_ops *ip_ops = &dummp_ops;

/*****************************************************************************/

static int mdla_plat_send_addr_info(void *arg)
{
	msleep(1000);

	ip_ops->rv_configuration();

	return 0;
}

static void mdla_plat_dbgfs_init(struct device *dev, struct dentry *parent)
{
	struct mdla_dbgfs_ipi_file *file;
	u32 i, mask, hw_ip_major_ver;

	if (!dev || !parent)
		return;

	debugfs_create_devm_seqfile(dev, DBGFS_USAGE_NAME, parent, ip_ops->dbgfs_usage);

	hw_ip_major_ver = get_major_num(mdla_plat_get_version());
	mask = BIT(hw_ip_major_ver);

	for (i = 0; ipi_dbgfs_file[i].fops != NULL; i++) {
		file = &ipi_dbgfs_file[i];
		if ((mask & file->sup_mask) != 0)
			debugfs_create_file(file->str, file->mode, parent, &file->val, file->fops);
	}

	if (hw_ip_major_ver <= 5)
		debugfs_create_file(DBGFS_MEM_NAME, 0644, parent, NULL, &mdla_rv_dbg_mem_fops);
}

static void mdla_plat_memory_show(struct seq_file *s)
{
	ip_ops->dump_dbg_mem(s, NULL);
}

void mdla_plat_up_init(void)
{
	struct task_struct *init_task;

	init_task = kthread_run(mdla_plat_send_addr_info, NULL, "mdla uP init");

	if (IS_ERR(init_task))
		mdla_err("create uP init thread failed\n");
}

/* platform public functions */
int mdla_rv_init(struct platform_device *pdev)
{
	int i;
	u32 nr_core_ids = mdla_util_get_core_num();

	dev_info(&pdev->dev, "%s()\n", __func__);

	mdla_plat_devices = devm_kzalloc(&pdev->dev,
				nr_core_ids * sizeof(struct mdla_dev),
				GFP_KERNEL);

	if (!mdla_plat_devices)
		return -1;

	switch (get_major_num(mdla_plat_get_version())) {
	case 2:
		ip_ops = &v2_ops;
		break;
	case 3:
		ip_ops = &v3_ops;
		break;
	case 5:
		ip_ops = &v5_ops;
		break;
	case 6:
		ip_ops = &v6_ops;
		break;
	default:
		break;
	}

	mdla_set_device(mdla_plat_devices, nr_core_ids);

	for (i = 0; i < nr_core_ids; i++) {
		mdla_plat_devices[i].mdla_id = i;
		mdla_plat_devices[i].dev = &pdev->dev;
	}

	mdla_plat_load_data(&pdev->dev, &boot_mem, &main_mem);

	if (mdla_ipi_init() != 0) {
		dev_info(&pdev->dev, "register apu_ctrl channel : Fail\n");
		if (mdla_plat_pwr_drv_ready())
			mdla_pwr_device_unregister(pdev);
		return -1;
	}

	mdla_dbg_plat_cb()->dbgfs_plat_init = mdla_plat_dbgfs_init;
	mdla_dbg_plat_cb()->memory_show     = mdla_plat_memory_show;

	ip_ops->alloc_dbg_mem();

	return 0;
}

void mdla_rv_deinit(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "%s()\n", __func__);

	ip_ops->free_dbg_mem();

	mdla_ipi_deinit();

	if (mdla_plat_pwr_drv_ready()
			&& mdla_pwr_device_unregister(pdev))
		dev_info(&pdev->dev, "unregister mdla power fail\n");

	mdla_plat_unload_data(&pdev->dev, &boot_mem, &main_mem);
}

