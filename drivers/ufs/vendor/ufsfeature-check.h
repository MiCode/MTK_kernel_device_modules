#ifndef _UFS_WB_CHECK_
#define _UFS_WB_CHECK_
#include <linux/types.h>
struct check_wb_t {
	int total_gb;
	int wb_gb;
};
struct ufstw_check_info {
	unsigned int tw_lun_buf_size;
	u32 seg_size;
	u8 unit_size;
};

void ufs_tw_check(struct ufs_hba *hba);
#endif
