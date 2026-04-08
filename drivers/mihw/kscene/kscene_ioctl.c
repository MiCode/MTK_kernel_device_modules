#include "kscene_ioctl.h"
#include "kscene.h"

#define TAG "kscene_ioctl: "

static unsigned long kscene_copy_from_user(void *to,
		const void __user *from, unsigned long ul_bytes)
{
	if (access_ok(from, ul_bytes))
		return __copy_from_user(to, from, ul_bytes);

	return ul_bytes;
}

__attribute__((unused))
static int kscene_copy_package_from_user(struct _KSCENE_IOCTL_PACKAGE *msg,
		unsigned long arg) {
	int ret	= 0;
	struct _KSCENE_IOCTL_PACKAGE *msg_user;

	msg_user = (struct _KSCENE_IOCTL_PACKAGE *)arg;
	if (kscene_copy_from_user(&msg, msg_user,
				sizeof(struct _KSCENE_IOCTL_PACKAGE))) {
		ret = -EFAULT;
	}
	return 0;
}

static long device_ioctl(struct file *filp,
		unsigned int cmd, unsigned long arg)
{
	ssize_t ret = 0;
	struct _KSCENE_IOCTL_FRAME_PACKAGE *msg_frame_user;
	struct _KSCENE_IOCTL_FRAME_PACKAGE msg_frame;
	struct _KSCENE_IOCTL_PACKAGE *msg_user;
	struct _KSCENE_IOCTL_PACKAGE msg;

	if (!kscene_enabled()) {
		return 0;
	}

	switch (cmd) {
		case KSCENE_IOCTL_FRAME:
			if (!kscene_logic_enabled()) {
				return 0;
			}
			msg_frame_user = (struct _KSCENE_IOCTL_FRAME_PACKAGE *)arg;
			if (kscene_copy_from_user(&msg_frame, msg_frame_user,
						sizeof(struct _KSCENE_IOCTL_FRAME_PACKAGE))) {
				ret = -EFAULT;
				return ret;
			}
			ks_dbg("cmd=%d, frame_event=%d, vsynd_id=%lld", cmd, msg_frame.step, msg_frame.vsync_id);
			kscene_notify_frame_event(msg_frame.step, msg_frame.pid, msg_frame.tid, msg_frame.vsync_id, msg_frame.begin);
			break;
		case KSCENE_IOCTL_ACTION:
		case KSCENE_IOCTL_TOUCH:
		case KSCENE_IOCTL_PID:
			msg_user = (struct _KSCENE_IOCTL_PACKAGE *)arg;
			if (kscene_copy_from_user(&msg, msg_user, sizeof(struct _KSCENE_IOCTL_PACKAGE))) {
				ret = -EFAULT;
				return ret;
			}
			ks_dbg("cmd=%d, event=%d, pid=%d, tid=%d, data=%d", cmd, msg.event_id, msg.pid, msg.tid, msg.data);
			kscene_notify_event(cmd, msg.event_id, msg.pid, msg.tid, msg.data);
			break;
		default:
			pr_debug(TAG "%s %d: unknown cmd %x\n",
				__FILE__, __LINE__, cmd);
			break;
	}
	return 0;
}

static int device_show(struct seq_file *m, void *v)
{
	return 0;
}

static int device_open(struct inode *inode, struct file *file)
{
	return single_open(file, device_show, inode->i_private);
}

static const struct proc_ops kscene_ioctl_ops = {
#if IS_ENABLED(CONFIG_COMPAT)
	.proc_compat_ioctl = device_ioctl,
#endif
	.proc_ioctl = device_ioctl,
	.proc_open = device_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

int kscene_init_ioctl(void) {
	struct proc_dir_entry *pe, *parent;
	int ret_val = 0;


	pr_debug(TAG"Start to init kscene ioctl\n");

	parent = proc_mkdir("kscene", NULL);

	pe = proc_create("perf_ioctl", 0777, parent, &kscene_ioctl_ops);
	if (!pe) {
		pr_debug(TAG"%s failed with %d\n",
				"Creating file node ",
				ret_val);
		ret_val = -ENOMEM;
		goto out;
	}

	return 0;

out:
	return ret_val;
}

void kscene_exit_ioctl(void) {
	pr_debug("kscene_exit_ioctl\n");
}
