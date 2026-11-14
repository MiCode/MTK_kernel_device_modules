// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/string.h>

#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>

#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/delay.h>

#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/time.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <asm/io.h>

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
#include <linux/scmi_protocol.h>
#include <tinysys-scmi.h>
#endif

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
#include <sspm_reservedmem.h>
#endif

#include "apmcupm_scmi_v2.h"
#include "pmsr_v4.h"

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
static struct scmi_tinysys_info_st *tinfo;
static int scmi_apmcupm_id;
#endif

static unsigned int timer_window_len;
static struct pmsr_cfg cfg = {
	.enable = false,
	.met_cts_mode = 0,
	.pmsr_speed_mode = DEFAULT_SPEED_MODE,
	.pmsr_window_len = 0,
	.pmsr_sample_rate = 0,
	.err = 0,
	.test = 0,
	.prof_cnt = 0,
	.sig_count = 0,
	.share_buf = NULL,
	.pmsr_tool_share_results = NULL,
	.acc_sig_name_len = 0,
	.output_limit = PMSR_DEFAULT_OUTPUT_LIMIT,
	.rt_logbuf_size = 0,
};
struct hrtimer pmsr_timer;
static struct workqueue_struct *pmsr_tool_forcereq;
static struct delayed_work pmsr_tool_forcereq_work;
static unsigned int pmsr_tool_last_idx;
static char *pmsr_rt_logbuf = NULL;
static char pmsr_dbgbuf[PMSR_BUF_WRITESZ_LARGE] = {0};
static unsigned int pmsr_tool_print_log;
#define log2buf(p, s, fmt, args...) \
	(p += scnprintf(p, sizeof(s) - strlen(s), fmt, ##args))
#undef log
#define log(fmt, args...)   log2buf(p, pmsr_dbgbuf, fmt, ##args)

static void pmsr_cfg_init(void)
{
	int i;

	/* no need to clean mbuf_len since it should be
	 * initialized when module init
	 */
	cfg.enable = false;
	cfg.met_cts_mode = 0;
	cfg.pmsr_speed_mode = DEFAULT_SPEED_MODE;
	cfg.pmsr_window_len = 0;
	cfg.pmsr_sample_rate = 0;
	cfg.err = 0;
	cfg.test = 0;
	cfg.prof_cnt = 0;
	cfg.sig_count = 0;
	cfg.acc_sig_name_len = 0;
	cfg.output_limit = PMSR_DEFAULT_OUTPUT_LIMIT;

	for (i = 0 ; i < cfg.sig_limit; i++) {
		if (!cfg.signal_name[i]) {
			kfree(cfg.signal_name[i]);
			cfg.signal_name[i] = NULL;
		}
	}

	for (i = 0 ; i < cfg.dpmsr_count; i++) {
		cfg.dpmsr[i].seltype = DEFAULT_SELTYPE;
		cfg.dpmsr[i].montype = DEFAULT_MONTYPE;
		cfg.dpmsr[i].signum = 0;
		cfg.dpmsr[i].en = 0;
	}

	/* pmsr_tool_last_idx is the last position read by ap */
	/* when pmsr tool just starts and read idx below is 1, */
	/* it means ap is faster than sspm -> there is no new data in sram */
	pmsr_tool_last_idx = cfg.pmsr_tool_buffer_max_space - 1;
}

static int pmsr_ipi_init(void)
{
	int ret = 0;

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
	tinfo = get_scmi_tinysys_info();

	if (!tinfo) {
		pr_info("get scmi info fail\n");
		return -EFAULT;
	}

	/* Return 0 on success
	 * -EINVAL if the property not exist
	 */
	ret = of_property_read_u32(tinfo->sdev->dev.of_node, "scmi-apmcupm",
			&scmi_apmcupm_id);
	if (ret) {
		pr_info("get dts property fail, ret %d\n", ret);
		return -ret;
	}
#endif

	return ret;
}

static int pmsr_get_info_by_scmi(unsigned int act)
{
	unsigned int user_info =
				(APMCU_SET_UID(APMCU_SCMI_UID_PMSR) |
				APMCU_SET_UUID(APMCU_SCMI_UUID_PMSR) |
				APMCU_SET_MG);
	int ret = 0;

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
	struct scmi_tinysys_status pmsr_tool_rvalue;
	struct apmcupm_scmi_set_input pmsr_scmi_set_data;

	/* Send a SCMI cmd */
	pmsr_scmi_set_data.user_info =
			(user_info | APMCU_SET_ACT(act));
	ret = scmi_tinysys_common_get(tinfo->ph, scmi_apmcupm_id,
			pmsr_scmi_set_data.user_info, &pmsr_tool_rvalue);
	if (ret) {
		cfg.err |= (1 << act);
		return ret;
	}

	/* Parsing the results from SCMI */
	switch(act) {
		case(PMSR_TOOL_ACT_GET_SRAM):
			cfg.share_buf =
				(struct pmsr_tool_mon_results_addr *)sspm_sbuf_get(pmsr_tool_rvalue.r1);
			if (cfg.share_buf) {
				cfg.sig_limit = cfg.share_buf[0].data_num;
				cfg.signal_name = kcalloc(cfg.sig_limit, sizeof(const char *), GFP_KERNEL);
			}
			cfg.pmsr_tool_buffer_max_space = pmsr_tool_rvalue.r2;
			cfg.pmsr_tool_share_results =
				(struct pmsr_tool_results *)sspm_sbuf_get(pmsr_tool_rvalue.r3);
			break;
		case(PMSR_TOOL_ACT_GET_MBUF_LEN):
			cfg.mbuf_data_limit = pmsr_tool_rvalue.r1;
			break;
		default:
			break;
	}
#endif
	return ret;
}

static void pmsr_tool_send_forcereq(struct work_struct *work)
{
	uint64_t timestamp = 0;
	unsigned int read_idx;
	unsigned int oldest_idx;
	struct pmsr_tool_mon_results *pmsr_tool_val;
	unsigned int *pmsr_tool_val_data;
	int ret;
	unsigned int i;
	unsigned int log_cnt = 0;
	unsigned int out_unit_max_ch = 0;
	unsigned int user_info =
			(APMCU_SET_UID(APMCU_SCMI_UID_PMSR) |
			APMCU_SET_UUID(APMCU_SCMI_UUID_PMSR) |
			APMCU_SET_MG);
	struct apmcupm_scmi_set_input pmsr_scmi_set_data;
	char *p = NULL;
	char *p1 = NULL;

	pmsr_tool_print_log = 1;
	/* if cfg.test = 1, then scmi has been sent */
	if ((cfg.pmsr_sample_rate != 0) && (cfg.test != 1)) {
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
		pmsr_scmi_set_data.user_info =
			(user_info | APMCU_SET_ACT(PMSR_TOOL_ACT_TEST));
		ret = scmi_tinysys_common_set(tinfo->ph, scmi_apmcupm_id,
			pmsr_scmi_set_data.user_info, 0, 0, 0, 0);
		if (ret)
			cfg.err |= (1 << PMSR_TOOL_ACT_TEST);
#endif
	}

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
	if (!cfg.pmsr_tool_share_results)
		return;
	oldest_idx = cfg.pmsr_tool_share_results->oldest_idx;
	read_idx = oldest_idx > 0 ?
		oldest_idx - 1 : cfg.pmsr_tool_buffer_max_space - 1;

	/* retrieve the pmsr results if they have not been retrieved yet */
	if ((read_idx != pmsr_tool_last_idx) && (cfg.share_buf) &&
		(read_idx < cfg.pmsr_tool_buffer_max_space)) {

		/* pmsr_tool_val - timestamp / window length */
		/* pmsr_tool_val_data - signal count */
		pmsr_tool_val = (struct pmsr_tool_mon_results *)sspm_sbuf_get(cfg.share_buf[read_idx].mon_res_addr);
		pmsr_tool_val_data = (unsigned int *)sspm_sbuf_get(cfg.share_buf[read_idx].data_addr);

		pmsr_tool_last_idx = read_idx;

		/* run-time print the results */
		if (!pmsr_rt_logbuf)
				return;

		timestamp = readl(&pmsr_tool_val->timestamp_h);
		timestamp = (timestamp << 32) | readl(&pmsr_tool_val->timestamp_l);
		p = pmsr_rt_logbuf;
		p[0] = '\0';
		/* prepare the log header */
		p += scnprintf(p, cfg.rt_logbuf_size - strlen(pmsr_rt_logbuf),
						"[PMSR][%u][%llu]", log_cnt, timestamp);

		/* count the length of a formated winlen value */
		p1 = p;
		p += scnprintf(p, cfg.rt_logbuf_size - strlen(pmsr_rt_logbuf),
						"(%u):", pmsr_tool_val->winlen);
		out_unit_max_ch = p - p1;

		/* 1. push the value one by one to the buffer until the remained size
		 * is not enough
		 * 2. print the data out and goto the step 1
		 */
		for (i = 0; i < cfg.output_limit; i++) {
			if ((cfg.rt_logbuf_size - strlen(pmsr_rt_logbuf)) < out_unit_max_ch) {
				pr_notice("%s\n", pmsr_rt_logbuf);
				log_cnt += 1;
				p = pmsr_rt_logbuf;
				p[0] = '\0';
				/* prepare the log header + 1 data remains */
				p += scnprintf(p, cfg.rt_logbuf_size - strlen(pmsr_rt_logbuf),
						"[PMSR][%u][%llu](%u):",
						log_cnt, timestamp, pmsr_tool_val->winlen);
			}
			p += scnprintf(p, cfg.rt_logbuf_size - strlen(pmsr_rt_logbuf),
						" %u", pmsr_tool_val_data[i]);
		}
		pr_notice("%s\n", pmsr_rt_logbuf);
	}
#endif
	pmsr_tool_print_log = 0;
}

static void pmsr_procfs_exit(void)
{
	remove_proc_entry("pmsr", NULL);
}

static ssize_t remote_data_read(struct file *filp, char __user *userbuf,
				size_t count, loff_t *f_pos)
{
	return 0;
}

static ssize_t remote_data_write(struct file *fp, const char __user *userbuf,
				 size_t count, loff_t *f_pos)
{
	unsigned int *v = pde_data(file_inode(fp));
	unsigned int user_info =
				(APMCU_SET_UID(APMCU_SCMI_UID_PMSR) |
				APMCU_SET_UUID(APMCU_SCMI_UUID_PMSR) |
				APMCU_SET_MG);
	int ret;
	int i;
	unsigned int met_mode = 0;
	struct apmcupm_scmi_set_input pmsr_scmi_set_data;

	if (!userbuf || !v)
		return -EINVAL;

	if (count >= PMSR_BUF_WRITESZ)
		return -EINVAL;

	if (kstrtou32_from_user(userbuf, count, 10, v))
		return -EFAULT;

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
	if (!tinfo)
		return -EFAULT;
#endif

	if ((void *)v == (void *)&cfg.enable) {
		if (cfg.enable == true) {
			/* the output limit should not bigger than total signal count */
			if (cfg.sig_count < cfg.output_limit)
				cfg.output_limit = cfg.sig_count;

			/* calculate the rt logbuf size:
			 * output_limit (N unit) * len (1 unit) + header len
			 * this size should not longer than the max log length
			 * Ex: if output limit is 400
			 *     rt logbuf size = 400 * 7 + 80
			 */
			cfg.rt_logbuf_size = (cfg.output_limit * PMSR_RT_LOGBUF_UNIT) +
					PMSR_DEFAULT_RT_LOGBUF_HEADER;
			if (cfg.rt_logbuf_size > PMSR_RT_LOGBUF_LIMIT)
				cfg.rt_logbuf_size = PMSR_RT_LOGBUF_LIMIT;

			/* allocate the memory for rt logbuf */
			pmsr_rt_logbuf = kzalloc(cfg.rt_logbuf_size, GFP_KERNEL);
			if (!pmsr_rt_logbuf)
				pr_notice("[PMSR] pmsr_rt_logbuf allocate err!\n");

			/* pass the window length/sample rate with met_mode */
			if (cfg.pmsr_sample_rate != 0) {
				timer_window_len = cfg.pmsr_sample_rate;
				cfg.pmsr_window_len = 0;
				met_mode = (1 << PMSR_MODE_FOR_MET_FR);
				cfg.enable_hrtimer = true;
			}
			if (cfg.pmsr_window_len != 0) {
				timer_window_len = cfg.pmsr_window_len;
				cfg.pmsr_sample_rate = 0;
				met_mode = (1 << PMSR_MODE_FOR_MET_TO);
				cfg.enable_hrtimer = false;
			}
			if (cfg.met_cts_mode)
				met_mode |= (1 << PMSR_MODE_FOR_MET_CTS);

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
			pmsr_scmi_set_data.user_info =
				(user_info | APMCU_SET_ACT(PMSR_TOOL_ACT_WINDOW));
			pmsr_scmi_set_data.in1 = cfg.pmsr_window_len;
			pmsr_scmi_set_data.in2 = cfg.pmsr_speed_mode;
			pmsr_scmi_set_data.in3 = met_mode;
			ret = scmi_tinysys_common_set(tinfo->ph, scmi_apmcupm_id,
				pmsr_scmi_set_data.user_info,
				pmsr_scmi_set_data.in1,
				pmsr_scmi_set_data.in2,
				pmsr_scmi_set_data.in3, 0);
			if (ret)
				cfg.err |= (1 << PMSR_TOOL_ACT_WINDOW);
#endif

			/* pass the signum */
			for (i = 0; i < cfg.dpmsr_count; i++) {
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
				pmsr_scmi_set_data.user_info =
					(user_info | APMCU_SET_ACT(PMSR_TOOL_ACT_SIGNUM));
				pmsr_scmi_set_data.in1 = i;
				pmsr_scmi_set_data.in2 = cfg.dpmsr[i].signum;
				ret = scmi_tinysys_common_set(tinfo->ph, scmi_apmcupm_id,
					pmsr_scmi_set_data.user_info,
					pmsr_scmi_set_data.in1,
					pmsr_scmi_set_data.in2,
					0, 0);
				if (ret)
					cfg.err |= (1 << PMSR_TOOL_ACT_SIGNUM);
#endif
			}
			/* pass the seltype */
			for (i = 0; i < cfg.dpmsr_count; i++) {
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
				pmsr_scmi_set_data.user_info =
					(user_info | APMCU_SET_ACT(PMSR_TOOL_ACT_SELTYPE));
				pmsr_scmi_set_data.in1 = i;
				pmsr_scmi_set_data.in2 = cfg.dpmsr[i].seltype;
				ret = scmi_tinysys_common_set(tinfo->ph, scmi_apmcupm_id,
					pmsr_scmi_set_data.user_info,
					pmsr_scmi_set_data.in1, pmsr_scmi_set_data.in2,
					0, 0);
				if (ret)
					cfg.err |= (1 << PMSR_TOOL_ACT_SELTYPE);
#endif
			}
			/* pass the montype */
			for (i = 0; i < cfg.dpmsr_count; i++) {
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
				pmsr_scmi_set_data.user_info =
					(user_info | APMCU_SET_ACT(PMSR_TOOL_ACT_MONTYPE));
				pmsr_scmi_set_data.in1 = i;
				pmsr_scmi_set_data.in2 = cfg.dpmsr[i].montype;
				ret = scmi_tinysys_common_set(tinfo->ph, scmi_apmcupm_id,
					pmsr_scmi_set_data.user_info,
					pmsr_scmi_set_data.in1, pmsr_scmi_set_data.in2,
					0, 0);
				if (ret)
					cfg.err |= (1 << PMSR_TOOL_ACT_MONTYPE);
#endif
			}
			/* pass the en */
			for (i = 0; i < cfg.dpmsr_count; i++) {
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
				pmsr_scmi_set_data.user_info =
					(user_info | APMCU_SET_ACT(PMSR_TOOL_ACT_EN));
				pmsr_scmi_set_data.in1 = i;
				pmsr_scmi_set_data.in2 = cfg.dpmsr[i].en;
				ret = scmi_tinysys_common_set(tinfo->ph, scmi_apmcupm_id,
					pmsr_scmi_set_data.user_info,
					pmsr_scmi_set_data.in1, pmsr_scmi_set_data.in2,
					0, 0);
				if (ret)
					cfg.err |= (1 << PMSR_TOOL_ACT_EN);
#endif
			}
			/* pass the prof_cnt */
			pmsr_scmi_set_data.user_info =
				(user_info | APMCU_SET_ACT(PMSR_TOOL_ACT_PROF_CNT_EN));
			pmsr_scmi_set_data.in1 = cfg.prof_cnt;
			ret = scmi_tinysys_common_set(tinfo->ph, scmi_apmcupm_id,
				pmsr_scmi_set_data.user_info,
				pmsr_scmi_set_data.in1, 0, 0, 0);
			if (ret)
				cfg.err |= (1 << PMSR_TOOL_ACT_PROF_CNT_EN);

			/* pass the enable command */
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
			pmsr_scmi_set_data.user_info =
				(user_info | APMCU_SET_ACT(PMSR_TOOL_ACT_ENABLE));
			ret = scmi_tinysys_common_set(tinfo->ph, scmi_apmcupm_id,
				pmsr_scmi_set_data.user_info,
				cfg.sig_count, 0, 0, 0);
#endif
			if (!ret) {
				if ((timer_window_len != 0) && (cfg.enable_hrtimer)) {
					hrtimer_start(&pmsr_timer,
							ns_to_ktime(timer_window_len * NSEC_PER_USEC),
							HRTIMER_MODE_REL_PINNED);
				}
			} else {
				cfg.err |= (1 << PMSR_TOOL_ACT_ENABLE);
			}
		} else {
			pmsr_cfg_init();
			/* pmsr_rt_logbuf should be freed after workqueue has been finished */
			while(pmsr_tool_print_log);
			if (!pmsr_tool_print_log) {
				kfree(pmsr_rt_logbuf);
				pmsr_rt_logbuf = NULL;
			}
			cfg.rt_logbuf_size = 0;
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
			pmsr_scmi_set_data.user_info =
				(user_info | APMCU_SET_ACT(PMSR_TOOL_ACT_DISABLE));
			ret = scmi_tinysys_common_set(tinfo->ph, scmi_apmcupm_id,
				pmsr_scmi_set_data.user_info,
				0, 0, 0, 0);
			if (ret)
				cfg.err |= (1 << PMSR_TOOL_ACT_DISABLE);
#endif
			hrtimer_try_to_cancel(&pmsr_timer);
		}
	} else if ((void *)v == (void *)&cfg.test) {
		if (cfg.test == 1) {
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
			if (!cfg.pmsr_tool_share_results || !cfg.share_buf)
				return -EFAULT;

			pmsr_scmi_set_data.user_info =
				(user_info | APMCU_SET_ACT(PMSR_TOOL_ACT_TEST));
			ret = scmi_tinysys_common_set(tinfo->ph, scmi_apmcupm_id,
				pmsr_scmi_set_data.user_info, 0, 0, 0, 0);
			if (ret)
				cfg.err |= (1 << PMSR_TOOL_ACT_TEST);
			else
				queue_delayed_work(pmsr_tool_forcereq, &pmsr_tool_forcereq_work, 0);
#endif
		}
	}
	return count;
}

static const struct proc_ops remote_data_fops = {
	.proc_read = remote_data_read,
	.proc_write = remote_data_write,
};

/* TODO: show warning if log buffer is nearly full */
static ssize_t local_ipi_read(struct file *fp, char __user *userbuf,
			      size_t count, loff_t *f_pos)
{
	int i, len = 0;
	char *p = pmsr_dbgbuf;

	p[0] = '\0';

	log("enable %d\n", cfg.enable ? 1 : 0);
	log("customized ts %u\n", !!cfg.met_cts_mode);
	log("speed_mode %u\n", cfg.pmsr_speed_mode);
	log("prof_cnt %u\n", cfg.prof_cnt);
	log("window_len %u (0x%x)\n",
	    cfg.pmsr_window_len, cfg.pmsr_window_len);
	log("sample_rate %u (0x%x)\n",
	    cfg.pmsr_sample_rate, cfg.pmsr_sample_rate);
	log("mbuf_data_limit %u\n", cfg.mbuf_data_limit);
	log("sig_limit %u\n", cfg.sig_limit);
	log("output_limit %u\n", cfg.output_limit);

	/* do not change this line */
	log("sig_cnt %u\n", cfg.sig_count);
	for (i = 0; i < cfg.sig_count; i++) {
		if (cfg.signal_name[i])
			log("%s\n",cfg.signal_name[i]);
	}

	for (i = 0; i < cfg.dpmsr_count; i++) {
		log("dpmsr %u seltype %u montype %u (%s) signum %u en %u\n",
				i,
				cfg.dpmsr[i].seltype,
				cfg.dpmsr[i].montype,
				cfg.dpmsr[i].montype == 0 ? "R" :
				cfg.dpmsr[i].montype == 1 ? "F" :
				cfg.dpmsr[i].montype == 2 ? "H" :
				cfg.dpmsr[i].montype == 3 ? "L" :
				"N",
				cfg.dpmsr[i].signum,
				cfg.dpmsr[i].en);
	}
	log("err 0x%x\n", cfg.err);

	len = p - pmsr_dbgbuf;
	return simple_read_from_buffer(userbuf, count, f_pos, pmsr_dbgbuf, len);
}

static ssize_t local_ipi_write(struct file *fp, const char __user *userbuf,
			       size_t count, loff_t *f_pos)
{
	unsigned int *v = pde_data(file_inode(fp));

	if (!userbuf || !v)
		return -EINVAL;

	if (count >= PMSR_BUF_WRITESZ)
		return -EINVAL;

	if (kstrtou32_from_user(userbuf, count, 10, v))
		return -EFAULT;

	return count;
}

static const struct proc_ops local_ipi_fops = {
	.proc_read = local_ipi_read,
	.proc_write = local_ipi_write,
};

static ssize_t local_signal_read(struct file *filp, char __user *userbuf,
				size_t count, loff_t *f_pos)
{
	int len = 0;
	char *p = pmsr_dbgbuf;

	len = p - pmsr_dbgbuf;
	return simple_read_from_buffer(userbuf, count, f_pos, pmsr_dbgbuf, len);
}

static char *pmsr_tool_proc_sig_name = "signal_name";
static char *pmsr_tool_proc_sig_id = "signal_id";
static ssize_t local_signal_write(struct file *fp, const char *userbuf,
			       size_t count, loff_t *f_pos)
{
	unsigned int *share_buf_id;
	char *kbuf_ptr = &pmsr_dbgbuf[0];
	char *sig_name = NULL;
	int len = 0;
	int idx = 0;

	if (count > PMSR_BUF_WRITESZ_LARGE)
		pr_notice("[PMSR] Err:signal size > %u\n", PMSR_BUF_WRITESZ_LARGE);

	share_buf_id = (unsigned int *)sspm_sbuf_get(cfg.share_buf[0].data_addr);
	if ((!userbuf) || (count > PMSR_BUF_WRITESZ_LARGE) || (!share_buf_id))
		return -EINVAL;

	/* TODO: what the buffer limitation for copy_from_user */
	memset(kbuf_ptr, 0, PMSR_BUF_WRITESZ_LARGE);
	if (copy_from_user(kbuf_ptr, userbuf, count))
		return -EFAULT;

	if (strncmp((fp->f_path.dentry->d_iname), pmsr_tool_proc_sig_name,
					strlen(pmsr_tool_proc_sig_name)) == 0) {
		/* Returns a pointer to a signal name string.
		 * This string is at most PMSR_SIGNAL_NAME_MAX_LEN bytes
		 * and is terminated with a null byte.
		 * Returns a NULL if insufficient memory was available
		 */
		while((sig_name = strsep(&kbuf_ptr, " "))) {
			if (idx == cfg.sig_limit) {
				pr_notice("[PMSR] Err:signal name > %u\n", cfg.sig_limit);
				break;
			}
			cfg.signal_name[idx++] =
				kstrndup(sig_name, PMSR_SIGNAL_NAME_MAX_LEN, GFP_KERNEL);
			cfg.acc_sig_name_len += strlen(sig_name);
		};
	} else if(strncmp((fp->f_path.dentry->d_iname), pmsr_tool_proc_sig_id,
					strlen(pmsr_tool_proc_sig_id)) == 0) {
		/* get the id each time, and move the kbuf_ptr by len */
		/* sscanf returns -1 when kbuf_ptr moves to the end of the string */
		while (sscanf(kbuf_ptr, "%u %n", &share_buf_id[idx], &len) == 1) {
			kbuf_ptr += len;
			idx += 1;
		}
	} else {
		return count;
	}

	if ((cfg.sig_count == 0) || (idx < cfg.sig_count))
		cfg.sig_count = idx;

	return count;
}

static const struct proc_ops local_signal_fops = {
	.proc_read = local_signal_read,
	.proc_write = local_signal_write,
};

static struct proc_dir_entry *pmsr_droot;

static int pmsr_procfs_init(void)
{
	int i;
	struct proc_dir_entry *dpmsr_dir_entry;

	pmsr_droot = proc_mkdir("pmsr", NULL);
	if (pmsr_droot) {
		proc_create("state", 0644, pmsr_droot, &local_ipi_fops);
		proc_create_data("output_limit", 0644, pmsr_droot, &local_ipi_fops,
				 (void *) &(cfg.output_limit));
		proc_create_data("speed_mode", 0644, pmsr_droot, &local_ipi_fops,
				 (void *) &(cfg.pmsr_speed_mode));
		proc_create_data("window_len", 0644, pmsr_droot, &local_ipi_fops,
				 (void *) &(cfg.pmsr_window_len));
		proc_create_data("prof_cnt", 0644, pmsr_droot, &local_ipi_fops,
				 (void *) &(cfg.prof_cnt));
		proc_create_data("sample_rate", 0644, pmsr_droot, &local_ipi_fops,
				 (void *) &(cfg.pmsr_sample_rate));
		proc_create_data("met_cts_mode", 0644, pmsr_droot, &local_ipi_fops,
				 (void *) &(cfg.met_cts_mode));
		proc_create_data("enable", 0644, pmsr_droot, &remote_data_fops,
				 (void *) &(cfg.enable));
		proc_create_data("test", 0644, pmsr_droot, &remote_data_fops,
				 (void *) &(cfg.test));
		proc_create("signal_name", 0644, pmsr_droot, &local_signal_fops);
		proc_create("signal_id", 0644, pmsr_droot, &local_signal_fops);

		/* If cfg.dpmsr is NULL, return -1 since the memory allocation
		 * might go wrong before this step
		 */
		if (!cfg.dpmsr)
			return -1;

		for (i = 0 ; i < cfg.dpmsr_count; i++) {
			char name[10];
			int len = 0;

			len = snprintf(name, sizeof(name), "dpmsr%d", i);
			/* len should be less than the size of name[] */
			if (len >= sizeof(name))
				pr_notice("dpmsr name fail\n");

			dpmsr_dir_entry = proc_mkdir(name, pmsr_droot);
			if (dpmsr_dir_entry) {
				proc_create_data("seltype",
						 0644, dpmsr_dir_entry, &local_ipi_fops,
						 (void *)&(cfg.dpmsr[i].seltype));
				proc_create_data("montype",
						 0644, dpmsr_dir_entry, &local_ipi_fops,
						 (void *)&(cfg.dpmsr[i].montype));
				proc_create_data("signum",
						 0644, dpmsr_dir_entry, &local_ipi_fops,
						 (void *)&(cfg.dpmsr[i].signum));
				proc_create_data("en",
						 0644, dpmsr_dir_entry, &local_ipi_fops,
						 (void *)&(cfg.dpmsr[i].en));
			}
		}
	}

	return 0;
}

static enum hrtimer_restart pmsr_timer_handle(struct hrtimer *timer)
{
	hrtimer_forward(timer, timer->base->get_time(),
			ns_to_ktime(timer_window_len * NSEC_PER_USEC));

	queue_delayed_work(pmsr_tool_forcereq, &pmsr_tool_forcereq_work, 0);

	return HRTIMER_RESTART;
}

static int __init pmsr_parsing_nodes(void)
{
	struct device_node *pmsr_node = NULL;
	char pmsr_desc[] = "mediatek,mtk-pmsr";
	int ret = 0;

	/* find the root node in dts */
	pmsr_node = of_find_compatible_node(NULL, NULL, pmsr_desc);
	if (!pmsr_node) {
		pr_notice("unable to find pmsr device node\n");
		return -1;
	}

	/* find the node and get the value */
	ret = of_property_read_u32_index(pmsr_node, "dpmsr-count",
			0, &cfg.dpmsr_count);
	if (ret) {
		pr_notice("fail to get dpmsr-count from dts\n");
		return -1;
	}
	return 0;
}

static int __init pmsr_init(void)
{
	/* register ipi */
	if (pmsr_ipi_init())
		return -1;

	/* if an error occurs while parsing the nodes, return directly */
	if (pmsr_parsing_nodes())
		return -1;

	/* allocate a memory for dpmsr configurations */
	cfg.dpmsr = kmalloc_array(cfg.dpmsr_count,
					sizeof(struct pmsr_dpmsr_cfg), GFP_KERNEL);

	/* pmsr get sspm sram address */
	if (pmsr_get_info_by_scmi(PMSR_TOOL_ACT_GET_SRAM))
		return -1;

	pmsr_cfg_init();

	if (pmsr_get_info_by_scmi(PMSR_TOOL_ACT_GET_MBUF_LEN))
		return -1;

	/* create debugfs node */
	pmsr_procfs_init();

	hrtimer_init(&pmsr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	pmsr_timer.function = pmsr_timer_handle;

	if (!pmsr_tool_forcereq) {
		pmsr_tool_forcereq = alloc_workqueue("pmsr_tool_forcereq", WQ_HIGHPRI, 0);

		if (!pmsr_tool_forcereq)
			pr_notice("pmsr tool forcereq workqueue fail\n");
	}

	INIT_DELAYED_WORK(&pmsr_tool_forcereq_work, pmsr_tool_send_forcereq);

	return 0;
}

module_init(pmsr_init);

static void __exit pmsr_exit(void)
{
	/* remove debugfs node */
	pmsr_procfs_exit();
	kfree(cfg.dpmsr);
	hrtimer_try_to_cancel(&pmsr_timer);

	flush_workqueue(pmsr_tool_forcereq);
	destroy_workqueue(pmsr_tool_forcereq);
}

module_exit(pmsr_exit);

MODULE_DESCRIPTION("Mediatek MT68XX pmsr driver");
MODULE_AUTHOR("SHChen <Show-Hong.Chen@mediatek.com>");
MODULE_LICENSE("GPL");
