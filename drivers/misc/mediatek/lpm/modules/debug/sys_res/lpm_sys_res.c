// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/fs.h>
#include <linux/spinlock.h>
#include <linux/suspend.h>
#include <lpm_dbg_fs_common.h>
#include <lpm_sys_res.h>
#include <lpm_sys_res_fs.h>

static struct lpm_sys_res_ops _lpm_sys_res_ops;
static spinlock_t _lpm_sys_res_lock;

static int sys_res_common_log_enable;
static unsigned int last_buffer_index;
static unsigned int temp_buffer_index;
static unsigned int last_suspend_diff_buffer_index;
static unsigned int last_diff_buffer_index;
static struct sys_res_record sys_res_stat[SYS_RES_SCENE_NUM];
struct sys_res_group_info *group_info;

#define NON_RES_SIG_GROUP (0xFFFFFFFF)
#define DEFAULT_RES_THRESHOLD (30)

static struct sys_res_mapping subsys_mapping[] = {
	{0, "md"},
	{0, "conn"},
	{0, "scp"},
	{0, "adsp"},
	{0, "pcie0"},
	{0, "pcie1"},
	{0, "mmproc"},
	{0, "uarthub"},
};


int lpm_sys_res_init(void)
{
	lpm_sys_res_fs_init();
	pr_info("%s %d: finish", __func__, __LINE__);
	return 0;
}
EXPORT_SYMBOL(lpm_sys_res_init);

void lpm_sys_res_exit(void)
{
	lpm_sys_res_fs_deinit();
	pr_info("%s %d: finish", __func__, __LINE__);
}
EXPORT_SYMBOL(lpm_sys_res_exit);

struct lpm_sys_res_ops *get_lpm_sys_res_ops(void)
{
	return &_lpm_sys_res_ops;
}
EXPORT_SYMBOL(get_lpm_sys_res_ops);

int register_lpm_sys_res_ops(struct lpm_sys_res_ops *ops)
{
	if (!ops)
		return -1;

	_lpm_sys_res_ops.get = ops->get;
	_lpm_sys_res_ops.update = ops->update;
	_lpm_sys_res_ops.get_detail = ops->get_detail;
	_lpm_sys_res_ops.get_threshold = ops->get_threshold;
	_lpm_sys_res_ops.set_threshold = ops->set_threshold;
	_lpm_sys_res_ops.enable_common_log = ops->enable_common_log;
	_lpm_sys_res_ops.get_log_enable = ops->get_log_enable;
	_lpm_sys_res_ops.log = ops->log;
	_lpm_sys_res_ops.lock = ops->lock;
	_lpm_sys_res_ops.get_id_name = ops->get_id_name;

	return 0;
}
EXPORT_SYMBOL(register_lpm_sys_res_ops);

void unregister_lpm_sys_res_ops(void)
{
	_lpm_sys_res_ops.get = NULL;
	_lpm_sys_res_ops.update = NULL;
	_lpm_sys_res_ops.get_detail = NULL;
	_lpm_sys_res_ops.get_threshold = NULL;
	_lpm_sys_res_ops.set_threshold = NULL;
	_lpm_sys_res_ops.enable_common_log = NULL;
	_lpm_sys_res_ops.get_log_enable = NULL;
	_lpm_sys_res_ops.log = NULL;
	_lpm_sys_res_ops.lock = NULL;
	_lpm_sys_res_ops.get_id_name = NULL;
}
EXPORT_SYMBOL(unregister_lpm_sys_res_ops);

static int lpm_sys_res_stat_alloc(struct sys_res_record *stat)
{
	struct res_sig_stats *spm_res_sig_stats_ptr;

	if (!stat)
		return -1;

	spm_res_sig_stats_ptr =
	kcalloc(1, sizeof(struct res_sig_stats), GFP_KERNEL);
	if (!spm_res_sig_stats_ptr)
		goto RES_SIG_ALLOC_ERROR;

	get_res_sig_stats(spm_res_sig_stats_ptr);
	if(!spm_res_sig_stats_ptr->res_sig_num)
		goto RES_SIG_ALLOC_TABLE_ERROR;

	spm_res_sig_stats_ptr->res_sig_tbl =
	kcalloc(spm_res_sig_stats_ptr->res_sig_num,
			sizeof(struct res_sig), GFP_KERNEL);
	if (!spm_res_sig_stats_ptr->res_sig_tbl)
		goto RES_SIG_ALLOC_TABLE_ERROR;

	get_res_sig_stats(spm_res_sig_stats_ptr);
	stat->spm_res_sig_stats_ptr = spm_res_sig_stats_ptr;

	return 0;

RES_SIG_ALLOC_TABLE_ERROR:
	kfree(spm_res_sig_stats_ptr);
	stat->spm_res_sig_stats_ptr = NULL;
RES_SIG_ALLOC_ERROR:
	return -1;

}

static void lpm_sys_res_stat_free(struct sys_res_record *stat)
{
	if(stat && stat->spm_res_sig_stats_ptr) {
		kfree(stat->spm_res_sig_stats_ptr->res_sig_tbl);
		kfree(stat->spm_res_sig_stats_ptr);
		stat->spm_res_sig_stats_ptr = NULL;
	}
}

static int __sync_lastest_lpm_sys_res_stat(struct sys_res_record *stat)
{
	int ret = 0;
#if IS_ENABLED(CONFIG_MTK_SWPM_MODULE)
	if (!stat ||
	    !stat->spm_res_sig_stats_ptr)
		return ret;

	ret = sync_latest_data();
	get_res_sig_stats(stat->spm_res_sig_stats_ptr);
#endif
	return ret;
}

static void __lpm_sys_res_stat_diff(struct sys_res_record *result,
				   struct sys_res_record *prev,
				   struct sys_res_record *cur)
{
	int i;

	if (!result || !prev || !cur)
		return;

	result->spm_res_sig_stats_ptr->suspend_time =
		prev->spm_res_sig_stats_ptr->suspend_time -
		cur->spm_res_sig_stats_ptr->suspend_time;

	result->spm_res_sig_stats_ptr->duration_time =
		prev->spm_res_sig_stats_ptr->duration_time -
		cur->spm_res_sig_stats_ptr->duration_time;

	for (i = 0; i < result->spm_res_sig_stats_ptr->res_sig_num; i++) {
		result->spm_res_sig_stats_ptr->res_sig_tbl[i].time =
		prev->spm_res_sig_stats_ptr->res_sig_tbl[i].time -
		cur->spm_res_sig_stats_ptr->res_sig_tbl[i].time;
	}

}

static void __lpm_sys_res_stat_add(struct sys_res_record *result,
				   struct sys_res_record *delta)
{
	int i;

	if (!result || !delta)
		return;

	result->spm_res_sig_stats_ptr->suspend_time +=
		delta->spm_res_sig_stats_ptr->suspend_time;

	result->spm_res_sig_stats_ptr->duration_time +=
		delta->spm_res_sig_stats_ptr->duration_time;

	for (i = 0; i < result->spm_res_sig_stats_ptr->res_sig_num; i++) {
		result->spm_res_sig_stats_ptr->res_sig_tbl[i].time +=
		delta->spm_res_sig_stats_ptr->res_sig_tbl[i].time;
	}
}

static int update_lpm_sys_res_stat(void)
{
	unsigned int temp;
	unsigned long flag;
	int ret;

	ret = __sync_lastest_lpm_sys_res_stat(&sys_res_stat[temp_buffer_index]);

	spin_lock_irqsave(&_lpm_sys_res_lock, flag);
	__lpm_sys_res_stat_diff(&sys_res_stat[last_diff_buffer_index],
			&sys_res_stat[temp_buffer_index],
			&sys_res_stat[last_buffer_index]);

	if (sys_res_stat[last_diff_buffer_index].spm_res_sig_stats_ptr->suspend_time > 0) {
		__lpm_sys_res_stat_add(&sys_res_stat[SYS_RES_SCENE_SUSPEND],
				      &sys_res_stat[last_diff_buffer_index]);

		temp = last_diff_buffer_index;
		last_diff_buffer_index = last_suspend_diff_buffer_index;
		last_suspend_diff_buffer_index = temp;

	} else {
		__lpm_sys_res_stat_add(&sys_res_stat[SYS_RES_SCENE_COMMON],
				      &sys_res_stat[last_diff_buffer_index]);
	}

	temp = temp_buffer_index;
	temp_buffer_index = last_buffer_index;
	last_buffer_index = temp;
	spin_unlock_irqrestore(&_lpm_sys_res_lock, flag);

	return ret;
}

static struct sys_res_record *get_lpm_sys_res_stat(unsigned int scene)
{
	if (scene >= SYS_RES_GET_SCENE_NUM)
		return NULL;

	switch(scene) {
	case SYS_RES_COMMON:
		return &sys_res_stat[SYS_RES_SCENE_COMMON];
	case SYS_RES_SUSPEND:
		return &sys_res_stat[SYS_RES_SCENE_SUSPEND];
	case SYS_RES_LAST_SUSPEND:
		return &sys_res_stat[last_suspend_diff_buffer_index];
	case SYS_RES_LAST:
		return &sys_res_stat[last_buffer_index];
	}
	return &sys_res_stat[scene];
}

static uint64_t lpm_sys_res_get_stat_detail(struct sys_res_record *stat, int op, unsigned int val)
{
	uint64_t ret = 0;
	uint64_t total_time = 0, sig_time = 0;

	if (!stat)
		return 0;

	switch (op) {
	case SYS_RES_OP_DURATION:
		ret = stat->spm_res_sig_stats_ptr->duration_time;
		break;
	case SYS_RES_OP_SUSPEND_TIME:
		ret = stat->spm_res_sig_stats_ptr->suspend_time;
		break;
	case SYS_RES_OP_SIG_TIME:
		if (val >= stat->spm_res_sig_stats_ptr->res_sig_num)
			return 0;
		ret = stat->spm_res_sig_stats_ptr->res_sig_tbl[val].time;
		break;
	case SYS_RES_OP_SIG_ID:
		if (val >= stat->spm_res_sig_stats_ptr->res_sig_num)
			return 0;
		ret = stat->spm_res_sig_stats_ptr->res_sig_tbl[val].sig_id;
		break;
	case SYS_RES_OP_SIG_GROUP_ID:
		if (val >= stat->spm_res_sig_stats_ptr->res_sig_num)
			return 0;
		ret = stat->spm_res_sig_stats_ptr->res_sig_tbl[val].grp_id;
		break;
	case SYS_RES_OP_SIG_OVERALL_RATIO:
		if (val >= stat->spm_res_sig_stats_ptr->res_sig_num)
			return 0;
		total_time = stat->spm_res_sig_stats_ptr->duration_time;
		sig_time = stat->spm_res_sig_stats_ptr->res_sig_tbl[val].time;
		ret = sig_time < total_time ?
			(sig_time * 100) / total_time : 100;
		break;
	case SYS_RES_OP_SIG_SUSPEND_RATIO:
		if (val >= stat->spm_res_sig_stats_ptr->res_sig_num)
			return 0;
		total_time = stat->spm_res_sig_stats_ptr->suspend_time;
		sig_time = stat->spm_res_sig_stats_ptr->res_sig_tbl[val].time;
		ret = sig_time < total_time ?
			(sig_time * 100) / total_time : 100;
		break;
	case SYS_RES_OP_SIG_ADDR:
		if (val >= stat->spm_res_sig_stats_ptr->res_sig_num)
			return 0;
		ret = (uint64_t)(&stat->spm_res_sig_stats_ptr->res_sig_tbl[val]);
		break;
	default:
		break;
	};
	return ret;
}

static unsigned int  lpm_sys_res_get_res_threshold(void)
{
	return group_info[0].threshold;
}

static void lpm_sys_res_set_res_threshold(unsigned int val)
{
	int i;

	if (val > 100)
		val = 100;

	for (i = 0; i < SYS_MAIN_RES_NUM; i++)
		group_info[i].threshold = val;
}

static void lpm_sys_res_common_log_enable(int en)
{
	sys_res_common_log_enable = (en)? 1 : 0;
}

static int lpm_sys_res_log_enable_get(void)
{
	return sys_res_common_log_enable;
}

static void lpm_sys_res_stat_log(unsigned int scene)
{
	#define LOG_BUF_OUT_SZ		(768)

	unsigned long flag;
	struct sys_res_record *sys_res_stat;
	uint64_t time, sys_index, sig_tbl_index;
	uint64_t threshold, ratio;
	int time_type, ratio_type;
	char scene_name[15];
	char sys_res_log_buf[LOG_BUF_OUT_SZ] = { 0 };
	int i, j = 0, sys_res_log_size = 0, sys_res_update = 0;

	if (scene == SYS_RES_LAST &&
	    !sys_res_common_log_enable)
		return;

	sys_res_update = update_lpm_sys_res_stat();

	if (sys_res_update) {
		pr_info("[name:spm&][SPM] SWPM data not update[%d]\n", sys_res_update);
		return;
	}

	spin_lock_irqsave(&_lpm_sys_res_lock, flag);

	sys_res_stat = get_lpm_sys_res_stat(scene);
	if (scene == SYS_RES_LAST_SUSPEND) {
		time_type = SYS_RES_OP_SUSPEND_TIME;
		strscpy(scene_name, "suspend", 10);
		ratio_type = SYS_RES_OP_SIG_SUSPEND_RATIO;
	} else if (scene == SYS_RES_LAST) {
		time_type = SYS_RES_OP_DURATION;
		strscpy(scene_name, "common", 10);
		ratio_type = SYS_RES_OP_SIG_OVERALL_RATIO;
	} else {
		spin_unlock_irqrestore(&_lpm_sys_res_lock, flag);
		return;
	}

	time = lpm_sys_res_get_stat_detail(sys_res_stat, time_type, 0);

	/* suspend may be aborted, no need to print log */
	if (time == 0) {
		sys_res_log_size += scnprintf(
			sys_res_log_buf + sys_res_log_size,
			LOG_BUF_OUT_SZ - sys_res_log_size,
			"[name:spm&][SPM] suspend aborted skip sys_res log\n");
		goto SKIP_LOG;
	}

	sys_res_log_size += scnprintf(
				sys_res_log_buf + sys_res_log_size,
				LOG_BUF_OUT_SZ - sys_res_log_size,
				"[name:spm&][SPM] %s %llu ms; ", scene_name, time);

	for (i = 0; i < SYS_MAIN_RES_NUM; i++){
		sys_index = group_info[i].sys_index;
		if (!sys_index || sys_index == NON_RES_SIG_GROUP)
			continue;

		sig_tbl_index = group_info[i].sig_table_index;
		threshold = group_info[i].threshold;
		ratio = lpm_sys_res_get_stat_detail(sys_res_stat,
					       ratio_type,
					       sys_index);

		sys_res_log_size += scnprintf(
				sys_res_log_buf + sys_res_log_size,
				LOG_BUF_OUT_SZ - sys_res_log_size,
				"group %d ratio %llu", i, ratio);

		if (ratio < threshold) {
			sys_res_log_size += scnprintf(
					sys_res_log_buf + sys_res_log_size,
					LOG_BUF_OUT_SZ - sys_res_log_size,
					"; ");
			continue;
		}

		sys_res_log_size += scnprintf(
				sys_res_log_buf + sys_res_log_size,
				LOG_BUF_OUT_SZ - sys_res_log_size,
				" (> %llu) [", threshold);

		for (j = 0; j < group_info[i].group_num; j++) {
			ratio = lpm_sys_res_get_stat_detail(sys_res_stat,
						       ratio_type,
						       j + sig_tbl_index);

			if (i == SYS_MAIN_RES_PWR_OFF) {
				if ((100 - ratio) < threshold)
					continue;
			} else {
				if (ratio < threshold)
					continue;
			}

			if (sys_res_log_size > LOG_BUF_OUT_SZ - 45) {
				pr_info("[name:spm&][SPM] %s", sys_res_log_buf);
				sys_res_log_size = 0;
			}

			sys_res_log_size += scnprintf(
				sys_res_log_buf + sys_res_log_size,
				LOG_BUF_OUT_SZ - sys_res_log_size,
				"0x%llx(%llu%%),",
				lpm_sys_res_get_stat_detail(sys_res_stat,
					SYS_RES_OP_SIG_ID, j + sig_tbl_index),
				ratio);
		}

		sys_res_log_size += scnprintf(
				sys_res_log_buf + sys_res_log_size,
				LOG_BUF_OUT_SZ - sys_res_log_size,
				"]; ");
	}
SKIP_LOG:
	pr_info("[name:spm&][SPM] %s", sys_res_log_buf);
	spin_unlock_irqrestore(&_lpm_sys_res_lock, flag);
}

static int lpm_sys_res_get_subsys_name(struct sys_res_mapping **map, unsigned int *size)
{
	unsigned int res_mapping_len;

	if (!map || !size)
		return -1;

	res_mapping_len = sizeof(subsys_mapping) / sizeof(struct sys_res_mapping);

	*size = res_mapping_len;
	*map = (struct sys_res_mapping *)&subsys_mapping;

	return 0;
}

static struct lpm_sys_res_ops lpm_sys_res_stat_ops = {
	.get = get_lpm_sys_res_stat,
	.update = update_lpm_sys_res_stat,
	.get_detail = lpm_sys_res_get_stat_detail,
	.get_threshold = lpm_sys_res_get_res_threshold,
	.set_threshold = lpm_sys_res_set_res_threshold,
	.enable_common_log = lpm_sys_res_common_log_enable,
	.get_log_enable = lpm_sys_res_log_enable_get,
	.log = lpm_sys_res_stat_log,
	.lock = &_lpm_sys_res_lock,
	.get_id_name = lpm_sys_res_get_subsys_name,
};

int lpm_sys_res_stat_init(void)
{
	int i, j, ret = -1;
	unsigned int subsys_mapping_len;

	subsys_mapping_len = sizeof(subsys_mapping) / sizeof(struct sys_res_mapping);
	for(i=0; i<subsys_mapping_len; i++) {
		ret = get_res_group_id(i, 0, 0, &subsys_mapping[i].id, NULL, NULL);
		if (ret) {
			pr_info("[name:spm&][SPM] subsys_mapping init fail\n");
			return ret;
		}
	}

	group_info = kcalloc(SYS_MAIN_RES_NUM, sizeof(struct sys_res_group_info), GFP_KERNEL);
	if (!group_info)
		return ret;

	for(i=0; i<SYS_MAIN_RES_NUM; i++) {
		ret = get_res_group_info(i,
					 &group_info[i].sys_index,
					 &group_info[i].sig_table_index,
					 &group_info[i].group_num
					);
		if (ret)
			continue;

		group_info[i].threshold = DEFAULT_RES_THRESHOLD;
		switch(i) {
		case SYS_MAIN_RES_PWR_OFF:
			group_info[i].sys_index = group_info[SYS_MAIN_RES_VCORE].sys_index;
			break;
		case SYS_MAIN_RES_PWR_ACT:
			group_info[i].sys_index = group_info[SYS_MAIN_RES_VCORE].sys_index;
			break;
		default:
			break;
		}
	}


	for (i = 0; i < SYS_RES_SCENE_NUM; i++) {
		ret = lpm_sys_res_stat_alloc(&sys_res_stat[i]);
		if(ret) {
			for (j = i - 1; j >= 0; j--)
				lpm_sys_res_stat_free(&sys_res_stat[j]);
			pr_info("[name:spm&][SPM] sys_res alloc fail\n");
			goto SYS_RES_STAT_ALLOC_FAIL;
		}
	}

	last_buffer_index = SYS_RES_SCENE_LAST_SYNC;
	temp_buffer_index = SYS_RES_SCENE_TEMP;

	last_suspend_diff_buffer_index = SYS_RES_SCENE_LAST_SUSPEND_DIFF;
	last_diff_buffer_index = SYS_RES_SCENE_LAST_DIFF;

	spin_lock_init(&_lpm_sys_res_lock);

	ret = register_lpm_sys_res_ops(&lpm_sys_res_stat_ops);
	if (ret) {
		pr_debug("[name:spm&][SPM] Failed to register LPM sys_res operations.\n");
		goto SYS_PLAT_INIT_ERROR;
	}

	return 0;

SYS_PLAT_INIT_ERROR:
	for (i=0; i<SYS_RES_SCENE_NUM; i++)
		lpm_sys_res_stat_free(&sys_res_stat[i]);

SYS_RES_STAT_ALLOC_FAIL:
	kfree(group_info);
	return ret;
}
EXPORT_SYMBOL(lpm_sys_res_stat_init);

void lpm_sys_res_stat_deinit(void)
{
	int i;

	for (i = 0; i < SYS_RES_SCENE_NUM; i++)
		lpm_sys_res_stat_free(&sys_res_stat[i]);

	unregister_lpm_sys_res_ops();
	pr_info("%s %d: finish", __func__, __LINE__);
}
EXPORT_SYMBOL(lpm_sys_res_stat_deinit);
