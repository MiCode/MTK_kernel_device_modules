#ifndef _UFSCTX_H_
#define _UFSCTX_H_

#if IS_ENABLED(CONFIG_SCSI_UFS_XIAOMI_CLD)
#include "../feature/cld/ufscld.h"
#endif

#if IS_ENABLED(CONFIG_MI_ERROR_STATE)
/* Host UIC error code of PA layer */
enum ufshcd_ec_pa {
	UFS_EC_PA_LANE_0,
	UFS_EC_PA_LANE_1,
	UFS_EC_PA_LANE_2,
	UFS_EC_PA_LANE_3,
	UFS_EC_PA_LINE_RESET,
	UFS_EC_PA_MAX,
};
/* Host UIC error code of DL layer */
enum ufshcd_ec_dl {
	UFS_EC_DL_NAC_RECEIVED,
	UFS_EC_DL_TCx_REPLAY_TIMER_EXPIRED,
	UFS_EC_DL_AFCx_REQUEST_TIMER_EXPIRED,
	UFS_EC_DL_FCx_PROTECT_TIMER_EXPIRED,
	UFS_EC_DL_CRC_ERROR,
	UFS_EC_DL_RX_BUFFER_OVERFLOW,
	UFS_EC_DL_MAX_FRAME_LENGTH_EXCEEDED,
	UFS_EC_DL_WRONG_SEQUENCE_NUMBER,
	UFS_EC_DL_AFC_FRAME_SYNTAX_ERROR,
	UFS_EC_DL_NAC_FRAME_SYNTAX_ERROR,
	UFS_EC_DL_EOF_SYNTAX_ERROR,
	UFS_EC_DL_FRAME_SYNTAX_ERROR,
	UFS_EC_DL_BAD_CTRL_SYMBOL_TYPE,
	UFS_EC_DL_PA_INIT_ERROR,
	UFS_EC_DL_PA_ERROR_IND_RECEIVED,
	UFS_EC_DL_MAX,
};
struct ufs_uic_state {
	u32 pa_err_cnt;
	u32 pa_err[UFS_EC_PA_MAX];
	u32 dl_err_cnt;
	u32 dl_err[UFS_EC_DL_MAX];
	u32 dme_err_cnt;
};
struct ufs_err_state {
	u64 err_occurred; /*err state counted*/
	char err_reason[10][32]; /*err reason noted*/
};
enum ufs_xiaomi_event_type {
	/* Xiaomi err_state events */
	UFS_EVT_WAIT_UIC_CMD_FAIL=30,
	UFS_EVT_WAIT_DEV_CMD_FAIL=31,
	UFS_EVT_UIC_PWR_CTRL_FAIL=32,
	UFS_EVT_HIBERNATE_ENTER_FAIL=33,
	UFS_EVT_HIBERNATE_EXIT_FAIL=34,
	UFS_EVT_CHANGE_PWRMODE_FAIL=35,
	UFS_EVT_ERR_HANDLER=36,
	UFS_EVT_TASK_ABORT_FAIL=37,
};
/* Descriptor idn for Query requests */
enum ufs_xiaomi_desc_idn {
	QUERY_DESC_IDN_FBO		= 0xA,
};
/* FBO descriptor */
enum fbo_desc_param {
	FBO_DESC_PARAM_LEN	= 0x0,
	FBO_DESC_PARAM_VERSION	= 0x1,
	FBO_DESC_PARAM_REC_LBA_RANGE_SIZE	= 0x3,
	FBO_DESC_PARAM_MAX_LBA_RANGE_SIZE	= 0x7,
	FBO_DESC_PARAM_MIN_LBA_RANGE_SIZE	= 0xB,
	FBO_DESC_PARAM_MAX_LBA_RANGE_CONUT	= 0xF,
	FBO_DESC_PARAM_MAX_LBA_RANGE_ALIGNMENT	= 0x10,
};
/* Possible values for dExtendedUFSFeaturesSupport */
enum {
	UFS_DEV_FBO_SUP			= BIT(17),
};
#endif

struct ufs_xiaomi_ctx {
	struct ufs_hba *hba;
#if IS_ENABLED(CONFIG_MI_ERROR_STATE)
	struct ufs_uic_state uic_state;
	struct ufs_err_state err_state;
#endif
	struct completion ufslink_comp;
};

#if IS_ENABLED(CONFIG_MI_ERROR_STATE)
void ufshcd_update_err_state(u32 id, u32 val);
void ufshcd_update_uic_error_cnt(u32 id, u32 val);
#endif

void xm_scsi_register(struct ufs_hba *hba);
void xm_scsi_unregister(void);

struct ufs_xiaomi_ctx *get_ufs_xiaomi(void);
/* Export for CLD driver support */
void ufshcd_scsi_block_requests(struct ufs_hba *hba);
int ufshcd_wait_for_doorbell_clr(struct ufs_hba *hba, u64 wait_timeout_us);
void ufshcd_scsi_unblock_requests(struct ufs_hba *hba);
int ufshcd_query_flag_retry(struct ufs_hba *hba,enum query_opcode opcode, enum flag_idn idn, u8 index, bool *flag_res);
int ufshcd_query_attr_retry(struct ufs_hba *hba,enum query_opcode opcode, enum attr_idn idn, u8 index, u8 selector,u32 *attr_val);
/* Export for CLD driver support end*/

int ufshcd_query_descriptor_retry_xm(struct ufs_hba *hba,
				  enum query_opcode opcode,
				  enum desc_idn idn, u8 index,
				  u8 selector,
				  u8 *desc_buf, int *buf_len);
int ufshcd_read_desc_param_sel(struct ufs_hba *hba,
			   enum desc_idn desc_id,
			   int desc_index,
			   u8 selector,
			   u8 param_offset,
			   u8 *param_read_buf,
			   u8 param_size);

#endif
