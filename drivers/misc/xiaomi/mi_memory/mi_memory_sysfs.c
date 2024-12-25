#include <linux/init.h>
#include <linux/export.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/kobject.h>
#include <linux/kdev_t.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/debugfs.h>
#include <linux/proc_fs.h>
#include <linux/completion.h>
#include <ufs/ufshcd.h>
#include "mi_memory_sysfs.h"
#include "ufsctx.h"

static struct memory_info *mi_memory;

extern struct ufs_info_t *g_ufs_info;
extern struct ufs_info_t ufs_info;

static void ufs_info_init_work(struct work_struct *work)
{
	struct ufs_xiaomi_ctx *p_ufs_xiaomi = get_ufs_xiaomi();

	wait_for_completion(&p_ufs_xiaomi->ufslink_comp);
	mi_memory->ufs_info = init_ufs_info();

	return;
}

static int mv_proc_show(struct seq_file *m, void *v)
{
	if (!mi_memory->ddr_info) {
		seq_printf(m, "%s: mi memory dram info not ready!\n", __func__);
	} else {
		seq_printf(m, "D: 0x%02x %d\n", mi_memory->ddr_info->ddr_id, mi_memory->ddr_info->ddr_size);
	}

	if (!mi_memory->ufs_info) {
		seq_printf(m, "%s: mi memory ufs info not ready!\n", __func__);
	} else {
		seq_printf(m, "U: 0x%04x %d %s %s\n", mi_memory->ufs_info->ufs_id, mi_memory->ufs_info->ufs_size,
						mi_memory->ufs_info->ufs_name, mi_memory->ufs_info->ufs_fwver);
	}

	return 0;
}

static int mv_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, mv_proc_show, NULL);
}

static const struct proc_ops memory_procfs_fops = {
	.proc_open	= mv_proc_open,
	.proc_read	= seq_read,
	.proc_lseek	= seq_lseek,
	.proc_release	= single_release,
};

/*
 * Create mi memory sysfs
 */
static const struct attribute_group *memory_sysfs_groups[] = {
	&ufs_sysfs_group,
	&dram_sysfs_group,
	NULL,
};

struct file_operations mem_ops = {
	.owner  = THIS_MODULE,
};

static int __init mi_memory_sysfs_init(void)
{
	int ret = 0;

	mi_memory = kzalloc(sizeof(struct memory_info), GFP_KERNEL);

	mi_memory->mem_class = class_create(THIS_MODULE, MI_MEMORY_CLASS);
	if (IS_ERR(mi_memory->mem_class)) {
		ret = PTR_ERR(mi_memory->mem_class);
		pr_err("%s: mi memory info class creation failed, err = %d\n", __func__, ret);
		goto out;
	}

	mi_memory->major = register_chrdev(MEMORYDEV_MAJOR, MI_MEMORY_MODULE, &mem_ops);
	if (mi_memory->major < 0) {
		ret = mi_memory->major;
		pr_err("%s: mi memory info chrdev creation failed, err = %d\n", __func__, ret);
		goto class_unreg;
	}

	mi_memory->mem_dev = device_create(mi_memory->mem_class, NULL, MKDEV(mi_memory->major, MEMORYDEV_MINOR), NULL, MI_MEMORY_DEVICE);
	if (IS_ERR(mi_memory->mem_dev)) {
		ret = -ENODEV;
		pr_err("%s: mi memory info device creation failed, err = %d\n", __func__, ret);
		goto chrdev_unreg;
	}

	ret = sysfs_create_groups(&mi_memory->mem_dev->kobj, memory_sysfs_groups);
	if (ret) {
		pr_err("%s: sysfs groups creation failed, err = %d\n", __func__, ret);
		goto memdev_unreg;
	}

	proc_create(MV_NAME, 0555, NULL, &memory_procfs_fops);

	mi_memory->ddr_info = init_dram_info();

	/*
	 * Init g_ufs_init earlier to avoid NULL pointer exception when invoking hr early
	 */
	g_ufs_info = &ufs_info;

	INIT_WORK(&mi_memory->ufsinfo_init_wq, ufs_info_init_work);
	schedule_work(&mi_memory->ufsinfo_init_wq);

	debugfs_create_symlink("ufshcd0", NULL, "../../class/mi_memory/mi_memory_device/ufshcd0");

	return ret;

memdev_unreg:
	device_destroy(mi_memory->mem_class, MKDEV(mi_memory->major, MEMORYDEV_MINOR));
chrdev_unreg:
	unregister_chrdev(MEMORYDEV_MAJOR, MI_MEMORY_MODULE);
class_unreg:
	class_destroy(mi_memory->mem_class);
out:
	kfree(mi_memory);
	return ret;
}

static void __exit mi_memory_sysfs_exit(void)
{
	sysfs_remove_groups(&mi_memory->mem_dev->kobj, memory_sysfs_groups);
	device_destroy(mi_memory->mem_class, MKDEV(mi_memory->major, MEMORYDEV_MINOR));
	unregister_chrdev(MEMORYDEV_MAJOR, MI_MEMORY_MODULE);
	class_destroy(mi_memory->mem_class);
	kfree(mi_memory);
}

subsys_initcall(mi_memory_sysfs_init);
module_exit(mi_memory_sysfs_exit);

MODULE_DESCRIPTION("Xiaomi Memory Debug Interface");
MODULE_LICENSE("GPL");
