#include <linux/miscdevice.h>
#include <linux/dma-mapping.h>
#include <linux/poll.h>
#include "xiaomi_touch_common.h"

#define KNOCK_RAW_DATA_SIZE     PAGE_SIZE
#define KNOCK_NODE_NAME         "xiaomi-touch-knock"
#define KNOCK_TAG               "knock_info"

struct knock_data {
    struct miscdevice misc_dev;
    unsigned int *raw_data;
    int raw_data_length;
    dma_addr_t phy_base;
    wait_queue_head_t wait_data_complete_queue_head;
    int need_frame_count;
    int current_frame_count;
    bool has_data_update;
    void (*need_frame_count_change_listener)(int);
};

static struct knock_data knock_data = {
    .raw_data = NULL,
    .raw_data_length = 0,
    .need_frame_count = 0,
    .current_frame_count = 0,
    .has_data_update = false,
    .need_frame_count_change_listener = NULL,
};

/**
 * Notify poll thread to read data
 */
void knock_data_notify(void) {
    wake_up_interruptible(&(knock_data.wait_data_complete_queue_head));
}
EXPORT_SYMBOL_GPL(knock_data_notify);

/**
 * Update knock data, and notify poll thread to read it.
 *
 * This function should be called by device driver.
 */
void update_knock_data(u8 *buf, int size, int frame_id) {
#if 0  /* add info head and tail for data */
    int offset = 0;

    memcpy((unsigned char *)knock_data.raw_data + offset, &frame_id, sizeof(short));
    offset += sizeof(short);

    memcpy((unsigned char *)knock_data.raw_data + offset, &size, sizeof(short));
    offset += sizeof(short);

    memcpy((unsigned char *)knock_data.raw_data + offset, (unsigned char *)buf, size);
    offset += size;

    memcpy((unsigned char *)knock_data.raw_data + offset, &frame_id, sizeof(short));
#else  /* only report raw data */
    memcpy((unsigned char *)knock_data.raw_data, (unsigned char *)buf, size);
#endif
    knock_data.raw_data_length = size;
    knock_data.has_data_update = true;

    pr_info("%s: %s frame id %d, size is %d\n", KNOCK_TAG, __func__, frame_id, size);
}
EXPORT_SYMBOL_GPL(update_knock_data);

/**
 * Register listener to frame count change.
 * This function will be called when user space write a new frame count
 */
void register_frame_count_change_listener(void *listener) {
    knock_data.need_frame_count_change_listener = listener;
    pr_info("%s: %s register need frame count change listener\n", KNOCK_TAG, __func__);
}
EXPORT_SYMBOL_GPL(register_frame_count_change_listener);

static ssize_t knock_data_read(struct file *file, char __user *buf, size_t count, loff_t *pos)
{
    char temp_buf[5];
    int result = 0;
    int n = 0;

    if (*pos)
        return 0;

    n = snprintf(temp_buf, 5, "%d\n", knock_data.need_frame_count);
    if ((result = copy_to_user(buf, temp_buf, n))) {
		return -EFAULT;
	}

    *pos += 1;

	return n;
}

static ssize_t knock_data_write(struct file *file, const char __user *buf, size_t count, loff_t *pos)
{
    char temp_buf[6];
    int frame_count = 0;
    int result = 0;
    int size = count < 5 ? count : 5;

    result = copy_from_user(temp_buf, buf, size);
    if (result) {
        frame_count = 0;
        pr_err("%s: %s copy from user failed!\n", KNOCK_TAG, __func__);
        goto end_set_value;
    }
    temp_buf[size] = '\0';

    result = sscanf(temp_buf, "%d", &frame_count);
    if (result < 0) {
        frame_count = 0;
        pr_err("%s: %s scanf knock frame count failed!\n", KNOCK_TAG, __func__);
    }
end_set_value:
    knock_data.need_frame_count = frame_count;
    pr_info("%s: %s set knock frame count %d!\n", KNOCK_TAG, __func__, knock_data.need_frame_count);

    if (knock_data.need_frame_count_change_listener) {
        knock_data.need_frame_count_change_listener(knock_data.need_frame_count);
    }

    /* send frame count to thp */
    thp_send_cmd_to_hal(THP_KNOCK_FRAME_COUNT, knock_data.need_frame_count);

	return count;
}

static long knock_data_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    if (!cmd) { /* cmd = 0: notify data has update */
        knock_data.has_data_update = true;
        knock_data_notify();
        /* pr_info("%s: %s notify read data from ioctl!\n", KNOCK_TAG, __func__); */
    }
    return 0;
}

static unsigned int knock_data_poll(struct file *file, poll_table *wait)
{
	unsigned int mask = 0;

    poll_wait(file, &(knock_data.wait_data_complete_queue_head), wait);

    if (knock_data.has_data_update) {
        mask |= POLLIN | POLLRDNORM;
        knock_data.has_data_update = false;
        /* pr_info("%s: %s has knock data need read!\n", KNOCK_TAG, __func__); */
    } else {
        /* pr_err("%s: %s has empty knock data need read!\n", KNOCK_TAG, __func__); */
    }
    /* pr_info("%s: %s mask is %d!\n", KNOCK_TAG, __func__, mask); */
    return mask;
}


static int knock_data_mmap(struct file *file, struct vm_area_struct *vma)
{
	unsigned long start = vma->vm_start;
	unsigned long size = vma->vm_end - vma->vm_start;
	unsigned long offset = vma->vm_pgoff << PAGE_SHIFT;
	unsigned long page;
	unsigned long pos;

	if (!knock_data.raw_data) {
		pr_err("%s: %s invalid memory\n", KNOCK_TAG, __func__);
		return -ENOMEM;
	}

	pos = (unsigned long)knock_data.phy_base + offset;
	page = pos >> PAGE_SHIFT;

	if(remap_pfn_range(vma, start, page, size, PAGE_SHARED)) {
		return -EAGAIN;
	} else {
		pr_info( "%s: %s remap_pfn_range %u, size:%ld, success\n", KNOCK_TAG, __func__, (unsigned int)page, size);
	}
	return 0;
}


static const struct file_operations xiaomi_knock_fops = {
	.owner  = THIS_MODULE,
	.poll   = knock_data_poll,
	.mmap   = knock_data_mmap,
    .read   = knock_data_read,
    .write  = knock_data_write,
    .unlocked_ioctl  = knock_data_ioctl,
};

/**
 * Knock data get init
 */
int knock_node_init(void)
{
    int result = 0;

    pr_err("%s: %s: enter!\n", KNOCK_TAG, __func__);

    init_waitqueue_head(&(knock_data.wait_data_complete_queue_head));

    if (!knock_data.raw_data) {
        knock_data.raw_data = (unsigned int *)kzalloc(KNOCK_RAW_DATA_SIZE, GFP_KERNEL);
        if (!knock_data.raw_data) {
            result = -1;
            pr_err("%s: %s: alloc memory for mmap failed!\n", KNOCK_TAG, __func__);
            goto error_alloc_memory;
        }
        knock_data.phy_base = virt_to_phys(knock_data.raw_data);
        pr_info("%s: %s: raw data addr:%lud, phy addr:%lud\n", KNOCK_TAG, __func__,
            (unsigned long)knock_data.raw_data, (unsigned long)knock_data.phy_base);
    }

    knock_data.misc_dev.minor = MISC_DYNAMIC_MINOR;
	knock_data.misc_dev.name = KNOCK_NODE_NAME;
	knock_data.misc_dev.fops = &xiaomi_knock_fops;
	knock_data.misc_dev.parent = NULL;
    result = misc_register(&(knock_data.misc_dev));
    if (result) {
        pr_info("%s: %s: register %s node failed\n", KNOCK_TAG, __func__, KNOCK_NODE_NAME);
        goto error_register_node;
    }
    pr_err("%s: %s: complete!\n", KNOCK_TAG, __func__);
    return 0;

error_register_node:
    if (knock_data.raw_data) {
        kfree(knock_data.raw_data);
        knock_data.raw_data = NULL;
    }

error_alloc_memory:
    pr_err("%s: %s: failed!\n", KNOCK_TAG, __func__);
    return result;

}

/**
 * Knock data release
 */
void knock_node_release(void)
{
    if (knock_data.raw_data) {
        kfree(knock_data.raw_data);
        knock_data.raw_data = NULL;
    }
    misc_deregister(&(knock_data.misc_dev));
}

