// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 xiaomi inc.
 */

#include <linux/blkdev.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/printk.h>
#include <linux/spinlock.h>
#include <linux/reboot.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/efi.h>
#include <linux/rcupdate.h>
#include <linux/of.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_host.h>
#include <linux/completion.h>
#include "../../scsi/sd.h"
#include "charger_partition.h"

// extern struct completion ufs_xiaomi_comp;

struct ChargerPartition {
	struct scsi_device *sdev;
        struct delayed_work charger_partition_work;

	int part_info_part_number;
	char *part_info_part_name;

	bool is_charger_partition_rdy;
};
static struct ChargerPartition *charger_partition;
static void* rw_buf;

typedef struct part {
	sector_t part_start;
	sector_t part_size;
} partinfo;
static partinfo part_info = { 0 };

static int charger_scsi_read_partition(struct scsi_device *sdev, void *buf, uint32_t lba, uint32_t blocks)
{
	uint8_t cdb[16];
	int ret = 0;
	struct scsi_sense_hdr sshdr = {};
	const struct scsi_exec_args exec_args = {
		.sshdr = &sshdr,
	};
	unsigned long flags = 0;

	spin_lock_irqsave(sdev->host->host_lock, flags);
	ret = scsi_device_get(sdev);
	if (!ret && !scsi_device_online(sdev)) {
		ret = -ENODEV;
		scsi_device_put(sdev);
		pr_err("%s get device fail\n", __func__);
	}
	spin_unlock_irqrestore(sdev->host->host_lock, flags);
	if (ret) {
		return ret;
        }
	sdev->host->eh_noresume = 1;

	// Fill in the CDB with SCSI command structure
	memset (cdb, 0, sizeof(cdb));
	cdb[0] = READ_10;				// Command
	cdb[1] = 0;
	cdb[2] = (lba >> 24) & 0xff;	// LBA
 	cdb[3] = (lba >> 16) & 0xff;
 	cdb[4] = (lba >> 8) & 0xff;
	cdb[5] = (lba) & 0xff;
	cdb[6] = 0;						// Group Number
	cdb[7] = (blocks >> 8) & 0xff;	// Transfer Len
	cdb[8] = (blocks) & 0xff;
	cdb[9] = 0;						// Control
	ret = scsi_execute_cmd(sdev, cdb, REQ_OP_DRV_IN, buf, (blocks * PART_BLOCK_SIZE), msecs_to_jiffies(15000), 3, &exec_args);
	if (ret) {
		pr_err("%s read error %d\n", __func__, ret);
	}
	scsi_device_put(sdev);
	sdev->host->eh_noresume = 0;
	return ret;
}

static int charger_scsi_write_partition(struct scsi_device *sdev, void *buf, uint32_t lba, uint32_t blocks)
{
	uint8_t cdb[16];
	int ret = 0;
	struct scsi_sense_hdr sshdr = {};
	const struct scsi_exec_args exec_args = {
		.sshdr = &sshdr,
	};
	unsigned long flags = 0;

	spin_lock_irqsave(sdev->host->host_lock, flags);
	ret = scsi_device_get(sdev);
	if (!ret && !scsi_device_online(sdev)) {
		ret = -ENODEV;
		scsi_device_put(sdev);
		pr_err("%s get device fail\n", __func__);
	}
	spin_unlock_irqrestore(sdev->host->host_lock, flags);

	if (ret) {
		return ret;
        }
	sdev->host->eh_noresume = 1;

	// Fill in the CDB with SCSI command structure
	memset (cdb, 0, sizeof(cdb));
	cdb[0] = WRITE_10;				// Command
	cdb[1] = 0;
	cdb[2] = (lba >> 24) & 0xff;	// LBA
	cdb[3] = (lba >> 16) & 0xff;
	cdb[4] = (lba >> 8) & 0xff;
	cdb[5] = (lba) & 0xff;
	cdb[6] = 0;						// Group Number
	cdb[7] = (blocks >> 8) & 0xff;	// Transfer Len
	cdb[8] = (blocks) & 0xff;
	cdb[9] = 0;					// Control
	ret = scsi_execute_cmd(sdev, cdb, REQ_OP_DRV_OUT, buf, (blocks * PART_BLOCK_SIZE), msecs_to_jiffies(15000), 3, &exec_args);
	if (ret) {
		pr_err("%s write error %d\n", __func__, ret);
	}
	scsi_device_put(sdev);
	sdev->host->eh_noresume = 0;
	return ret;
}

int charger_partition_alloc(u8 charger_partition_host_type, u8 charger_partition_info_type, uint32_t size)
{
	int ret = 0;

	if(!charger_partition->is_charger_partition_rdy) {
		pr_err("charger partition not rdy, can't do rw!");
		return -1;
	}
	if(charger_partition_host_type >= CHARGER_PARTITION_HOST_LAST) {
		pr_err("charger_partition_host_type not support!");
		return -1;
	}
	if(charger_partition_info_type >= CHARGER_PARTITION_INFO_LAST) {
		pr_err("charger_partition_info_type not support!");
		return -1;
	}
	if(size >= CHARGER_PARTITION_RWSIZE) {
		pr_err("read %u size not support!", size);
		return -1;
	}

	rw_buf = kzalloc(CHARGER_PARTITION_RWSIZE, GFP_KERNEL);
	if(!rw_buf){
		pr_err("malloc buf error!");
		return -1;
	}

	/* check if avaliable */
	ret = charger_scsi_read_partition(charger_partition->sdev, rw_buf, (part_info.part_start + CHARGER_PARTITION_HEADER), CHARGER_PARTITION_RWSIZE / PART_BLOCK_SIZE);
	if (ret) {
		pr_err("%s charger read error %d\n", __func__, ret);
		kfree(rw_buf);
		return -1;
	}
	pr_err("%s avaliable:%u\n", __func__, ((charger_partition_header *)rw_buf)->avaliable);
	if(0 == ((charger_partition_header *)rw_buf)->avaliable) {
		pr_err("%s not avaliable, can't do rw now!\n", __func__);
		kfree(rw_buf);
		return -1;
	}
	((charger_partition_header *)rw_buf)->avaliable = 0;
	pr_err("%s set avaliable:%u\n", __func__, ((charger_partition_header *)rw_buf)->avaliable);
	ret = charger_scsi_write_partition(charger_partition->sdev, (void *)rw_buf, (part_info.part_start + CHARGER_PARTITION_HEADER), CHARGER_PARTITION_RWSIZE / PART_BLOCK_SIZE);
	if (ret) {
		pr_err("%s charger write error %d\n", __func__, ret);
		kfree(rw_buf);
		return -1;
	}

	return ret;
}
EXPORT_SYMBOL(charger_partition_alloc);

int charger_partition_dealloc(u8 charger_partition_host_type, u8 charger_partition_info_type, uint32_t size)
{
	int ret = 0;

	if(!charger_partition->is_charger_partition_rdy) {
		pr_err("charger partition not rdy, can't do rw!");
		return -1;
	}
	if(charger_partition_host_type >= CHARGER_PARTITION_HOST_LAST) {
		pr_err("charger_partition_host_type not support!");
		return -1;
	}
	if(charger_partition_info_type >= CHARGER_PARTITION_INFO_LAST) {
		pr_err("charger_partition_info_type not support!");
		return -1;
	}
	if(size >= CHARGER_PARTITION_RWSIZE) {
		pr_err("read %u size not support!", size);
		return -1;
	}

	memset(rw_buf, 0, CHARGER_PARTITION_RWSIZE);
	ret = charger_scsi_read_partition(charger_partition->sdev, rw_buf, (part_info.part_start + CHARGER_PARTITION_HEADER), CHARGER_PARTITION_RWSIZE / PART_BLOCK_SIZE);
	if (ret) {
		pr_err("%s charger read error %d\n", __func__, ret);
		kfree(rw_buf);
		return -1;
	}
	((charger_partition_header *)rw_buf)->avaliable = 1;
	pr_err("%s set avaliable:%u\n", __func__, ((charger_partition_header *)rw_buf)->avaliable);
	ret = charger_scsi_write_partition(charger_partition->sdev, (void *)rw_buf, (part_info.part_start + CHARGER_PARTITION_HEADER), CHARGER_PARTITION_RWSIZE / PART_BLOCK_SIZE);
	if (ret) {
		pr_err("%s charger write error %d\n", __func__, ret);
		kfree(rw_buf);
		return -1;
	}

	kfree(rw_buf);
	return ret;
}
EXPORT_SYMBOL(charger_partition_dealloc);

void *charger_partition_read(u8 charger_partition_host_type, u8 charger_partition_info_type, uint32_t size)
{
	int ret = 0;

	if(!charger_partition->is_charger_partition_rdy) {
		pr_err("charger partition not rdy, can't do read!");
		return NULL;
	}
	if(!rw_buf) {
		pr_err("rw_buf null, please alloc first!");
		return NULL;
	}
	if(charger_partition_host_type >= CHARGER_PARTITION_HOST_LAST) {
		pr_err("charger_partition_host_type not support!");
		return NULL;
	}
	if(charger_partition_info_type >= CHARGER_PARTITION_INFO_LAST) {
		pr_err("charger_partition_info_type not support!");
		return NULL;
	}
	if(size >= CHARGER_PARTITION_RWSIZE) {
		pr_err("read %u size not support!", size);
		return NULL;
	}

	memset(rw_buf, 0, CHARGER_PARTITION_RWSIZE);
	ret = charger_scsi_read_partition(charger_partition->sdev, rw_buf, (part_info.part_start + charger_partition_info_type), CHARGER_PARTITION_RWSIZE / PART_BLOCK_SIZE);
	if (ret) {
		pr_err("%s charger read error %d\n", __func__, ret);
		return NULL;
	}
	return rw_buf;
}
EXPORT_SYMBOL(charger_partition_read);

int charger_partition_write(u8 charger_partition_host_type, u8 charger_partition_info_type, void *buf, uint32_t size)
{
	int ret = 0;

	if(!charger_partition->is_charger_partition_rdy) {
		pr_err("charger partition not rdy, can't do read!");
		return -1;
	}
	if(!rw_buf) {
		pr_err("rw_buf null, please alloc first!");
		return -1;
	}
	if(charger_partition_host_type >= CHARGER_PARTITION_HOST_LAST) {
		pr_err("charger_partition_host_type not support!");
		return -1;
	}
	if(charger_partition_info_type >= CHARGER_PARTITION_INFO_LAST) {
		pr_err("charger_partition_info_type not support!");
		return -1;
	}
	if(size >= CHARGER_PARTITION_RWSIZE) {
		pr_err("read %u size not support!", size);
		return -1;
	}

	memset(rw_buf, 0, CHARGER_PARTITION_RWSIZE);
	ret = charger_scsi_read_partition(charger_partition->sdev, rw_buf, (part_info.part_start + charger_partition_info_type), CHARGER_PARTITION_RWSIZE / PART_BLOCK_SIZE);
	if (ret) {
		pr_err("%s charger read error %d\n", __func__, ret);
		return -1;
	}
	memcpy(rw_buf, buf, size);
 
	ret = charger_scsi_write_partition(charger_partition->sdev, rw_buf, (part_info.part_start + charger_partition_info_type), CHARGER_PARTITION_RWSIZE / PART_BLOCK_SIZE);
	if (ret) {
		pr_err("%s charger write error %d\n", __func__, ret);
		return -1;
	}

	return ret;
}
EXPORT_SYMBOL(charger_partition_write);

static bool look_up_scsi_device(int lun)
{
	struct Scsi_Host *shost;

	shost = scsi_host_lookup(0);
	if (!shost)
		return false;

	charger_partition->sdev = scsi_device_lookup(shost, 0, 0, lun);
	if (!charger_partition->sdev)
		return false;

	scsi_host_put(shost);
	return true;
}

static bool check_device_is_correct(void)
{
	if (strncmp(charger_partition->sdev->host->hostt->proc_name, UFSHCD, strlen(UFSHCD))) {
                pr_err("%s proc name is not ufshcd, name: %s", __func__, charger_partition->sdev->host->hostt->proc_name);
		return false;
	}

	return true;
}

static bool get_charger_partition_info(void)
{
	struct scsi_disk *sdkp = NULL;
	struct scsi_device *sdev = charger_partition->sdev;
	int part_number = charger_partition->part_info_part_number;
	struct block_device *part;

	if (!sdev->sdev_gendev.driver_data) {
		pr_err("%s scsi disk is null\n", __func__);
		return false;
	}

	sdkp = (struct scsi_disk *)sdev->sdev_gendev.driver_data;
	if (!sdkp->disk) {
		pr_err("%s gendisk is null\n", __func__);
		return false;
	}

	charger_partition->part_info_part_name = sdkp->disk->disk_name;
	pr_err("%s partion: %s\n",
			__func__, charger_partition->part_info_part_name);

	part = xa_load(&sdkp->disk->part_tbl, part_number);
	if (!part) {
		pr_err("%s device is null\n", __func__);
		return false;
	}

	part_info.part_start = part->bd_start_sect * PART_SECTOR_SIZE / PART_BLOCK_SIZE;
	part_info.part_size = bdev_nr_sectors(part) * PART_SECTOR_SIZE / PART_BLOCK_SIZE;

	pr_err("%s partion: %s start %llu(block) size %llu(block)\n",
			__func__, charger_partition->part_info_part_name, part_info.part_start, part_info.part_size);
	return true;
}

int get_hwc_from_dtb(char *hwc) {
	struct device_node *proj_info_node;
	const char *read_hwc;
	char *ptr=NULL;
	int res_len=-1;
	char hwc_word[]="androidboot.hwc=";

	proj_info_node  = of_find_node_by_name(NULL,"chosen");
	if (NULL == proj_info_node) {
		pr_err("%s:device chosen node not exist.",__func__);
		return res_len;
	}
	res_len = of_property_read_string(proj_info_node,"hwinfo",&read_hwc);
	if (res_len != 0) {
		pr_err("%s:device bootargs read fail:res_len=%d.",__func__,res_len);
		return res_len;
	}
	ptr = strstr(read_hwc, hwc_word);
	if (ptr != NULL) {
		strncpy(hwc, ptr+strlen(hwc_word), 9); //copy the result to pointer
		hwc[9] = '\0';
		pr_err("%s, get result %s.\n", __func__,hwc);
		return 0;
	}
	pr_err("%s:strstr get pointer hwc failed.",__func__);
	return -1;
}

int check_charger_partition_header(void)
{
	int ret = 0;
	charger_partition_header *header = NULL;

	header = kzalloc(CHARGER_PARTITION_RWSIZE, GFP_KERNEL);
	if(!header){
		pr_err("malloc buf error!");
		return -1;
	}

	ret = charger_scsi_read_partition(charger_partition->sdev, (void *)header, (part_info.part_start + CHARGER_PARTITION_HEADER), CHARGER_PARTITION_RWSIZE / PART_BLOCK_SIZE);
	if (ret) {
		pr_err("%s charger read error %d\n", __func__, ret);
		kfree(header);
		return -1;
	}

	if(header->magic != CHARGER_PARTITION_MAGIC) {
		header->magic = CHARGER_PARTITION_MAGIC;
	}
	header->initialized = 1;
	header->avaliable = 1;

	ret = charger_scsi_write_partition(charger_partition->sdev, (void *)header, (part_info.part_start + CHARGER_PARTITION_HEADER), CHARGER_PARTITION_RWSIZE / PART_BLOCK_SIZE);
	if (ret) {
		pr_err("%s charger write error %d\n", __func__, ret);
		kfree(header);
		return -1;
	}

	ret = charger_scsi_read_partition(charger_partition->sdev, (void *)header, (part_info.part_start + CHARGER_PARTITION_HEADER), CHARGER_PARTITION_RWSIZE / PART_BLOCK_SIZE);
	if (ret) {
		pr_err("%s charger read error %d\n", __func__, ret);
		kfree(header);
		return -1;
	}

	kfree(header);
	return ret;
}

int get_charger_partition_info_1(void)
{
	int ret = 0;
	charger_partition_info_1 *info_1 = NULL;

	ret = charger_partition_alloc(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_1, sizeof(charger_partition_info_1));
	if(ret < 0) {
		pr_err("%s failed to alloc\n", __func__);
		return -1;
	}

	info_1 = (charger_partition_info_1 *)charger_partition_read(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_1, sizeof(charger_partition_info_1));
	if(!info_1) {
		pr_err("%s failed to read\n", __func__);
		ret = charger_partition_dealloc(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_1, sizeof(charger_partition_info_1));
		if(ret < 0) {
			pr_err("%s failed to dealloc\n", __func__);
			return -1;
		}
		return -1;
	}

	ret = charger_partition_dealloc(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_1, sizeof(charger_partition_info_1));
	if(ret < 0) {
		pr_err("%s failed to dealloc\n", __func__);
		return -1;
	}
	return 0;
}

int set_charger_partition_info_1(void)
{
	int ret = 0;
	charger_partition_info_1 info_1 = {.power_off_mode = 2, .zero_speed_mode = 2, .test = 0x23456789, .reserved = 0};

	ret = charger_partition_alloc(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_1, sizeof(charger_partition_info_1));
	if(ret < 0) {
		pr_err("%s failed to alloc\n", __func__);
		return -1;
	}

	ret = charger_partition_write(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_1, (void *)&info_1, sizeof(charger_partition_info_1));
	if(ret < 0) {
		pr_err("%s failed to write\n", __func__);
		ret = charger_partition_dealloc(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_1, sizeof(charger_partition_info_1));
		if(ret < 0) {
			pr_err("%s failed to dealloc\n", __func__);
			return -1;
		}
		return -1;
	}

	ret = charger_partition_dealloc(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_1, sizeof(charger_partition_info_1));
	if(ret < 0) {
		pr_err("%s failed to dealloc\n", __func__);
		return -1;
	}
	return 0;
}

static void charger_partition_work(struct work_struct *work)
{
        int lun = 0, ret = 0;
	char HWC[16]={0};

	ret = get_hwc_from_dtb(HWC);
	if (!ret) {
		if(!strncmp(HWC, "CN", 2) || !strncmp(HWC, "In", 2))
		{
			charger_partition->part_info_part_number = 84;
		}else if(!strncmp(HWC, "Gl", 2)){
			charger_partition->part_info_part_number = 85;
		}else {
			charger_partition->part_info_part_number = 84;
		}
	}

        for(lun = 0; lun < 6; lun++) {
		if (!look_up_scsi_device(lun)) {
                        pr_err("%s not find, continue...\n", __func__);
			continue;
		}

		if (!check_device_is_correct()){
                        pr_err("%s not find finally, won't read charger partition!!!\n", __func__);
			return;
                }

		if (get_charger_partition_info()) {
			charger_partition->is_charger_partition_rdy = true;
			check_charger_partition_header();
			get_charger_partition_info_1();
			set_charger_partition_info_1();
			get_charger_partition_info_1();
			break;
		}
	}
}

int charger_partition_init(void)
{
	charger_partition = (struct ChargerPartition *)kzalloc(sizeof(struct ChargerPartition), GFP_KERNEL);

	INIT_DELAYED_WORK(&charger_partition->charger_partition_work, charger_partition_work);
	schedule_delayed_work(&charger_partition->charger_partition_work, msecs_to_jiffies(CHARGER_WORK_DELAY_MS));
	return 0;
}
void charger_partition_exit(void)
{
	cancel_delayed_work(&charger_partition->charger_partition_work);
	kfree(charger_partition);
}

