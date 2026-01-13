#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/kernel.h>


static int __init boot_monitor_init(void)
{
	return 0;
}

static void __exit boot_monitor_exit(void)
{
	
}

module_init(boot_monitor_init);
module_exit(boot_monitor_exit);
MODULE_LICENSE("GPL v2");
