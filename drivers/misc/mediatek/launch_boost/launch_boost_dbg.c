// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2024-2024 XRing Technologies Co., Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/stat.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>
#include <linux/device.h>
#include <linux/seq_file.h>
#include <linux/proc_fs.h>

#include "mi_lb_def.h"
#include "launch_boost_debug.h"
#include "launch_boost_com.h"

struct lb_owner debug_owner;

static int lb_collect_show(struct seq_file *m, void *v)
{
    unsigned int app_index = 0;
    struct package_info *package_list = NULL;

    mutex_lock(&g_manager->package_list_lock);
    if (list_empty(&g_manager->package_infos)) {
		seq_puts(m, "g_cold_record_list_user0 is empty\n");
		goto unlock;
	}

    seq_printf(m, "total memory size %lu app_num: %u\n",
                    g_manager->total_memory , g_manager->app_num );
    list_for_each_entry(package_list, &g_manager->package_infos, app_list) {
        seq_printf(m, "[%d] app:%s, uid:%d, file num: %d, file size: %u, hit rate: %d%%, mm size: %d\n",
                            app_index++,
                            package_list->owner.package_name,
                            package_list->owner.uid,
                            package_list->files,
                            package_list->total_startup_bytes,
                            package_list->hit_rate,
                            package_list->mm_size);
    }
unlock:
	mutex_unlock(&g_manager->package_list_lock);
    return 0;
}


static int lb_collect_file_open(struct inode *inode, struct file *file)
{
	return single_open(file, lb_collect_show, NULL);
}

static const struct proc_ops lb_collect_file_fops = {
	.proc_open	= lb_collect_file_open,
	.proc_read	= seq_read,
	.proc_lseek	= seq_lseek,
	.proc_release	= single_release,
};


static int mi_lb_create_proc(void)
{
	struct proc_dir_entry *lb_dir, *lb_collect;

	lb_dir = proc_mkdir("iorap_node", NULL);
	if (!lb_dir) {
		MI_LB_ERR("proc_mkdir failed\n");
		return -ENOMEM;
	}

	lb_collect = proc_create("collect", 0, lb_dir, &lb_collect_file_fops);
	if (!lb_collect) {
		remove_proc_entry("iorap_node", NULL);
		MI_LB_ERR("proc_create lb_collect failed\n");
		return -ENOMEM;
	}

	return 0;

}

int mi_lb_dbg_init(void)
{
	int error;

	error = mi_lb_create_proc();
	if (error)
		MI_LB_ERR("lb_create_proc failed\n");

	return error;
}

int g_lb_msg_level = 3;
module_param_named(lb_debug, g_lb_msg_level, int, 0644);
MODULE_PARM_DESC(lb_debug, "iorap debug msg level");

bool g_only_collect = 0;
module_param_named(lb_only_collect, g_only_collect, bool, 0644);
MODULE_PARM_DESC(lb_only_collect, "iorap only collect");

bool g_atrace_enable = 0;
module_param_named(lb_atrace_enable, g_atrace_enable, bool, 0644);
MODULE_PARM_DESC(lb_atrace_enable, "iorap atrace enable");

bool g_collect_specify_filetype = 0;
module_param_named(lb_collect_specify_filetype, g_collect_specify_filetype, bool, 0644);
MODULE_PARM_DESC(lb_collect_specify_filetype, "iorap only collect specify filetype enable");

bool g_collect_pinfile = 1;
module_param_named(lb_collect_pinfile, g_collect_pinfile, bool, 0644);
MODULE_PARM_DESC(lb_collect_pinfile, "iorap collect pinfile");

bool g_debug_verbose = 0;
module_param_named(lb_debug_verbose, g_debug_verbose, bool, 0644);
MODULE_PARM_DESC(lb_debug_verbose, "iorap debug verbose");

 