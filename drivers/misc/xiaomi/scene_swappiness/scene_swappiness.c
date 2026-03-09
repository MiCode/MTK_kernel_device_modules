
#define pr_fmt(fmt) "scene_swappiness: " fmt
#include <linux/module.h>
#include <linux/types.h>
#include <trace/hooks/vmscan.h>
#include <linux/swap.h>
#include <linux/proc_fs.h>
#include <linux/mm.h>
static int s_anon_swappiness = 60;
static int h_anon_swappiness = 160;
static int zero_anon_swappiness = 0;
static int free_swap_threshold = 25600;
static struct proc_dir_entry *swappiness_entry;
#define PARA_BUF_LEN 128

static void to_set_swappiness(void *data, int *swappiness)
{
	//小于100M
	if(get_nr_swap_pages() < free_swap_threshold){
		*swappiness = zero_anon_swappiness;
	}else if (current_is_kswapd()) {
		*swappiness = h_anon_swappiness;
	}else{
		*swappiness = s_anon_swappiness;
	}
	return;
}

static int register_scene_swappiness_vendor_hooks(void)
{
	int ret = 0;
	ret = register_trace_android_vh_tune_swappiness(to_set_swappiness, NULL);
	if (ret != 0) {
		pr_err("register_trace_android_vh_set_swappiness failed! ret=%d\n", ret);
		return ret;
	}
	return ret;
}

static void __exit destroy_swappiness_proc(void)
{
	proc_remove(swappiness_entry);
	swappiness_entry = NULL;
}

static void unregister_scene_swappiness_vendor_hooks(void)
{
	unregister_trace_android_vh_tune_swappiness(to_set_swappiness, NULL);
	return;
}

static inline bool check_val(char *buf, char *token, unsigned long *val)
{
	int ret = -EINVAL;
	char *str = strstr(buf, token);
	if (!str)
		return ret;
	ret = kstrtoul(str + strlen(token), 0, val);
	if (ret)
		return -EINVAL;
	if (*val > 200) {
		pr_err("%lu is invalid\n", *val);
		return -EINVAL;
	}
	return 0;
}

static ssize_t swappiness_write(struct file *file,
		const char __user *buff, size_t len, loff_t *ppos)
{
	char kbuf[PARA_BUF_LEN] = {'\0'};
	char *str;
	long val;
	if (len > PARA_BUF_LEN - 1) {
		pr_err("len %lu is too long\n", len);
		return -EINVAL;
	}
	if (copy_from_user(&kbuf, buff, len))
		return -EFAULT;
	kbuf[len] = '\0';
	str = strstrip(kbuf);
	if (!str) {
		pr_err("buff %s is invalid\n", kbuf);
		return -EINVAL;
	}
	if (!check_val(str, "h_anon_swappiness=", &val)) {
		h_anon_swappiness = val;
		return len;
	}
	if (!check_val(str, "s_anon_swappiness=", &val)) {
		s_anon_swappiness = val;
		return len;
	}
	if (!check_val(str, "free_swap_threshold=", &val)) {
		free_swap_threshold = val*2560;
		return len;
	}
	return -EINVAL;
}

static ssize_t swappiness_read(struct file *file,
		char __user *buffer, size_t count, loff_t *off)
{
	char kbuf[PARA_BUF_LEN] = {'\0'};
	int len;
	len = snprintf(kbuf, PARA_BUF_LEN,
			"Tend to recycle file pages,s_anon_swappiness: %d\n", s_anon_swappiness);
	len += snprintf(kbuf + len, PARA_BUF_LEN - len,
			"h_anon_swappiness: %d\n", h_anon_swappiness);
	len += snprintf(kbuf + len, PARA_BUF_LEN - len,
			"free_swap_threshold: %d\n", free_swap_threshold);
	if (len == PARA_BUF_LEN)
		kbuf[len - 1] = '\0';
	if (len > *off)
		len -= *off;
	else
		len = 0;
	if (copy_to_user(buffer, kbuf + *off, (len < count ? len : count)))
		return -EFAULT;
	*off += (len < count ? len : count);
	return (len < count ? len : count);
}

static const struct proc_ops proc_swappiness_ops = {
	.proc_write          = swappiness_write,
	.proc_read		= swappiness_read,
	.proc_lseek		= default_llseek,
};

static int __init create_swappiness_proc(void)
{
	struct proc_dir_entry *root_dir_entry = proc_mkdir("mi_mem_swappiness", NULL);
	swappiness_entry = proc_create((root_dir_entry ?
			"swappiness_para" : "mi_mem_swappiness/swappiness_para"),
			0666, root_dir_entry, &proc_swappiness_ops);
	if (swappiness_entry) {
		printk("Register swappiness interface success.\n");
		return 0;
	}
	pr_err("Register swappiness interface failed TVT.\n");
	return -ENOMEM;
}

static int __init scene_swappiness_init(void)
{
	int ret = 0;
	ret = create_swappiness_proc();
	if (ret)
		return ret;
	ret = register_scene_swappiness_vendor_hooks();
	if (ret != 0) {
		destroy_swappiness_proc();
		return ret;
	}
	pr_info("scene_swappiness_init succeed!\n");
	return 0;
}

static void __exit scene_swappiness_exit(void)
{
	unregister_scene_swappiness_vendor_hooks();
	destroy_swappiness_proc();
	pr_info("scene_swappiness_exit succeed!\n");
	return;
}

module_init(scene_swappiness_init);
module_exit(scene_swappiness_exit);
module_param_named(kswapd_vm_swappiness, h_anon_swappiness, int, S_IRUGO | S_IWUSR);
module_param_named(vm_swappiness, s_anon_swappiness, int, S_IRUGO | S_IWUSR);
MODULE_LICENSE("GPL v2");
