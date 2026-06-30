// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/delay.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>

#include "mcupm_ipi_id.h"
#include "include/mcupm_internal_driver.h"
#include "include/mcupm_plt.h"
#include "include/mcupm_timesync.h"

/*share memory start address defination*/
#define MCUPM_MBOX_SLOT_NUM			40

static u32 internal_ipidev_number = 0;
static struct mtk_mbox_device *mcupm_mboxdevs = NULL;
struct mtk_ipi_device *mcupm_ipidevs = NULL;
u32 get_mcupms_ipidev_number(void);
u32 get_mcupms_ipidev_number(void)
{
	return internal_ipidev_number;
}
EXPORT_SYMBOL_GPL(get_mcupms_ipidev_number);

static int platform_parse_and_init_mtk_mbox_device(struct platform_device *pdev, struct mtk_mbox_device *mboxdev);
int mcupms_init_ipi_mboxs(struct platform_device *pdev) {
	int ret;
	struct device *dev = &pdev->dev;
	struct platform_device *new_pdev;
	struct device_node *node = NULL;
	u32 counts = 0, mbox_numer = 0;
	char name_buf[20];

	if(!pdev || !dev)
		return -EINVAL;

	for_each_child_of_node(dev->of_node, node) {
		if (of_node_is_type(node, "mcupm_mbox")) {
			mbox_numer++;
		}
		if (mbox_numer >= max_mcupm) {
			return -EINVAL;
		}
	}
	pr_info("[MCUPM] mtk_ipi_init_mboxs: mbox number %d\n", mbox_numer);
	mcupm_mboxdevs = kcalloc(mbox_numer,
				sizeof(struct mtk_mbox_device), GFP_KERNEL);
	mcupm_ipidevs = kcalloc(mbox_numer,
				sizeof(struct mtk_ipi_device), GFP_KERNEL);

	for_each_child_of_node(dev->of_node, node) {
		int enabled;

		ret = of_property_read_u32(node, "enabled", &enabled);
		if(ret || !enabled) {
			pr_info("[MCUPM] skip mbox(%d):%s ret(%d), enabled(%d)", counts, node->full_name, ret, enabled);
			counts++;
			continue;
		}

		new_pdev = of_platform_device_create(node, node->name, NULL);
		if(!new_pdev) {
			of_node_put(node);
			return -ENOMEM;
		}

		ret = platform_parse_and_init_mtk_mbox_device(new_pdev, &mcupm_mboxdevs[counts]);
		if(ret) {
			pr_info("[MCUPM] platform_parse_and_init_mtk_mbox_device failed ret=%d\n", ret);
			return -1;
		}

		snprintf(name_buf, sizeof(name_buf), "mcupm_ipidev_%d", counts);
		mcupm_ipidevs[counts].name =  kstrdup_const(name_buf, GFP_KERNEL);
		mcupm_ipidevs[counts].id = IPI_DEV_MCUPM;

		ret = mtk_ipi_device_register(&mcupm_ipidevs[counts], new_pdev, &mcupm_mboxdevs[counts],
						      MCUPM_CHAN_MAX);

		if(ret) {
			pr_info("[MCUPM] mtk_ipi_device_register failed ret=%d\n", ret);
			return -1;
		}

		pr_info("[MCUPM] mtk_ipi_device_register(%d) ret=%d\n", counts, ret);
		counts++;
	}
	internal_ipidev_number = counts;
	

	return 0;
}

static int of_parse_mtk_mbox_pin_send(struct device_node *dn,
																struct mtk_mbox_device *mboxdev) {
/*
 84  struct mtk_mbox_pin_send {
 85 	 unsigned int mbox	   : 4,
 86 				  offset   : 20,
 87 				  send_opt : 2,
 88 				  lock	  : 2;
 89 	 unsigned int msg_size;
 90 	 unsigned int pin_index;    //DTS can configuration, for interrupt offset
 91 	 unsigned int chan_id;
 92 	 struct mutex mutex_send;     //init in mtk_ipi_device_register()
 93 	 struct completion comp_ack;  //init in mtk_ipi_device_register()
 94 	 spinlock_t pin_lock;         //init in mtk_ipi_device_register()
 95 	 struct mtk_mbox_send_record send_record;
 96  };
 97
 			// id, mbox_id, send_size, offset 
			send-table =
				<0 0 4 0>,
				<1 1 8 40>,
				<2 2 3 80>,
				<3 3 6 120>;
 */
 enum {
  		send_cells_num = 4
};
	struct mtk_mbox_pin_send *pin_send_table;
	u32 ret, send_count, mbox_id, offset;


	if(!dn || !mboxdev)
		return -EINVAL;

	send_count = of_property_count_u32_elems(
				dn, "send-table")
				/ send_cells_num;

	mboxdev->send_count = send_count;

	pin_send_table = kcalloc(mboxdev->send_count,
			sizeof(struct mtk_mbox_pin_send), GFP_KERNEL);

	mboxdev->pin_send_table = pin_send_table;

	for(int i = 0; i < mboxdev->send_count; i++) {
		//pin_send_table[i].chan_id = i; IPI id
		ret = of_property_read_u32_index(dn,
  				"send-table",
  				i * send_cells_num,
  				&pin_send_table[i].chan_id);
		if (ret) {
			pr_info("[MCUPM]%s:Cannot get ipi id (%d):%d\n", __func__, i,__LINE__);
  			return ret;
  		}

		ret = of_property_read_u32_index(dn,
  				"send-table",
  				i * send_cells_num + 1,
  				&mbox_id);
  		if (ret) {
			pr_info("[MCUPM] %s:Cannot get mbox id (%d):%d\n", __func__, i, __LINE__);
  			return ret;
  		}
		/* because mbox is a bit-field */
		pin_send_table[i].mbox = mbox_id;
		pin_send_table[i].pin_index = mbox_id;

		//pin_send_table[i].msg_size = PIN_S_MSG_SIZE_PLATFORM;
		ret = of_property_read_u32_index(dn,
  				"send-table",
  				i * send_cells_num + 2,
  				&pin_send_table[i].msg_size);
  		if (ret) {
			pr_info("[MCUPM] %s:Cannot get msg size (%d):%d\n", __func__, i, __LINE__);
  			return ret;
  		}

		//pin_send_table[i].offset = 0;
		ret = of_property_read_u32_index(dn,
  				"send-table",
  				i * send_cells_num + 3,
  				&offset);
  		if (ret) {
			pr_info("[MCUPM] %s:Cannot get offset (%d):%d\n", __func__, i, __LINE__);
  			return ret;
  		}
		pin_send_table[i].offset = offset;
		pin_send_table[i].send_opt = MBOX_OPT_SMEM;

	}
	return 0;

}

static int of_parse_mtk_mbox_pin_recv(struct device_node *dn,
																struct mtk_mbox_device *mboxdev) {
/*
 117  struct mtk_mbox_pin_recv { 
 118	 unsigned int mbox	   : 4,   //Should same as minfo->id
 119				  offset   : 20,
 120				  recv_opt	   : 2,
 121				  lock		   : 2,
 122				  buf_full_opt : 2,
 123				  cb_ctx_opt   : 2;
 124	 unsigned int msg_size;
 125	 unsigned int pin_index;
 126	 unsigned int chan_id;       // init while create. IPI id
 127	 struct completion notify;   //init in mtk_ipi_device_register()
 128	 mbox_pin_cb_t mbox_pin_cb;
 129	 void *pin_buf;
 130	 void *prdata;
 131	 spinlock_t pin_lock;        //init in mtk_ipi_device_register()
 132	 struct mtk_mbox_recv_record recv_record;
 133	 atomic_t polling_lock;
 134  };

// id, mbox_id, recv_size, recv_opt, cb_ctx_opt, offset 
 recv-table =
	 <0 0 1 0 0 20>,
	 <1 1 8 0 0 60>,
	 <2 2 1 0 0 100>,
	 <3 3 1 0 0 140>;


 */
enum {
  		recv_cells_num = 6
};
	struct mtk_mbox_pin_recv *pin_recv_table;
	u32 ret, recv_count, cb_ctx_opt, mbox_id, recv_opt, offset;

	if(!dn || !mboxdev)
		return -EINVAL;

	recv_count = of_property_count_u32_elems(
				dn, "recv-table")
				/ recv_cells_num;

	mboxdev->recv_count = recv_count;
	pin_recv_table = kcalloc(mboxdev->recv_count,
  			sizeof(struct mtk_mbox_pin_recv), GFP_KERNEL);

	mboxdev->pin_recv_table = pin_recv_table;

	for(int i = 0; i < mboxdev->recv_count; i++) {
		ret = of_property_read_u32_index(dn,
  				"recv-table",
  				i * recv_cells_num,
  				&pin_recv_table[i].chan_id);
		if (ret) {
			pr_info("[MCUPM]%s:Cannot get ipi id (%d):%d\n", __func__, i,__LINE__);
  			return ret;
  		}

		ret = of_property_read_u32_index(dn,
  				"recv-table",
  				i * recv_cells_num + 1,
  				&mbox_id);
  		if (ret) {
			pr_info("[MCUPM] %s:Cannot get mbox id (%d):%d\n", __func__, i, __LINE__);
  			return ret;
  		}
		/* because mbox and recv_opt is a bit-field */
		pin_recv_table[i].mbox = mbox_id;
		pin_recv_table[i].pin_index = mbox_id;

 		ret = of_property_read_u32_index(dn,
 				"recv-table",
 				i * recv_cells_num + 2,
 				&pin_recv_table[i].msg_size);
 		if (ret) {
			pr_info("[MCUPM]%s:Cannot get msg size (%d):%d\n", __func__, i,
 						__LINE__);
 			return ret;
 		}

		//pin_recv_table[i].recv_opt = MBOX_RECV_MESSAGE;
 		ret = of_property_read_u32_index(dn,
 				"recv-table",
 				i * recv_cells_num + 3,
 				&recv_opt);
 		if (ret) {
			pr_info("[MCUPM]%s:Cannot get recv opt (%d):%d\n", __func__, i,
 						__LINE__);
 			return ret;
 		}
		pin_recv_table[i].recv_opt = recv_opt;

		//pin_recv_table[i].cb_ctx_opt = MBOX_CB_IN_ISR;
		ret = of_property_read_u32_index(dn,
				"recv-table",
				i * recv_cells_num + 4,
				&cb_ctx_opt);
		if (ret) {
			pr_info("[MCUPM]%s:Cannot get cb_ctx_opt (%d):%d\n", __func__, i,
						__LINE__);
			return ret;
		}
		pin_recv_table[i].cb_ctx_opt = cb_ctx_opt;
	
		//pin_recv_table[i].offset = PIN_S_SIZE; unit 4byte
		ret = of_property_read_u32_index(dn,
				"recv-table",
				i * recv_cells_num + 5,
				&offset);
		if (ret) {
			pr_info("[MCUPM]%s:Cannot get offset (%d):%d\n", __func__, i,
						__LINE__);
			return ret;
		}
		pin_recv_table[i].offset = offset;

		pin_recv_table[i].buf_full_opt = MBOX_BUF_FULL_DROP;
	}

	return 0;

}

static int platform_parse_mtk_mbox_info(struct platform_device *pdev,
																struct mtk_mbox_device *mboxdev) {
 	int ret;
	struct resource mbox_res[5];
	void __iomem *mbox_iomap[5];
	struct mtk_mbox_info* mbox_info = NULL;
	struct device *dev = &pdev->dev;

	if(!pdev || !mboxdev)
		return -EINVAL;

	for(int i = 0; i < 5; i++) {
		ret = of_address_to_resource(dev->of_node, i, &mbox_res[i]);
		if(ret) {
			pr_info("[MCUPM] %s:%s get mbox(%d) res fail ret=%d\n", __func__, dev->of_node->full_name, i, ret);
			return ret;
		}
		mbox_iomap[i] = of_iomap(dev->of_node, i);
		if (!mbox_iomap[i]) {
			pr_info("%s() can't find iomem(%d) for %s\n", __func__, i, dev->of_node->full_name);
			return -ENOMEM;
		}
	}

	/* Assign info_table */
	mboxdev->count = platform_irq_count(pdev);
	mbox_info = kcalloc(mboxdev->count,
  			sizeof(struct mtk_mbox_info), GFP_KERNEL);
	mboxdev->info_table = mbox_info;

	pr_info("[MCUPM] platform_parse_mtk_mbox_info, mdev_name=%s mboxdev->count=%d size=%d\n", mboxdev->name, mboxdev->count, (int)resource_size(&mbox_res[0]));
	/* Remap mbox TCM */
	for(int pin_id = 0; pin_id < mboxdev->count; pin_id++) {
		mbox_info[pin_id].slot = MCUPM_MBOX_SLOT_NUM;
		mbox_info[pin_id].opt = MBOX_OPT_SMEM;

		/* Register remaind MBOX ISR */
		ret = mtk_smem_init(pdev, mboxdev, pin_id,
							mbox_iomap[0] + (pin_id * MCUPM_MBOX_SLOT_NUM * MCUPM_MBOX_SLOT_SIZE),
							mbox_iomap[1],
							mbox_iomap[2],
							mbox_iomap[3],
							mbox_iomap[4]);
		if(ret) {
			pr_info("[MCUPM] %s initial mbox pin(%d) smem fail ret=%d\n", __func__, pin_id, ret);
			return ret;
		}
	}

	return 0;
}
void memorycpy_to_tiny(void __iomem *dest, const void *src, int size) {

	int i;
	u32 __iomem *t = dest;
	const u32 *s = src;

	pr_info("[MCUPM] memorycpy_to_tiny dest=%p, src=%p, size=%d\n",dest, src, size);	
	for (i = 0; i < ((size + 3) >> 2); i++)
		*t++ = *s++;

	return;
}
void memorycpy_from_tiny(void __iomem *dest, const void *src, int size) {
	int i;
	u32 *t = dest;
	const u32 __iomem *s = src;
	
	pr_info("[MCUPM] memorycpy_from_tiny dest=%p, src=%p, size=%d, val=0x%x\n",dest, src, size, *s);
	for (i = 0; i < ((size + 3) >> 2); i++)
		*t++ = *s++;

	return;
}

static int platform_parse_and_init_mtk_mbox_device(struct platform_device *pdev, struct mtk_mbox_device *mboxdev) {
	u32 ret;
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;

	if(!pdev || !mboxdev)
		return -EINVAL;

	memset(mboxdev, 0, sizeof(struct mtk_mbox_device));

	mboxdev->name = kstrdup_const(node->name, GFP_ATOMIC);
	//Parse mbox info
	pr_info("[MCUPM] %s Initialize mbox info table\n", node->full_name);
	ret = platform_parse_mtk_mbox_info(pdev, mboxdev);
	if(ret) {
		pr_info("[MCUPM] platform_parse_mtk_mbox_info failed ret=%d\n",ret);
		return -1;
	}

	//Initialize recv table
	pr_info("[MCUPM] %s Initialize recv table\n", node->full_name);
	ret = of_parse_mtk_mbox_pin_recv(dev->of_node, mboxdev);
	if(ret) {
		pr_info("[MCUPM] of_parse_mtk_mbox_pin_recv failed ret=%d\n",ret);
		return -1;
	}

	//Initialize send table
	pr_info("[MCUPM] %s Initialize send table\n", node->full_name);
	ret = of_parse_mtk_mbox_pin_send(dev->of_node, mboxdev);
	if(ret) {
		pr_info("[MCUPM] of_parse_mtk_mbox_pin_send failed ret=%d\n",ret);
		return -1;
	}
	//mboxdev->memcpy_to_tiny	= memorycpy_to_tiny;
	//mboxdev->memcpy_from_tiny = memorycpy_from_tiny;
	//mtk_mbox_dump_all(mboxdev);

	return 0;
}
