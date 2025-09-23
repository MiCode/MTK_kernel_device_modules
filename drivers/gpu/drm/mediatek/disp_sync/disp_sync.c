// SPDX-License-Identifier: GPL-2.0

/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/io.h>

#include "disp_sync.h"
#include "mtk_sync.h"
#include "mtk_log.h"

#define DISP_SYNC_IOC_MAGIC	'Y'

#define DISP_SYNC_IOC_FENCE_CREATE	_IOWR(DISP_SYNC_IOC_MAGIC, 0,\
		struct drm_mtk_gem_submit)
#define DISP_SYNC_IOC_FENCE_RELEASE	_IOWR(DISP_SYNC_IOC_MAGIC, 1,\
		struct drm_mtk_gem_submit)
#define DISP_SYNC_IOC_WAIT_VSYNC	_IOWR(DISP_SYNC_IOC_MAGIC, 2,\
		struct disp_sync_vblank_reply)


#define MAX_SESSION_COUNT 4

#define MTK_SESSION_MODE(id) (((id) >> 24) & 0xff)
#define MTK_SESSION_TYPE(id) (((id) >> 16) & 0xff)
#define MTK_SESSION_DEV(id) ((id)&0xff)
#define MAKE_MTK_SESSION(type, dev) ((unsigned int)((type) << 16 | (dev)))

#define DISP_REG_VSYNC_INTEN 0x00
#define DISP_REG_VSYNC_INTSTA 0x0C
#define DISP_VSYNC_INT_MSK (1 << 1)

static dev_t disp_sync_dev_no;
static struct cdev *disp_sync_dev;
static struct class *my_class;
struct mtk_disp_sync *disp_sync;


unsigned long vsync_cnt;

static struct mtk_fence_session_sync_info _mtk_fence_context[MAX_SESSION_COUNT];

static LIST_HEAD(info_pool_head);
static DEFINE_MUTEX(_disp_fence_mutex);
static DEFINE_MUTEX(fence_buffer_mutex);

static irqreturn_t mtk_disp_sync_irq_handler(int irq, void *dev_id)
{
	struct mtk_disp_sync *disp = dev_id;
	unsigned int val = 0;
	int ret = 0;

	val = readl(disp->regs + DISP_REG_VSYNC_INTSTA);
	if (!val) {
		ret = IRQ_HANDLED;
		return ret;
	}
	DDPINFO("handle display sync irq :0x%x\n", val);
	writel(~val, disp->regs + DISP_REG_VSYNC_INTSTA);

	if (val & DISP_VSYNC_INT_MSK) {
		DDPINFO("handle display irq:%d\n", (unsigned int) (ktime_get() - disp->vblank_time));
		vsync_cnt++;
		disp->vblank_time = ktime_get();
		atomic_set(&disp->vblank_irq, 1);
		wake_up_interruptible(&(disp->vblank_irq_wq));
	}
	ret = IRQ_HANDLED;
	return ret;
}

static int mtk_fence_init(void)
{
	int i = 0;
	struct mtk_fence_session_sync_info *session_info = NULL;

	memset((void *)&_mtk_fence_context, 0, sizeof(_mtk_fence_context));
	/* for (i = 0; i < ARRAY_SIZE(_mtk_fence_context) /
	 * sizeof(_mtk_fence_context[0]); i++) {
	 */
	for (i = 0; i < ARRAY_SIZE(_mtk_fence_context); i++) { /* rogerhsu */
		session_info = &_mtk_fence_context[i];
		session_info->session_id = 0xffffffff;
	}

	return 0;
}

static struct mtk_fence_buf_info *mtk_init_buf_info(struct mtk_fence_buf_info *buf)
{
	INIT_LIST_HEAD(&buf->list);
	buf->fence = MTK_INVALID_FENCE_FD;
	buf->client = NULL;
	buf->hnd = NULL;
	buf->idx = 0;
	buf->mva = 0;
	buf->layer_type = 0;

	return buf;
}

static struct mtk_fence_buf_info *mtk_get_buf_info(void)
{
	struct mtk_fence_buf_info *info;

	/* we must use another mutex for buffer list because it will be operated
	 * by ALL layer info.
	 */
	mutex_lock(&fence_buffer_mutex);
	if (!list_empty(&info_pool_head)) {
		info = list_first_entry(&info_pool_head,
					struct mtk_fence_buf_info, list);
		list_del_init(&info->list);
		mtk_init_buf_info(info);
	} else {
		info = kzalloc(sizeof(struct mtk_fence_buf_info), GFP_KERNEL);
		mtk_init_buf_info(info);
		DDPINFO("create new mtk_fence_buf_info node %p\n", info);
	}
	mutex_unlock(&fence_buffer_mutex);

	return info;
}


static char *mtk_fence_session_mode_spy(unsigned int session_id)
{
	switch (MTK_SESSION_TYPE(session_id)) {
	case MTK_SESSION_V_PRIMARY:
		return "Virt-P";
	case MTK_SESSION_V_DYNAMIC_INTERNAL:
		return "Virt-DI";
	case MTK_SESSION_V_DYNAMIC_EX1:
		return "Virt-EX1";
	case MTK_SESSION_V_DYNAMIC_EX2:
		return "Virt-EX2";
	case MTK_SESSION_V_DYNAMIC_EX3:
		return "Virt-EX3";
	default:
		return "NA";
	}
}

static struct mtk_fence_session_sync_info *
_get_session_sync_info(unsigned int session_id)
{
	int i = 0;
	int j = 0;
	struct mtk_fence_session_sync_info *session_info = NULL;
	struct mtk_fence_info *layer_info = NULL;
	char name[32];

	mutex_lock(&_disp_fence_mutex);
	for (i = 0; i < ARRAY_SIZE(_mtk_fence_context); i++) {
		if (session_id == _mtk_fence_context[i].session_id) {
			session_info = &(_mtk_fence_context[i]);
			goto done;
		}
	}

	for (i = 0; i < ARRAY_SIZE(_mtk_fence_context); i++) {
		if (_mtk_fence_context[i].session_id == 0xffffffff) {
			DDPERR(
				"not found session info for session_id:0x%08x,insert %p to array index:%d\n",
				session_id, &(_mtk_fence_context[i]), i);
			_mtk_fence_context[i].session_id = session_id;
			session_info = &(_mtk_fence_context[i]);

			sprintf(name, "%s%d_prepare",
				mtk_fence_session_mode_spy(session_id),
				MTK_SESSION_DEV(session_id));
			sprintf(name, "%s%d_frame_cfg",
				mtk_fence_session_mode_spy(session_id),
				MTK_SESSION_DEV(session_id));
			sprintf(name, "%s%d_wait_fence",
				mtk_fence_session_mode_spy(session_id),
				MTK_SESSION_DEV(session_id));
			sprintf(name, "%s%d_setinput",
				mtk_fence_session_mode_spy(session_id),
				MTK_SESSION_DEV(session_id));
			sprintf(name, "%s%d_setoutput",
				mtk_fence_session_mode_spy(session_id),
				MTK_SESSION_DEV(session_id));
			sprintf(name, "%s%d_trigger",
				mtk_fence_session_mode_spy(session_id),
				MTK_SESSION_DEV(session_id));
			sprintf(name, "%s%d_findidx",
				mtk_fence_session_mode_spy(session_id),
				MTK_SESSION_DEV(session_id));
			sprintf(name, "%s%d_release",
				mtk_fence_session_mode_spy(session_id),
				MTK_SESSION_DEV(session_id));
			sprintf(name, "%s%d_waitvsync",
				mtk_fence_session_mode_spy(session_id),
				MTK_SESSION_DEV(session_id));
			sprintf(name, "%s%d_err",
				mtk_fence_session_mode_spy(session_id),
				MTK_SESSION_DEV(session_id));

			for (j = 0;
			     j < (sizeof(session_info->session_layer_info) /
				  sizeof(session_info->session_layer_info[0]));
			     j++) {

				if (MTK_SESSION_TYPE(session_id) ==
				    MTK_SESSION_V_PRIMARY)
					sprintf(name, "-VP_%d_%d-",
						MTK_SESSION_DEV(session_id), j);
				else if (MTK_SESSION_TYPE(session_id) ==
				    MTK_SESSION_V_DYNAMIC_INTERNAL)
					sprintf(name, "-VDI_%d_%d-",
						MTK_SESSION_DEV(session_id), j);
				else
					sprintf(name, "-VNA_%d_%d-",
						MTK_SESSION_DEV(session_id), j);

				layer_info =
					&(session_info->session_layer_info[j]);
				mutex_init(&(layer_info->sync_lock));
				layer_info->layer_id = j;
				layer_info->fence_idx = 0;
				layer_info->timeline_idx = 0;
				layer_info->inc = 0;
				layer_info->cur_idx = 0;
				layer_info->inited = 1;
				layer_info->timeline =
					mtk_sync_timeline_create(name);
				if (layer_info->timeline) {
					DDPINFO("create timeline success\n");
					DDPINFO("%s=%p, layer_info=%p\n",
						name, layer_info->timeline,
						layer_info);
				}

				INIT_LIST_HEAD(&layer_info->buf_list);
			}

			goto done;
		}
	}

done:

	if (session_info == NULL)
		DDPERR("wrong session_id:%d, 0x%08x\n", session_id,
			  session_id);

	mutex_unlock(&_disp_fence_mutex);
	return session_info;
}


static struct mtk_fence_info *_disp_sync_get_sync_info(unsigned int session_id,
						unsigned int timeline_id)
{
	struct mtk_fence_info *layer_info = NULL;
	struct mtk_fence_session_sync_info *session_info =
		_get_session_sync_info(session_id);

	mutex_lock(&_disp_fence_mutex);
	if (session_info) {
		if (timeline_id >=
		    sizeof(session_info->session_layer_info) /
			    sizeof(session_info->session_layer_info[0])) {

			DDPERR("invalid timeline_id:%d\n", timeline_id);
			goto done;
		} else {
			layer_info = &(
				session_info->session_layer_info[timeline_id]);
		}
	}

	if (layer_info == NULL || session_info == NULL) {
		DDPINFO(
			"can't get sync info for session_id:0x%08x, timeline_id:%d\n",
			session_id, timeline_id);
		goto done;
	}

	if (layer_info->inited == 0) {
		DDPERR("layer_info[%d] not inited\n", timeline_id);
		goto done;
	}

done:
	mutex_unlock(&_disp_fence_mutex);
	return layer_info;
}

static struct mtk_fence_buf_info *mtk_fence_prepare_buf(struct drm_mtk_gem_submit *buf)
{
	int ret = 0;
	unsigned int session_id = 0;
	unsigned int timeline_id = 0;
	struct mtk_fence_buf_info *buf_info = NULL;
	struct fence_data data;
	struct mtk_fence_info *layer_info = NULL;
	struct mtk_fence_session_sync_info *session_info = NULL;

	if (buf == NULL) {
		DDPERR("Prepare Buffer, buf is NULL!!\n");
		return NULL;
	}

	session_id = buf->session_id;
	timeline_id = buf->layer_id;
	session_info = _get_session_sync_info(session_id);
	layer_info = _disp_sync_get_sync_info(session_id, timeline_id);

	if (layer_info == NULL) {
		DDPERR("%s:%d layer_info is null\n", __func__, __LINE__);
		return NULL;
	}

	if (layer_info->inited == 0) {
		DDPERR(
			"FATAL ERROR, sync info not inited, session_id=0x%08x|layer_id=%d\n",
			session_id, timeline_id);
		return NULL;
	}

	buf_info = mtk_get_buf_info();
	mutex_lock(&layer_info->sync_lock);
	data.fence = MTK_INVALID_FENCE_FD;
	data.value = ++(layer_info->fence_idx);
	mutex_unlock(&(layer_info->sync_lock));

	snprintf(data.name, sizeof(data.name), "disp-S%x-L%d-%d", session_id,
		 timeline_id, data.value);
	ret = mtk_sync_fence_create(layer_info->timeline, &data);
	if (ret != 0) {
		/* Does this really happened? */
		DDPERR("%s%d,layer%d create Fence Object failed ret=%d!\n",
			  mtk_fence_session_mode_spy(session_id),
			  MTK_SESSION_DEV(session_id), timeline_id, ret);
	}
	buf_info->fence = data.fence;
	buf_info->idx = data.value;

	buf_info->mva_offset = 0;
	buf_info->trigger_ticket = 0;
	buf_info->buf_state = create;
	buf_info->ts_create = sched_clock();
	mutex_lock(&layer_info->sync_lock);
	list_add_tail(&buf_info->list, &layer_info->buf_list);
	mutex_unlock(&layer_info->sync_lock);

	return buf_info;
}

static void mtk_release_fence(unsigned int session_id, unsigned int layer_id,
		       int fence)
{
	struct mtk_fence_buf_info *buf;
	struct mtk_fence_buf_info *n;
	int num_fence = 0;
	int current_timeline_idx = 0;
	struct mtk_fence_info *layer_info = NULL;
	struct mtk_fence_session_sync_info *session_info = NULL;

	session_info = _get_session_sync_info(session_id);
	layer_info = _disp_sync_get_sync_info(session_id, layer_id);

	if (layer_info == NULL) {
		DDPERR("%s:%d layer_info is null\n", __func__, __LINE__);
		return;
	}

	if (layer_info->timeline == NULL)
		return;

	mutex_lock(&layer_info->sync_lock);
	current_timeline_idx = layer_info->timeline_idx;
	num_fence = fence - layer_info->timeline_idx;
	if (num_fence > 0) {
		mtk_sync_timeline_inc(layer_info->timeline, num_fence, 0);
		layer_info->timeline_idx = fence;

		if (num_fence >= 2)
			DDPERR(
				"Warning, R/%s%d/L%d/timeline idx:%d/fence:%d\n",
				mtk_fence_session_mode_spy(session_id),
				MTK_SESSION_DEV(session_id), layer_id,
				current_timeline_idx, fence);
	} else {
		mutex_unlock(&layer_info->sync_lock);
		DDPERR(
			"error, R/%s%d/L%d/timeline idx:%d/fence:%d\n",
			mtk_fence_session_mode_spy(session_id),
			MTK_SESSION_DEV(session_id), layer_id,
			current_timeline_idx, fence);
		return;
	}


	list_for_each_entry_safe(buf, n, &layer_info->buf_list, list) {
		if (buf->idx > fence)
			continue;

		layer_info->fence_fd = buf->fence;

		DDPINFO("R+/%s%d/L%d/id%d/last%d/new%d/idx%d\n",
			 mtk_fence_session_mode_spy(session_id),
			 MTK_SESSION_DEV(session_id), layer_id, fence,
			 current_timeline_idx, layer_info->fence_idx,
			 buf->idx);


		list_del_init(&buf->list);

		/* we must use another mutex for buffer list*/
		/* because it will be operated by ALL layer info.*/

		mutex_lock(&fence_buffer_mutex);
		list_add_tail(&buf->list, &info_pool_head);
		mutex_unlock(&fence_buffer_mutex);
		buf->ts_period_keep = sched_clock() - buf->ts_create;
		/* DDPERR("buf->idx=%d,ts_create=%lld,
		 * ts_period_keep=%lld\n",
		 * buf->idx, buf->ts_create,
		 * buf->ts_period_keep);
		 */
	}
	mutex_unlock(&layer_info->sync_lock);
}

static void mtk_release_layer_fence(unsigned int session_id, unsigned int layer_id,
	unsigned int fence_index)
{
	int fence = 0;

	fence = fence_index;

	DDPINFO("RL+/%s%d/L%d/id%d\n", mtk_fence_session_mode_spy(session_id),
		 MTK_SESSION_DEV(session_id), layer_id, fence);
	/* DDPINFO("layer%d release all fence %d\n", layer_id, fence); */
	mtk_release_fence(session_id, layer_id, fence);
}

static unsigned int mtk_fence_get_present_timeline_id(unsigned int session_id)
{
	if (MTK_SESSION_TYPE(session_id) == MTK_SESSION_V_PRIMARY)
		return MTK_TIMELINE_PRIMARY_PRESENT_TIMELINE_ID;
	if (MTK_SESSION_TYPE(session_id) == MTK_SESSION_V_DYNAMIC_INTERNAL)
		return MTK_TIMELINE_SECONDARY_PRESENT_TIMELINE_ID;
	if (MTK_SESSION_TYPE(session_id) == MTK_SESSION_V_DYNAMIC_EX1)
		return MTK_TIMELINE_THIRD_PRESENT_TIMELINE_ID;
	if (MTK_SESSION_TYPE(session_id) == MTK_SESSION_V_DYNAMIC_EX2)
		return MTK_TIMELINE_FOURTH_PRESENT_TIMELINE_ID;
	if (MTK_SESSION_TYPE(session_id) == MTK_SESSION_V_DYNAMIC_EX3)
		return MTK_TIMELINE_FIFTH_PRESENT_TIMELINE_ID;
	DDPINFO("session id is wrong, session=0x%x!!\n", session_id);
	return MTK_TIMELINE_COUNT;
}

static int mtk_release_present_fence(unsigned int session_id, unsigned int fence_idx)
{
	struct mtk_fence_info *layer_info = NULL;
	unsigned int timeline_id = 0;
	int fence_increment = 0;

	timeline_id = mtk_fence_get_present_timeline_id(session_id);
	if (timeline_id >= MTK_TIMELINE_COUNT) {
		DDPMSG("%s:%d timeline is null\n", __func__, __LINE__);
		return -1;
	}
	layer_info = _disp_sync_get_sync_info(session_id, timeline_id);
	if (layer_info == NULL) {
		DDPERR("%s:%d layer_info is null\n", __func__, __LINE__);
		return -1;
	}

	mutex_lock(&layer_info->sync_lock);

	fence_increment = fence_idx - layer_info->timeline->value;

	if (fence_increment <= 0) {
		DDPMSG("fence_increment as 0, fence_idx %d\n", fence_idx);
		goto done;
	}

	if (fence_increment >= 2)
		DDPMSG("Warning, R/%s%d/L%d/timeline idx:%d/fence:%d\n",
			 mtk_fence_session_mode_spy(session_id),
			 MTK_SESSION_DEV(session_id), timeline_id,
			 layer_info->timeline->value, fence_idx);


	mtk_sync_timeline_inc(layer_info->timeline, fence_increment, 0);
	DDPMSG("RL+/%s%d/L%d/id%d\n",
		 mtk_fence_session_mode_spy(session_id),
		 MTK_SESSION_DEV(session_id), timeline_id, fence_idx);

done:
	mutex_unlock(&layer_info->sync_lock);
	return 0;
}

static long disp_sync_ioctl_create_fence(struct file *file, unsigned long param)
{
	struct drm_mtk_gem_submit args = {0};
	struct mtk_fence_buf_info *buf = NULL;

	if (copy_from_user(&args, (void *)param, sizeof(args))) {
		DDPERR("copy disp_sync from user fail\n");
		return -EINVAL;
	}

	if (args.layer_en && (args.ion_fd >= 0)) {
		buf = mtk_fence_prepare_buf(&args);
		if (buf != NULL) {
			args.fence_fd = buf->fence;
			args.index = buf->idx;
		} else {
			DDPERR("[DISP SYNC] P+ FAIL /%s%d/L%d/e%d/fd%d/id%d/ffd%d\n",
				  mtk_fence_session_mode_spy(args.session_id),
				  MTK_SESSION_DEV(args.session_id),
				  args.layer_id, args.layer_en, args.fb_id,
				  args.index, args.fence_fd);
			args.fence_fd = MTK_INVALID_FENCE_FD; /* invalid fd */
			args.index = 0;
		}
	} else {
		DDPERR("layer_en:%d, ion_fd:%d\n", args.layer_en, args.ion_fd);
		args.fence_fd = MTK_INVALID_FENCE_FD; /* invalid fd */
		args.index = 0;
	}

	DDPINFO("P+ /%s%d/L%d/e%d/fd%d/id%d/ffd%d\n",
		mtk_fence_session_mode_spy(args.session_id),
				  MTK_SESSION_DEV(args.session_id),
				  args.layer_id, args.layer_en, args.fb_id,
				  args.index, args.fence_fd);

	if (copy_to_user((void *)param, &args, sizeof(args))) {
		DDPERR("%s copy_to_user failed\n", __func__);
		return -EFAULT;
	}
	return 0;
}

static long disp_sync_ioctl_release_fence(struct file *file, unsigned long param)
{
	struct drm_mtk_gem_submit args = {0};

	if (copy_from_user(&args, (void *)param, sizeof(args))) {
		DDPERR("copy disp_sync from user fail\n");
		return -EINVAL;
	}

	if (args.type == MTK_PRESENT_FENCE)
		mtk_release_present_fence(args.session_id, args.index);
	else if (args.type == MTK_LAYER_RELEASE_FENCE)
		mtk_release_layer_fence(args.session_id, args.layer_id, args.index);
	else
		DDPERR("invalid fence type\n");

	return 0;
}

static long disp_sync_ioctl_wait_vsync(struct file *file, unsigned long param)
{
	int ret = 0;
	struct disp_sync_vblank_reply *reply = (struct disp_sync_vblank_reply *)param;
	struct timespec64 ts;
	struct disp_sync_vblank_reply kernel_reply;

	atomic_set(&disp_sync->vblank_irq, 0);

	ret = wait_event_interruptible_timeout(disp_sync->vblank_irq_wq,
		atomic_read(&disp_sync->vblank_irq), msecs_to_jiffies(3000));
	if (ret == 0) {
		DDPERR("disp sync wait sync timeout\n");
		return -EFAULT;
	}

	//copy vblank time to user
	kernel_reply.sequence = vsync_cnt;
	ts = ktime_to_timespec64(disp_sync->vblank_time);
	kernel_reply.tval_sec = (u32)ts.tv_sec;
	kernel_reply.tval_usec = ts.tv_nsec / 1000;

	if (reply) {
		ret = copy_to_user((void *)param, &kernel_reply, sizeof(struct disp_sync_vblank_reply));
		if (ret) {
			DDPERR("disp sync wait vsync ioctl fail\n");
			return -EFAULT;
		}
	} else {
		DDPERR("disp sync wait no reply timestamp\n");
	}

	return ret;
}

static long disp_sync_ioctl(struct file *file, unsigned int cmd,
			  unsigned long param)
{
	switch (cmd) {
	case DISP_SYNC_IOC_FENCE_CREATE:
		return disp_sync_ioctl_create_fence(file, param);
	case DISP_SYNC_IOC_FENCE_RELEASE:
		return disp_sync_ioctl_release_fence(file, param);
	case DISP_SYNC_IOC_WAIT_VSYNC:
		return disp_sync_ioctl_wait_vsync(file, param);
	default:
		return -ENOTTY;
	}
}

#ifdef IF_ZERO

static int disp_sync_open(struct inode *inode, struct file *file)
{
	struct mtk_disp_sync *disp_sync = NULL;
	struct cdev *cdev;

	cdev = inode->i_cdev;
	disp_sync = container_of(cdev, struct mtk_disp_sync, cdev);
	if (disp_sync == NULL) {
		DDPERR("%s fail get priv data\n", __func__);
		return -EFAULT;
	}

	file->private_data = disp_sync;

	DDPERR("%s %x\n", __func__, disp_sync);
	return 0;
}
#endif

const struct file_operations disp_sync_fops = {
	.owner	= THIS_MODULE,
//	.open = disp_sync_open,
	.unlocked_ioctl = disp_sync_ioctl,
//	.compat_ioctl	= disp_sync_ioctl,
};

static int  disp_sync_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int ret;
	int irq;
	struct resource *res;

	DDPMSG("%s+\n", __func__);

	disp_sync = devm_kzalloc(dev, sizeof(*disp_sync), GFP_KERNEL);
	if (!disp_sync)
		return -ENOMEM;

	ret = alloc_chrdev_region(&disp_sync_dev_no, 0, 1, "disp_sync");
	if (ret < 0) {
		DDPERR("disp sync probe fail\n");
		return ret;
	}
	disp_sync_dev = cdev_alloc();
	disp_sync_dev->owner = THIS_MODULE;
	disp_sync_dev->ops = &disp_sync_fops;
	ret = cdev_add(disp_sync_dev, disp_sync_dev_no, 1);
	if (ret < 0) {
		DDPERR("disp sync failed to add cdev\n");
		unregister_chrdev_region(disp_sync_dev_no, 1);
		return ret;
	}
	disp_sync->cdev = *disp_sync_dev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	disp_sync->regs_pa = res->start;
	disp_sync->regs = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (IS_ERR(disp_sync->regs)) {
		DDPERR("base devm_ioremap failed:%ld", PTR_ERR(disp_sync->regs));
		return PTR_ERR(disp_sync->regs);
	}

	irq = platform_get_irq(pdev, 0);
	if (irq >= 0) {
		ret = devm_request_irq(dev, irq, mtk_disp_sync_irq_handler,
					   IRQF_TRIGGER_NONE | IRQF_SHARED, dev_name(dev),
					   disp_sync);
		if (ret < 0) {
			DDPERR("%s:%d, failed to request irq:%d ret:%d\n",
					__func__, __LINE__,
					irq, ret);
			return ret;
		}
	} else {
		DDPERR("disp sync no irq\n");
	}


	atomic_set(&disp_sync->vblank_irq, 0);
	init_waitqueue_head(&(disp_sync->vblank_irq_wq));
	platform_set_drvdata(pdev, disp_sync);


	my_class =  class_create(THIS_MODULE, "disp_sync");
	if (IS_ERR(my_class)) {
		cdev_del(disp_sync_dev);
		unregister_chrdev_region(disp_sync_dev_no, 1);
		DDPERR("disp sync class create faile\n");
		return PTR_ERR(my_class);
	}
	device_create(my_class, NULL, disp_sync_dev_no, NULL, "disp_sync");

	mtk_fence_init();

	DDPMSG("%s-\n", __func__);

	return 0;
}

static int  disp_sync_remove(struct platform_device *pdev)
{
	struct mtk_disp_sync *priv_data = platform_get_drvdata(pdev);

	kfree(priv_data);

    // Unregister the character device
	cdev_del(disp_sync_dev);
	unregister_chrdev_region(disp_sync_dev_no, 1);

	return 0;
}

static const struct of_device_id disp_sync_driver_dt_match[] = {
	{.compatible = "mediatek,disp-sync",},
	{},
};

static struct platform_driver disp_sync_driver = {
	.probe = disp_sync_probe,
	.remove = disp_sync_remove,
	.driver = {

			.name = "disp_sync",
			.owner = THIS_MODULE,
			.of_match_table = disp_sync_driver_dt_match,
		},
};

module_platform_driver(disp_sync_driver);

MODULE_AUTHOR("Amy Zhang<amy.zhang@mediatek.com>");
MODULE_DESCRIPTION("Mediatek SoC DISP driver");
MODULE_LICENSE("GPL");
