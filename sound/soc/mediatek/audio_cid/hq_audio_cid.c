#include <linux/types.h>
#include <linux/notifier.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/firmware.h>
#include <linux/errno.h>

#define	AUDIO_CLASS_NAME        "audio"

/* audio class */
static struct class  audio_class;
static bool audio_status2charger;

BLOCKING_NOTIFIER_HEAD(audio_status_notifier_list);
EXPORT_SYMBOL_GPL(audio_status_notifier_list);
int audio_status_notifier_register_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&audio_status_notifier_list, nb);
}
EXPORT_SYMBOL(audio_status_notifier_register_client);
int audio_status_notifier_unregister_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&audio_status_notifier_list, nb);
}
EXPORT_SYMBOL(audio_status_notifier_unregister_client);
int audio_status_notifier_call_chain(unsigned long val, void *v)
{
	return blocking_notifier_call_chain(&audio_status_notifier_list, val, v);
}
EXPORT_SYMBOL(audio_status_notifier_call_chain);

static ssize_t audio_status_store(struct class *c, struct class_attribute *attr,
		const char *buf, size_t count)
{
	bool val;
	if (kstrtobool(buf, &val)){
		pr_err("set audio status to charger failed!");
		return -EINVAL;
	}
	if (audio_status2charger != val) {
		audio_status2charger = val;
		audio_status_notifier_call_chain(audio_status2charger, NULL);
		pr_info("%s set audio status to [%d]!", __func__, audio_status2charger);
	}
	return count;
}

static ssize_t audio_status_show(struct class *c, struct class_attribute *attr,
		char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", audio_status2charger);
}

static CLASS_ATTR_RW(audio_status);
static struct attribute *audio_class_attrs[] = {
	&class_attr_audio_status.attr,
	NULL,
};

ATTRIBUTE_GROUPS(audio_class);
static struct class audio_class = {
	.name          = AUDIO_CLASS_NAME,
	.owner         = THIS_MODULE,
	.class_groups  = audio_class_groups,
};

static int __init hq_audio_init(void)
{
	return class_register(&audio_class);
}


late_initcall(hq_audio_init);
MODULE_AUTHOR("HQ_Audio");
MODULE_DESCRIPTION("Huaqin Audio Info Driver");
MODULE_LICENSE("GPL");