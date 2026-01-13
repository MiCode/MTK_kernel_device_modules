#include <linux/delay.h>
#include <linux/init.h>
#include <linux/reboot.h>
#include <asm/unaligned.h>
#include <ufs/ufs.h>
#include "../xiaomi/include/ufshcd.h"
#include "ufsfeature-check.h"
#include "../../../misc/hqsysfs/hqsys_pcba.h"
#include "ufshcd-priv.h"

#define UFSF_QUERY_DESC_GEOMETRY_MAX_SIZE	0x59
#define UFSF_QUERY_DESC_CONFIGURAION_MAX_SIZE	0xE6
#define UFSF_QUERY_DESC_UNIT_MAX_SIZE		0x2D
#define QUERY_ATTR_IDN_SUP_VENDOR_OPTIONS		0xFF
#define UFSF_QUERY_DESC_DEVICE_MAX_SIZE		0x5F
#define LI_EN_32(x)				be32_to_cpu(*(__be32 *)(x))
#define UFS_UPIU_MAX_GENERAL_LUN		8
#define seq_scan_lu(lun) for (lun = 0; lun < UFS_UPIU_MAX_GENERAL_LUN; lun++)
#define ERR_MSG(msg, args...)		pr_err("%s:%d err: " msg "\n", \
					       __func__, __LINE__, ##args)

void fill_wb_gb(struct ufs_hba *hba, unsigned int segsize, unsigned int unitsize, unsigned int rawval, struct check_wb_t *check)
{
	int unit;
	if (!hba) {
		pr_err("HBA is null.\n");
		return;
	}
	unit = (segsize * unitsize) >> 1;
	check->wb_gb = (rawval * unit) >> 20;
}

static int ufsf_read_desc(struct ufs_hba *hba, u8 desc_id, u8 desc_index,
			  u8 selector, u8 *desc_buf, u32 size)
{
	int err = 0;
	pm_runtime_get_sync(hba->dev);
	err = ufshcd_query_descriptor_retry(hba, UPIU_QUERY_OPCODE_READ_DESC,
					    desc_id, desc_index,
					    selector,
					    desc_buf, &size);
	if (err)
		ERR_MSG("reading Device Desc failed. err = %d", err);
	pm_runtime_put_sync(hba->dev);
	return err;
}

static int ufsf_read_geo_desc(struct ufs_hba *hba, u8 selector, struct ufstw_check_info *tw_dev_info,
	struct check_wb_t *check)
{
	u8 geo_buf[UFSF_QUERY_DESC_GEOMETRY_MAX_SIZE];
	u64 total_size;
	int ret = 0;

	ret = ufsf_read_desc(hba, QUERY_DESC_IDN_GEOMETRY, 0, selector,
			     geo_buf, UFSF_QUERY_DESC_GEOMETRY_MAX_SIZE);

	if (ret)
		return ret;

	total_size = get_unaligned_be64(&geo_buf[0x04]);
	check->total_gb = total_size >> 21;
	tw_dev_info->seg_size = LI_EN_32(&geo_buf[GEOMETRY_DESC_PARAM_SEG_SIZE]);
	tw_dev_info->unit_size = geo_buf[GEOMETRY_DESC_PARAM_ALLOC_UNIT_SIZE];

	return 0;
}

static void mi_ufshcd_check_provision_config(void)
{
	struct PCBA_MSG *pcba_msg = get_pcba_msg();
	if (!pcba_msg)
		return;
	if (pcba_msg->pcba_stage < P1) {
		pr_info("%s: pcba_stage = %d !\n", __func__, pcba_msg->pcba_stage);
	} else {
		ERR_MSG("%s: phone is not enable write booster feature, factory image can not turn on!\n",
			__func__);
		ERR_MSG("%s: If you have any question, please contact memory group!\n", __func__);
		msleep(500);
		BUG_ON(1);
	}
}

int check_wb_size(struct check_wb_t *check)
{
	int ret = -1;
	int total_gb[] = {64, 128, 256, 512};
	int wb_gb[] = {6, 12, 24, 48};
	int i;
	int total_size_f = 0;
	if (check->total_gb > 512 && check->total_gb <= 1024) {
		total_size_f = 1024;
	} else if (check->total_gb > 256) {
		total_size_f = 512;
	} else if (check->total_gb > 128) {
		total_size_f = 256;
	} else if (check->total_gb > 64) {
		total_size_f = 128;
	} else if (check->total_gb > 32) {
		total_size_f = 64;
	} else if (check->total_gb > 16) {
		total_size_f = 32;
	} else if (check->total_gb > 8) {
		total_size_f = 16;
	} else {
		pr_info("ufs total size unknown:%dGB\n", check->total_gb);
		return ret;
	}
	pr_info("ufs total:%dGB wb:%dGB\n",
		total_size_f, check->wb_gb);
	for (i = 0; i < ARRAY_SIZE(total_gb); i++) {
		if (total_gb[i] == total_size_f) {
			if (wb_gb[i] == check->wb_gb)
				return i;
		}
	}
	return -1;
}

static void check_tw_provision(struct ufs_hba *hba, struct check_wb_t *check)
{
	if (ufshcd_is_wb_allowed(hba)) {
		if (check_wb_size(check) == -1) {
			mi_ufshcd_check_provision_config();
		}
	} else {
			ERR_MSG("%s: UFSHCD_CAP_WB_EN fail!\n", __func__);
			msleep(500);
			BUG_ON(1);
	}
}

static void ufsf_read_unit_desc(struct ufs_hba *hba, int lun, u8 selector, struct ufstw_check_info *tw_dev_info, struct check_wb_t *check)
{
	u8 unit_buf[UFSF_QUERY_DESC_UNIT_MAX_SIZE];
	int ret = 0;

	ret = ufsf_read_desc(hba, QUERY_DESC_IDN_UNIT, lun, selector,
			unit_buf, UFSF_QUERY_DESC_UNIT_MAX_SIZE);

	if (ret) {
		ERR_MSG("read unit desc failed. ret (%d)", ret);
		goto out;
	}

	tw_dev_info->tw_lun_buf_size = LI_EN_32(&unit_buf[UNIT_DESC_PARAM_WB_BUF_ALLOC_UNITS]);
	fill_wb_gb(hba, tw_dev_info->seg_size, tw_dev_info->unit_size, tw_dev_info->tw_lun_buf_size, check);
	if (lun == 2)
		check_tw_provision(hba, check);

out:
	return;
}

void ufs_tw_check(struct ufs_hba *hba)
{
	int ret=0, lun = 0;
	u8 selector = 0;
	u8 device_buf[UFSF_QUERY_DESC_DEVICE_MAX_SIZE];
	struct ufstw_check_info *tw_dev_info;
	struct check_wb_t *check;

	tw_dev_info = kzalloc(sizeof(struct ufstw_check_info), GFP_KERNEL);
	if (!tw_dev_info) {
		ERR_MSG("%s:%d Memory allocation failed for check.\n", __func__, __LINE__);
		goto out;
	}

	check = kzalloc(sizeof(struct check_wb_t), GFP_KERNEL);
	if (!check) {
		ERR_MSG("%s:%d Memory allocation failed for check.\n", __func__, __LINE__);
		kfree(tw_dev_info);
		goto out;
	}

	ret = ufsf_read_geo_desc(hba, selector, tw_dev_info, check);

	if (ret) {
		ERR_MSG("%s: get_geo_desc fail! ret (%d)\n", __func__, ret);
		goto out;
        }

	ret = ufsf_read_desc(hba, QUERY_DESC_IDN_DEVICE, 0, selector,
			     device_buf, UFSF_QUERY_DESC_DEVICE_MAX_SIZE);

	if (ret) {
		ERR_MSG("read device desc failed. ret (%d)", ret);
		goto out;
	}

	if (device_buf[DEVICE_DESC_PARAM_WB_TYPE]) {
		tw_dev_info->tw_lun_buf_size = LI_EN_32(&device_buf[DEVICE_DESC_PARAM_WB_SHARED_ALLOC_UNITS]);
		fill_wb_gb(hba, tw_dev_info->seg_size, tw_dev_info->unit_size, tw_dev_info->tw_lun_buf_size, check);
		check_tw_provision(hba, check);
	} else {
		seq_scan_lu(lun)
			ufsf_read_unit_desc(hba, lun, selector, tw_dev_info, check);
	}

	kfree(tw_dev_info);
	kfree(check);
	return;

out:
	if(ret) {
		kfree(tw_dev_info);
		kfree(check);
	}

	return;
}
