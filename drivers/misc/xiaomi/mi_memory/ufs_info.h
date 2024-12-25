#ifndef __UFS_INFO_H__
#define __UFS_INFO_H__

#include <linux/sysfs.h>
#include <linux/fs.h>
#include <linux/printk.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/nls.h>

#define SD_ASCII_STD true
#define SD_RAW false

#define SCSI_LUN 			0
#define SCSI_LUN_MICRON 		2

enum ufs_desc_def_size {
	QUERY_DESC_DEVICE_DEF_SIZE		= 0x59,
	QUERY_DESC_CONFIGURATION_DEF_SIZE	= 0x90,
	QUERY_DESC_UNIT_DEF_SIZE		= 0x2D,
	QUERY_DESC_INTERCONNECT_DEF_SIZE	= 0x06,
	QUERY_DESC_GEOMETRY_DEF_SIZE		= 0x48,
	QUERY_DESC_POWER_DEF_SIZE		= 0x62,
	QUERY_DESC_HEALTH_DEF_SIZE		= 0x25,
};

enum field_width {
	BYTE	= 1,
	WORD	= 2,
	DWORD   = 4,
};

/* Query request retries */
#define QUERY_REQ_RETRIES 3

/**
 * struct uc_string_id - unicode string
 *
 * @len: size of this descriptor inclusive
 * @type: descriptor type
 * @uc: unicode string character
 */
struct uc_string_id {
	u8 len;
	u8 type;
	wchar_t uc[];
} __packed;


struct desc_field_offset {
	char *name;
	int offset;
	enum field_width width_byte;
};

struct ufs_info_t {
	u16 ufs_id;
	u32 ufs_size;
	u8 *ufs_name;
	u8 *ufs_fwver;
	struct scsi_device *sdev;
	struct ufs_hba *hba;
};

struct ufs_hba *get_ufs_hba_data(void);
struct ufs_info_t *init_ufs_info(void);
u16 get_ufs_id(void);

#endif
