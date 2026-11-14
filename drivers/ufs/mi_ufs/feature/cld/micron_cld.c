#include "../../include/mi_ufshcd.h"
#include "ufscld.h"
#include "../../core/mi_ufsctx.h"

#define GEN11_QUERY_FLAG_IDN_CLD_ENABLE 0x13
#define GEN11_QUERY_ATTR_IDN_CLD_LEVEL 0x31
#define GEN11_QUERY_ATTR_IDN_DEFRAG 0x32
#define GEN12_QUERY_FLAG_IDN_CLD_ENABLE 0x83
#define GEN12_QUERY_ATTR_IDN_CLD_LEVEL 0x81
#define GEN12_QUERY_ATTR_IDN_DEFRAG 0x82
static int query_flag_idn_cld_enable;
static int query_attr_idn_cld_level;
static int query_attr_idn_defrag;
enum MICRON_DEFRAG {
	DEFRAG_IDEL = 0x0,
	DEFRAG_OPERATION_ONGOING = 0x01,
	DEFRAG_OPERATION_STOPPED = 0x02,
	DEFRAG_OPERATION_COMPLETED = 0x03,
};
int micron_init_idn(struct ufscld_dev *cld)
{
	int ret = 0;
	struct ufs_hba *hba = cld->hba;
	if (hba->ufs_version < 0x400) {
		INFO_MSG("MICRON UFS is Gen11");
		query_flag_idn_cld_enable = GEN11_QUERY_FLAG_IDN_CLD_ENABLE;
		query_attr_idn_cld_level = GEN11_QUERY_ATTR_IDN_CLD_LEVEL;
		query_attr_idn_defrag = GEN11_QUERY_ATTR_IDN_DEFRAG;
	} else if (hba->ufs_version >= 0x400){
		INFO_MSG("MICRON UFS is Gen12");
		query_flag_idn_cld_enable = GEN12_QUERY_FLAG_IDN_CLD_ENABLE;
		query_attr_idn_cld_level = GEN12_QUERY_ATTR_IDN_CLD_LEVEL;
		query_attr_idn_defrag = GEN12_QUERY_ATTR_IDN_DEFRAG;
	} else{
			ERR_MSG("Please check MICRON datasheet.");
			ret = -1;
	}
	return ret;
}
int micron_get_frag_level(struct ufscld_dev *cld, int *frag_level)
{
	struct ufs_hba *hba = cld->hba;
	int ret = 0, attr = -1;
	ret = ufshcd_query_attr_retry(hba, UPIU_QUERY_OPCODE_READ_ATTR, (enum attr_idn)query_attr_idn_cld_level, 0, 0, &attr);
	if (ret)
		return ret;
	if (attr == 0) {
		*frag_level = CLD_LEV_CLEAN;
	} else if (attr == 1) {
		*frag_level= CLD_LEV_CRITICAL;
	} else {
		pr_info("micron cld unknown level %d\n", attr);
		ret = -1;
	}
	return ret;
}
int micron_cld_set_trigger(struct ufscld_dev *cld, u32 trigger)
{
	struct ufs_hba *hba = cld->hba;
	if (trigger)
		return ufshcd_query_flag_retry(hba, UPIU_QUERY_OPCODE_SET_FLAG, (enum flag_idn)query_flag_idn_cld_enable, 0, NULL);
	else
		return ufshcd_query_flag_retry(hba, UPIU_QUERY_OPCODE_CLEAR_FLAG, (enum flag_idn)query_flag_idn_cld_enable, 0, NULL);
}
int micron_cld_get_trigger(struct ufscld_dev *cld, u32 *trigger)
{
	struct ufs_hba *hba = cld->hba;
	return ufshcd_query_flag_retry(hba, UPIU_QUERY_OPCODE_READ_FLAG, (enum flag_idn)query_flag_idn_cld_enable, 0, (bool *)trigger);
}
static int micron_cld_get_defragprogress(struct ufscld_dev *cld, int *defragprogress)
{
	struct ufs_hba *hba = cld->hba;
	int ret = 0;
	*defragprogress = 0;
	ret = ufshcd_query_attr_retry(hba, UPIU_QUERY_OPCODE_READ_ATTR, (enum attr_idn)query_attr_idn_defrag, 0, 0, defragprogress);
	return ret;
}
int micron_cld_operation_status(struct ufscld_dev *cld, int *op_status)
{
	int ret = 0;
	int defragprogress = 0;
	ret = micron_cld_get_defragprogress(cld, &defragprogress);
	if (ret)
		ERR_MSG("get defragprogress failed ret=%d\n", ret);
	if (defragprogress == DEFRAG_OPERATION_COMPLETED || defragprogress == DEFRAG_OPERATION_STOPPED) {// if cld was done
		*op_status = CLD_STATUS_IDLE;
	} else if (defragprogress == DEFRAG_OPERATION_ONGOING){
		*op_status = CLD_STATUS_PROGRESSING;
	} else {
		*op_status = CLD_STATUS_NA;
	}
	return ret;
}
struct ufscld_ops micron_cld_ops = {
	.cld_get_frag_level = micron_get_frag_level,
	.cld_set_trigger = micron_cld_set_trigger,
	.cld_get_trigger = micron_cld_get_trigger,
	.cld_operation_status = micron_cld_operation_status
};
