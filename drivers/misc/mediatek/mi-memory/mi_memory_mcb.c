#include <linux/mm.h>

#include <linux/printk.h>

#include <linux/string.h>

#include <linux/init.h>
#include <linux/module.h>

int __init  mi_memory_mcb(void){
 return 0;
}

void __exit mi_memory_exit(void)
{
  ;
}
module_init(mi_memory_mcb);
module_exit(mi_memory_exit);
MODULE_LICENSE("GPL");