#include <linux/init.h>
#include <linux/export.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/kobject.h>
#include <linux/kdev_t.h>

static int __init mi_memory_dummy_init(void)
{
	return 0;
}

static void __exit mi_memory_dummy_exit(void)
{
	return;
}

subsys_initcall(mi_memory_dummy_init);
module_exit(mi_memory_dummy_exit);

MODULE_DESCRIPTION("Xiaomi Memory Dummy");
MODULE_LICENSE("GPL");
