#ifndef __OCA72XXX_LOG_H__
#define __OCA72XXX_LOG_H__

#include <linux/kernel.h>


/********************************************
 *
 * print information control
 *
 *******************************************/
#define OCA_LOGI(fmt, ...)\
	pr_info("[Oca] %s:" fmt "\n", __func__, ##__VA_ARGS__)

#define OCA_LOGD(fmt, ...)\
	pr_debug("[Oca] %s:" fmt "\n", __func__, ##__VA_ARGS__)

#define OCA_LOGE(fmt, ...)\
	pr_err("[Oca] %s:" fmt "\n", __func__, ##__VA_ARGS__)


#define OCA_DEV_LOGI(dev, fmt, ...)\
	pr_info("[Oca] [%s]%s: " fmt "\n", dev_name(dev), __func__, ##__VA_ARGS__)

#define OCA_DEV_LOGD(dev, fmt, ...)\
	pr_debug("[Oca] [%s]%s: " fmt "\n", dev_name(dev), __func__, ##__VA_ARGS__)

#define OCA_DEV_LOGE(dev, fmt, ...)\
	pr_err("[Oca] [%s]%s: " fmt "\n", dev_name(dev), __func__, ##__VA_ARGS__)



#endif
