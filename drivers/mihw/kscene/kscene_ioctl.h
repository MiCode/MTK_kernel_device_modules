#ifndef _KSCENE_IOCTL_H
#define _KSCENE_IOCTL_H
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
//#include <linux/utsname.h>
//#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/moduleparam.h>
#include <linux/uaccess.h>
#include <linux/printk.h>
#include <linux/string.h>

#include "kscene.h"

struct _KSCENE_IOCTL_FRAME_PACKAGE {
	__u32 pid;
	__u32 tid;
	__u32 step;
	__s64 vsync_id;
	__u32 begin;
};

struct _KSCENE_IOCTL_PACKAGE {
	__u32 pid;
	__u32 tid;
	__u32 event_id;
	__s32 data;
};

// frame
#define KSCENE_IOCTL_FRAME						_IOW('g', 1,  struct _KSCENE_IOCTL_FRAME_PACKAGE)

// action
#define KSCENE_IOCTL_ACTION						_IOW('g', 11,  struct _KSCENE_IOCTL_PACKAGE)

// touch
#define KSCENE_IOCTL_TOUCH						_IOW('g', 21,  struct _KSCENE_IOCTL_PACKAGE)

// pid
#define KSCENE_IOCTL_PID						_IOW('g', 31,  struct _KSCENE_IOCTL_PACKAGE)


int kscene_init_ioctl(void);
void kscene_exit_ioctl(void);

#endif