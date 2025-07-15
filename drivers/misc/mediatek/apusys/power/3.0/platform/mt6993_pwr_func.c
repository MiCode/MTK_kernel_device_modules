// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#include <linux/slab.h>
#include "apu_top.h"
#include "aputop_log.h"
#include "aputop_rpmsg.h"
#include <linux/delay.h>
#include <linux/iopoll.h>
#include <linux/sched/clock.h>
#include "mt6993_apupwr.h"
#include "mt6993_apupwr_prot.h"
#define LOCAL_DBG	(1)
static void __iomem *spare_reg_base;
static void __iomem *are_sram_base;
// for saving data after sync with remote site
static struct tiny_dvfs_opp_tbl opp_tbl;
static struct tiny_dvfs_opp_tbl opp_tbl2;
static struct apu_pwr_curr_info curr_info;
int mt6993_mdla_pll_freq[OPP_TABLE_SIZE];
int mt6993_mvpu_pll_freq[OPP_TABLE_SIZE];
int opp_request_bit;
static const char * const pll_name[] = {
				"PLL_CONN", "PLL_RV33", "PLL_MVPU", "PLL_MDLA"};
static const char * const buck_name[] = {
				"BUCK_VAPU", "BUCK_VSRAM", "BUCK_VCORE"};
#define _OPP_LMT_TBL(_opp_lmt_reg) {    \
	.opp_lmt_reg = _opp_lmt_reg,    \
}

#define APUPW_READQ(_addr)         ((uint64_t)(apu_readl(_addr)) + ((uint64_t)(apu_readl(_addr + 0x4)) << 32))
#define APUPW_WRITEQ(_val, _addr)  { \
	apu_writel((uint32_t)(_val & 0xFFFFFFFF), _addr); \
	apu_writel((uint32_t)(_val >> 32), _addr + 0x4); \
}

static struct cluster_dev_opp_info opp_limit_tbl[CLUSTER_NUM] = {
	_OPP_LMT_TBL(D_ACX_LIMIT_OPP_REG),
};

static inline int over_range_check(int opp)
{
	// we treat opp -1 as a special hint regard to unlimit opp !
	if (opp == -1)
		return -1;
	else if (opp < mt6993_user_max_opp)
		return mt6993_user_max_opp;
	else if (opp > USER_MIN_OPP_VAL)
		return USER_MIN_OPP_VAL;
	else
		return opp;
}

static void _opp_limiter(int vpu_max, int vpu_min, int dla_max, int dla_min,
		enum apu_opp_limit_type type)
{
	int i;
	unsigned int reg_data;
	unsigned int reg_offset;

	vpu_max = over_range_check(vpu_max);
	vpu_min = over_range_check(vpu_min);
	dla_max = over_range_check(dla_max);
	dla_min = over_range_check(dla_min);
#if LOCAL_DBG
	apu_pr_info_ratelimited("%s type:%d, %d/%d/%d/%d\n", __func__, type,
			vpu_max, vpu_min, dla_max, dla_min);
#endif

	for (i = 0 ; i < CLUSTER_NUM ; i++) {
		opp_limit_tbl[i].dev_opp_lmt.vpu_max = vpu_max & 0x3f;
		opp_limit_tbl[i].dev_opp_lmt.vpu_min = vpu_min & 0x3f;
		opp_limit_tbl[i].dev_opp_lmt.dla_max = dla_max & 0x3f;
		opp_limit_tbl[i].dev_opp_lmt.dla_min = dla_min & 0x3f;
		opp_limit_tbl[i].dev_opp_lmt.lmt_type = type & 0xff;
		reg_data = 0x0;
		reg_data = (
			((vpu_max & 0x3f) << 0) |	// [5:0]
			((vpu_min & 0x3f) << 6) |	// [11:6]
			((dla_max & 0x3f) << 12) |	// [17:12]
			((dla_min & 0x3f) << 18) |	// [23:18]
			((type & 0xff) << 24));		// dedicate 1 byte
		reg_offset = opp_limit_tbl[i].opp_lmt_reg;
		apu_writel(reg_data, spare_reg_base + reg_offset);
#if LOCAL_DBG
		apu_pr_info_ratelimited("%s cluster%d write:0x%08x, readback:0x%08x\n",
				__func__, i, reg_data,
				apu_readl(spare_reg_base + reg_offset));
#endif
	}
}

static void limit_opp_to_all_devices(int opp)
{
	int c_id, d_id;

	for (c_id = 0 ; c_id < CLUSTER_NUM ; c_id++)
		for (d_id = 0 ; d_id < DEVICE_NUM ; d_id++)
			_opp_limiter(opp, opp, opp, opp, OPP_LIMIT_DEBUG);

}

void mt6993_aputop_opp_limit(int upper_opp, int low_opp,
		enum apu_opp_limit_type type)
{
	int vpu_max, vpu_min, dla_max, dla_min;

	vpu_max = upper_opp;
	vpu_min = low_opp;
	dla_max = upper_opp;
	dla_min = low_opp;
	_opp_limiter(vpu_max, vpu_min, dla_max, dla_min, type);
}

#if IS_ENABLED(CONFIG_DEBUG_FS)
static int aputop_dbg_set_parameter(int param, int argc, int *args)
{
	int ret = 0, i;
	struct aputop_rpmsg_data rpmsg_data;

	for (i = 0 ; i < argc ; i++) {
		if (args[i] < -1 || args[i] >= INT_MAX) {
			pr_info("%s invalid args[%d]\n", __func__, i);
			return -EINVAL;
		}
	}
	memset(&rpmsg_data, 0, sizeof(struct aputop_rpmsg_data));

	switch (param) {
	case APUPWR_DBG_DEV_CTL:
		if (argc == 3) {
			rpmsg_data.cmd = APUTOP_DEV_CTL;
			rpmsg_data.data0 = args[0]; // cluster_id
			rpmsg_data.data1 = args[1]; // device_id
			rpmsg_data.data2 = args[2]; // POWER_ON/POWER_OFF
			aputop_send_rpmsg(&rpmsg_data, 100);
		} else {
			pr_info("%s invalid param num:%d\n", __func__, argc);
			ret = -EINVAL;
		}
		break;
	case APUPWR_DBG_DEV_SET_OPP:
		if (argc == 3) {
			rpmsg_data.cmd = APUTOP_DEV_SET_OPP;
			rpmsg_data.data0 = args[0]; // cluster_id
			rpmsg_data.data1 = args[1]; // device_id
			rpmsg_data.data2 = args[2]; // opp
			aputop_send_rpmsg(&rpmsg_data, 100);
		} else {
			pr_info("%s invalid param num:%d\n", __func__, argc);
			ret = -EINVAL;
		}
		break;
	case APUPWR_DBG_DVFS_DEBUG:
		if (argc == 1) {
			limit_opp_to_all_devices(args[0]);
		} else {
			pr_info("%s invalid param num:%d\n", __func__, argc);
			ret = -EINVAL;
		}
		break;
	case APUPWR_DBG_DUMP_OPP_TBL:
		if (argc == 1) {
			rpmsg_data.cmd = APUTOP_DUMP_OPP_TBL;
			rpmsg_data.data0 = args[0]; // pseudo data
			aputop_send_rpmsg(&rpmsg_data, 100);
		} else {
			pr_info("%s invalid param num:%d\n", __func__, argc);
			ret = -EINVAL;
		}
		break;
	case APUPWR_DBG_DUMP_OPP_TBL2:
		if (argc == 1) {
			rpmsg_data.cmd = APUTOP_DUMP_OPP_TBL2;
			rpmsg_data.data0 = args[0]; // pseudo data
			aputop_send_rpmsg(&rpmsg_data, 100);
		} else {
			pr_info("%s invalid param num:%d\n", __func__, argc);
			ret = -EINVAL;
		}
		break;
	case APUPWR_DBG_CURR_STATUS:
		if (argc == 1) {
			rpmsg_data.cmd = APUTOP_CURR_STATUS;
			rpmsg_data.data0 = args[0]; // pseudo data
			aputop_send_rpmsg(&rpmsg_data, 100);
		} else {
			pr_info("%s invalid param num:%d\n", __func__, argc);
			ret = -EINVAL;
		}
		break;
	case APUPWR_DBG_PROFILING:
		if (argc == 2) {
			rpmsg_data.cmd = APUTOP_PWR_PROFILING;
			// 0:clean, 1:result, 2:allow bit, 3:allow bitmask
			rpmsg_data.data0 = args[0];
			// value of allow bit/bitmask
			rpmsg_data.data1 = args[1]; // allow bitmask
			aputop_send_rpmsg(&rpmsg_data, 100);
		} else {
			pr_info("%s invalid param num:%d\n", __func__, argc);
			ret = -EINVAL;
		}
		break;
	case APUPWR_DBG_CLK_SET_RATE:
		if (argc == 4) {
			rpmsg_data.cmd = APUTOP_CLK_SET_RATE;
			rpmsg_data.data0 = args[0]; // conn
			rpmsg_data.data1 = args[1]; // rv33
			rpmsg_data.data2 = args[2]; // mvpu
			rpmsg_data.data3 = args[3]; // mdla
			aputop_send_rpmsg(&rpmsg_data, 100);
		} else {
			pr_info("%s invalid param num:%d\n", __func__, argc);
			ret = -EINVAL;
		}
		break;
	case APUPWR_DBG_BUK_SET_VOLT:
		if (argc == 2) {
			rpmsg_data.cmd = APUTOP_BUK_SET_VOLT;
			rpmsg_data.data0 = args[0]; // vapu target opp
			rpmsg_data.data1 = args[1]; // vapu target volt
			aputop_send_rpmsg(&rpmsg_data, 100);
		} else {
			pr_info("%s invalid param num:%d\n", __func__, argc);
			ret = -EINVAL;
		}
		break;
	case APUPWR_DBG_ARE:
		if (argc == 1) {
			rpmsg_data.cmd = APUTOP_ARE_DBG;
			// args[0] = are hw id
			rpmsg_data.data0 = args[0];
			aputop_send_rpmsg(&rpmsg_data, 100);
		} else {
			pr_info("%s invalid param num:%d\n", __func__, argc);
			ret = -EINVAL;
		}
		break;
	case APUPWR_DBG_HW_VOTER:
		if (argc == 1) {
			rpmsg_data.cmd = APUTOP_HW_VOTER_DBG;
			// args[0] = are hw id
			rpmsg_data.data0 = args[0];
			aputop_send_rpmsg(&rpmsg_data, 100);
		} else {
			pr_info("%s invalid param num:%d\n", __func__, argc);
			ret = -EINVAL;
		}
		break;
	case APUPWR_DBG_MISC:
		if (argc == 4) {
			rpmsg_data.cmd = APUTOP_DBG_MISC;
			rpmsg_data.data0 = args[0];
			rpmsg_data.data1 = args[1];
			rpmsg_data.data2 = args[2];
			rpmsg_data.data3 = args[3];
			aputop_send_rpmsg(&rpmsg_data, 100);
		} else {
			pr_info("%s invalid param num:%d\n", __func__, argc);
			ret = -EINVAL;
		}
		break;
	default:
		pr_info("%s unsupport the pwr param:%d\n", __func__, param);
		ret = -EINVAL;
		break;
	}

	return ret;
}

// boost range : 100 ~ 0 (from fast to slow)
// opp range : 1 ~ USER_MIN_OPP_VAL (from fast to slow) , opp0 is turbo boost
static int _apu_boost_to_opp(int boost)
{
	int opp = mt6993_user_max_opp;
	int max_opp_vpu_freq, min_opp_vpu_freq, boost_to_freq;

	if (boost >= 100)
		return mt6993_user_max_opp;

	if (boost < 0)
		boost = 0;

	max_opp_vpu_freq = opp_tbl.opp[mt6993_user_max_opp].pll_freq[PLL_VPU];

	if (opp_tbl2.tbl_size > 0)
		min_opp_vpu_freq = opp_tbl2.opp[opp_tbl2.tbl_size - 1].pll_freq[PLL_VPU];
	else
		min_opp_vpu_freq = opp_tbl.opp[USER_MIN_OPP_VAL].pll_freq[PLL_VPU];

	boost_to_freq = boost * (max_opp_vpu_freq - min_opp_vpu_freq) / 100 + min_opp_vpu_freq;

	for (int i = mt6993_user_max_opp ; i < opp_tbl.tbl_size ; i++){
		if (boost_to_freq >= opp_tbl.opp[i].pll_freq[PLL_VPU]){
			opp = i;
			return opp;
		}
	}

	for (int i = 0 ; i < opp_tbl2.tbl_size ; i++){
		if (boost_to_freq >= opp_tbl2.opp[i].pll_freq[PLL_VPU]){
			opp = i + opp_tbl.tbl_size;
			break;
		}
	}

	return opp;
}

static void plat_dump_boost_mapping(struct seq_file *s)
{
	int boost, opp, i;
	int opp_cnt[USER_MIN_OPP_VAL + 1] = {};
	int max_boost = 100;
	int prev_min_opp_boost;
	int cur_min_opp_boost;

	for (boost = TURBO_BOOST_VAL  ; boost >= 0 ; boost--) {
		opp =  _apu_boost_to_opp(boost);
		if(boost <= max_boost)
			opp_cnt[opp]++;
	}

	for (i = mt6993_user_max_opp ; i <= USER_MIN_OPP_VAL ; i++){
		if(i == mt6993_user_max_opp){
			seq_printf(s, "opp:%2d : boost:%3d ~ %3d (%2d)\n",
					   i,
					   max_boost,
					   max_boost,
					   opp_cnt[i]);
			prev_min_opp_boost = max_boost;
		} else {
			cur_min_opp_boost = prev_min_opp_boost - opp_cnt[i];
			seq_printf(s, "opp:%2d : boost:%3d ~ %3d (%2d)\n",
					   i,
					   cur_min_opp_boost,
					   prev_min_opp_boost-1,
					   opp_cnt[i]);
			prev_min_opp_boost = cur_min_opp_boost;
		}
	}
}

static int aputop_show_opp_tbl(struct seq_file *s, void *unused)
{
	struct tiny_dvfs_opp_tbl tbl;
	int size, i, j, k;

	pr_info("%s ++\n", __func__);
	memcpy(&tbl, &opp_tbl, sizeof(struct tiny_dvfs_opp_tbl));
	size = tbl.tbl_size;
	// first line
	seq_printf(s, "\n| # | %s | %s|", buck_name[0], buck_name[1]);

	for (i = 0 ; i <= PLL_DLA ; i++)
		seq_printf(s, " %s |", pll_name[i]);
	seq_puts(s, "\n");

	for (i = 0 ; i < size ; i++) {
		seq_printf(s, "| %d |   %06d  |   %06d  |",
			i, tbl.opp[i].vapu, tbl.opp[i].vsram);
		for (j = 0 ; j <= PLL_DLA ; j++)
			seq_printf(s, "  %07d |", tbl.opp[i].pll_freq[j]);
		seq_puts(s, "\n");
	}
	memcpy(&tbl, &opp_tbl2, sizeof(struct tiny_dvfs_opp_tbl));
	size = tbl.tbl_size;

	for (k = 0; size > 0 ; size--, i++, k++) {
		seq_printf(s, "| %d |   %06d  |   %06d  |",
			i, tbl.opp[k].vapu, tbl.opp[k].vsram);
		for (j = 0 ; j <= PLL_DLA ; j++)
			seq_printf(s, "  %07d |", tbl.opp[k].pll_freq[j]);
		seq_puts(s, "\n");
	}

	seq_puts(s, "\n");
	plat_dump_boost_mapping(s);
	seq_puts(s, "\n");

	return 0;
}

static int aputop_show_curr_status(struct seq_file *s, void *unused)
{
	struct apu_pwr_curr_info info;
	struct rpc_status_dump cluster_dump[CLUSTER_NUM + 1];
	int i;

	pr_info("%s ++\n", __func__);
	memset(&cluster_dump, 0, sizeof(struct rpc_status_dump));
	memcpy(&info, &curr_info, sizeof(struct apu_pwr_curr_info));
	seq_puts(s, "\n");

	for (i = 0 ; i <= PLL_DLA ; i++) {
		seq_printf(s, "%s : opp %d , %d(kHz)\n",
				pll_name[i],
				info.pll_opp[i],
				info.pll_freq[i]);
	}

	for (i = 0 ; i < BUCK_NUM ; i++) {
		if (info.buck_volt[i])
			seq_printf(s, "%s : opp %d , %d(uV)\n",
					buck_name[i],
					info.buck_opp[i],
					info.buck_volt[i]);
	}

	return 0;
}

static int apu_top_dbg_show(struct seq_file *s, void *unused)
{
	int ret = 0;
	enum aputop_rpmsg_cmd cmd = get_curr_rpmsg_cmd();

	pr_info("%s for aputop_rpmsg_cmd : %d\n", __func__, cmd);

	if (cmd == APUTOP_DUMP_OPP_TBL2)
		ret = aputop_show_opp_tbl(s, unused);
	else if (cmd == APUTOP_CURR_STATUS)
		ret = aputop_show_curr_status(s, unused);
	else
		pr_info("%s not support this cmd\n", __func__);

	return ret;
}

int mt6993_apu_top_dbg_open(struct inode *inode, struct file *file)
{
	pr_info("%s ++\n", __func__);
	return single_open(file, apu_top_dbg_show, inode->i_private);
}

#define MAX_ARG 4
ssize_t mt6993_apu_top_dbg_write(
		struct file *flip, const char __user *buffer,
		size_t count, loff_t *f_pos)
{
	char *tmp, *token, *cursor;
	int ret, i, param;
	unsigned int args[MAX_ARG];

	tmp = kzalloc(count + 1, GFP_KERNEL);

	if (!tmp)
		return -ENOMEM;

	ret = copy_from_user(tmp, buffer, count);

	if (ret) {
		pr_info("[%s] copy_from_user failed, ret=%d\n", __func__, ret);
		goto out;
	}

	tmp[count] = '\0';
	cursor = tmp;
	/* parse a command */
	token = strsep(&cursor, " ");
	if (!strcmp(token, "device_ctl"))
		param = APUPWR_DBG_DEV_CTL;
	else if (!strcmp(token, "device_set_opp"))
		param = APUPWR_DBG_DEV_SET_OPP;
	else if (!strcmp(token, "dvfs_debug"))
		param = APUPWR_DBG_DVFS_DEBUG;
	else if (!strcmp(token, "dump_opp_tbl"))
		param = APUPWR_DBG_DUMP_OPP_TBL;
	else if (!strcmp(token, "dump_opp_tbl2"))
		param = APUPWR_DBG_DUMP_OPP_TBL2;
	else if (!strcmp(token, "curr_status"))
		param = APUPWR_DBG_CURR_STATUS;
	else if (!strcmp(token, "pwr_profiling"))
		param = APUPWR_DBG_PROFILING;
	else if (!strcmp(token, "clk_set_rate"))
		param = APUPWR_DBG_CLK_SET_RATE;
	else if (!strcmp(token, "buk_set_volt"))
		param = APUPWR_DBG_BUK_SET_VOLT;
	else if (!strcmp(token, "are_dump"))
		param = APUPWR_DBG_ARE;
	else if (!strcmp(token, "hw_voter_dump"))
		param = APUPWR_DBG_HW_VOTER;
	else if (!strcmp(token, "dbg_misc"))
		param = APUPWR_DBG_MISC;
	else {
		ret = -EINVAL;
		pr_info("no power param[%s]!\n", token);
		goto out;
	}
	/* parse arguments */
	for (i = 0; i < MAX_ARG && (token = strsep(&cursor, " ")); i++) {
		ret = kstrtoint(token, 10, &args[i]);
		if (ret) {
			pr_info("fail to parse args[%d](%s)", i, token);
			goto out;
		}
	}

	aputop_dbg_set_parameter(param, i, args);
	ret = count;
out:
	kfree(tmp);
	return ret;
}
#endif

void mt6993_request_opp_table(void)
{
	struct aputop_rpmsg_data rpmsg_data;
	int retry = 10, ret = 0;

	opp_request_bit = 1;
	memset(&rpmsg_data, 0, sizeof(struct aputop_rpmsg_data));

	rpmsg_data.cmd = APUTOP_DUMP_OPP_TBL;
	rpmsg_data.data0 = 1; // pseudo data
	do {
		ret = aputop_send_rpmsg(&rpmsg_data, 200);
		if (mt6993_mdla_pll_freq[USER_MID_OPP_VAL - 1] != 0)
			break;
		udelay(1000);
	} while (--retry);

	retry = 10;
	rpmsg_data.cmd = APUTOP_DUMP_OPP_TBL2;
	rpmsg_data.data0 = 1; // pseudo data
	do {
		ret = aputop_send_rpmsg(&rpmsg_data, 200);
		if (mt6993_mdla_pll_freq[OPP_TABLE_SIZE - 1] != 0)
			break;
		udelay(1000);
	} while (--retry);

}

static void save_opp_table(struct tiny_dvfs_opp_tbl *tbl, int start_index)
{
	struct tiny_dvfs_opp_tbl mytbl;
	int size, i, j;

	size = tbl->tbl_size;
	memcpy(&mytbl, tbl, sizeof(struct tiny_dvfs_opp_tbl));
	size = mytbl.tbl_size;

	pr_info("Saving OPP Table Data to mytbl:\n");
	for (i = 0; i < size; i++) {
		pr_info("OPP %d: vapu=%d, vsram=%d", i, mytbl.opp[i].vapu, tbl->opp[i].vsram);
		for (j = 0; j < 4; j++)
			pr_info(" pll_freq[%d]=%d", j, mytbl.opp[i].pll_freq[j]);
		if (i + start_index < OPP_TABLE_SIZE)
			mt6993_mdla_pll_freq[i + start_index] = mytbl.opp[i].pll_freq[PLL_DLA];
		pr_info("\n");
	}

	for (i = 0; i < size; i++) {
		if (i + start_index < OPP_TABLE_SIZE)
			mt6993_mvpu_pll_freq[i + start_index] = mytbl.opp[i].pll_freq[PLL_VPU];
		pr_info("\n");
	}
}

int mt6993_apu_top_rpmsg_cb(int cmd, void *data, int len, void *priv, u32 src)
{
	int ret = 0;

	switch ((enum aputop_rpmsg_cmd)cmd) {
	case APUTOP_DEV_CTL:
	case APUTOP_DEV_SET_OPP:
	case APUTOP_PWR_PROFILING:
	case APUTOP_CLK_SET_RATE:
	case APUTOP_BUK_SET_VOLT:
		// do nothing
		break;
	case APUTOP_DUMP_OPP_TBL:
		if (len) {
			memcpy(&opp_tbl, data, len);
			if(opp_request_bit == 1) {
				save_opp_table(&opp_tbl, 0);
				//memset(&opp_tbl, 0, sizeof(opp_tbl));
			}
		} else
			ret = -EINVAL;
		break;
	case APUTOP_DUMP_OPP_TBL2:
		if (len) {
			memcpy(&opp_tbl2, data, len);
			if(opp_request_bit == 1) {
				save_opp_table(&opp_tbl2, USER_MID_OPP_VAL);
				//memset(&opp_tbl, 0, sizeof(opp_tbl));
				opp_request_bit = 0;
			}
		} else
			ret = -EINVAL;
		break;
	case APUTOP_CURR_STATUS:
		if (len == sizeof(curr_info)) {
			memcpy(&curr_info,
				(struct apu_pwr_curr_info *)data, len);
		} else {
			pr_info("%s invalid size : %d/%lu\n",
					__func__, len, sizeof(curr_info));
			ret = -EINVAL;
		}
		break;
	default:
		pr_info("%s invalid cmd : %d\n", __func__, cmd);
		ret = -EINVAL;
	}

	return ret;
}

int mt6993_drv_cfg_remote_sync(struct aputop_func_param *aputop)
{
	void __iomem *reg_addr = 0x0;
	uint32_t reg_data = 0x0;
	int func_id = 0;

	if (IS_ERR_OR_NULL(aputop))
		return -EINVAL;

	func_id = aputop->func_id;

	if (func_id == APUTOP_FUNC_DRV_CFG) {
		reg_addr = spare_reg_base + DRV_CFG_SYNC_REG;
	} else if (func_id == APUTOP_FUNC_FEATURE_OPTION_0) {
		reg_addr = spare_reg_base + FEATURE_OPTION_0_SYNC_REG;
	} else if (func_id == APUTOP_FUNC_FEATURE_OPTION_1) {
		reg_addr = spare_reg_base + FEATURE_OPTION_1_SYNC_REG;
	} else {
		pr_info("%s id %d is invalid\n", __func__, func_id);
		return -EINVAL;
	}

	reg_data = (aputop->param1 & 0xff) |
		((aputop->param2 & 0xff) << 8) |
		((aputop->param3 & 0xff) << 16) |
		((aputop->param4 & 0xff) << 24);

	apu_writel(reg_data, reg_addr);

	return 0;
}

int mt6993_init_remote_data_sync(void __iomem *reg_base, void __iomem *are_base)
{
	int i;
	uint32_t reg_offset = 0x0;

	spare_reg_base = reg_base;
	are_sram_base = are_base;
	for (i = 0 ; i < CLUSTER_NUM ; i++) {
		// 0xffff_ffff means no limit
		memset(&opp_limit_tbl[i].dev_opp_lmt, -1,
				sizeof(struct device_opp_limit));
		reg_offset = opp_limit_tbl[i].opp_lmt_reg;
#if LOCAL_DBG
		pr_info("%s spare_reg_base:0x%p, offset:0x%08x\n",
				__func__, spare_reg_base, reg_offset);
#endif
		apu_writel(0xFFFC0FC0, spare_reg_base + reg_offset);
	}

	apu_writel(0x0, spare_reg_base + DRV_CFG_SYNC_REG);
	apu_writel(0x0, spare_reg_base + FEATURE_OPTION_0_SYNC_REG);
	apu_writel(0x0, spare_reg_base + FEATURE_OPTION_1_SYNC_REG);

	return 0;
}

#define DECODE_READ_POINTER(_r_w_code)     (((_r_w_code) & 0x00003FFF) << 2)
#define DECODE_WRITE_POINTER(_r_w_code)    (((_r_w_code) & 0x3FFF0000) >> 14)
#define OPP_STTS_BUF_FOOTPRINT(_r_ptr)     ((uint32_t)(_r_ptr) + 0x4)
#define OPP_STTS_BUF_CURR_OPP_TS(_r_ptr)   ((uint32_t)(_r_ptr) + 0x8)
#define OPP_STTS_BUF_OPPS_TS(_r_ptr, _opp) ((uint32_t)(_r_ptr) + 0x10 + (0x8 * _opp))
#define ENG_ON_BUF_ENG_TS(_r_ptr, _eng_id) ((uint32_t)(_r_ptr) + 0x08 + (0x8 * _eng_id))

int mt6993_request_npu_pwr_stats(
	enum NPUPW_STTS_REQ_TYPE req_type, enum NPUPW_STTS_REQ_MODE mode,
	struct npupw_stts *p_npupw_stts)
{
	int ret = 0;
	uint32_t val = 0;
	uint32_t r_ptr_ofst = 0;
	uint64_t npu_on_ts = 0, npu_on_stats = 0;
	uint32_t opp_footprint = 0;
	uint64_t opp_time_stats[OPP_TABLE_SIZE] = {0}, curr_opp_ts0 = 0;
	uint64_t engine_on_time[NPU_ENGINES_MAX] = {0};
	void __iomem *mbox_pwr_idx_sync_reg = spare_reg_base + APU_PWR_INDXER_SYNC_REG;

	/* check if FO on */
	if ((apu_readl(are_sram_base + ARE_NDM_ENALBE_HINT) &
		BIT(APUPW_FO_HINT_PWR_IDX_BIT)) == 0) {
		ret = 1;
		goto out;
	}

	/* hold mbox */
	// FIXME: use mbox sema
	ret = readl_relaxed_poll_timeout_atomic(mbox_pwr_idx_sync_reg, val, (!val), 50, 2000);
	if (ret) {
		pr_info("%s polling mbox clear timeout, val = 0x%x, ret %d\n", __func__, val, ret);
		ret = 2;
		goto out;
	}
	apu_writel(0x1, mbox_pwr_idx_sync_reg);

	// pr_info("%s get mbox!\n", __func__);

	switch (req_type) {
	case NPU_STTS_NPU_ON:
		// TODO: overwrite read and write ptr, and clear write buffer
		r_ptr_ofst = apu_readl(are_sram_base + ARE_PWR_IDX_NPU_ON_R_W_PTR);
		r_ptr_ofst = DECODE_READ_POINTER(r_ptr_ofst);

		npu_on_ts = APUPW_READQ(are_sram_base + r_ptr_ofst);
		npu_on_stats = APUPW_READQ(are_sram_base + r_ptr_ofst + 0x8);

		break;
	case NPU_STTS_NPUFREQ:
		// TODO: overwrite read and write ptr, and clear write buffer
		r_ptr_ofst = apu_readl(are_sram_base + ARE_PWR_IDX_OPP_ST_R_W_PTR);
		r_ptr_ofst = DECODE_READ_POINTER(r_ptr_ofst);

		opp_footprint = apu_readl(
			(are_sram_base + OPP_STTS_BUF_FOOTPRINT(r_ptr_ofst)));

		curr_opp_ts0 = APUPW_READQ(
			(are_sram_base + OPP_STTS_BUF_CURR_OPP_TS(r_ptr_ofst)));

		for (int _opp = 0; _opp < OPP_TABLE_SIZE; ++_opp)
			opp_time_stats[_opp] = APUPW_READQ(
				(are_sram_base + OPP_STTS_BUF_OPPS_TS(r_ptr_ofst, _opp)));
		break;
	case NPU_STTS_ENGINE_ON:
		// TODO: overwrite read and write ptr, and clear write buffer
		r_ptr_ofst = apu_readl(are_sram_base + ARE_PWR_IDX_ENG_ON_R_W_PTR);
		r_ptr_ofst = DECODE_READ_POINTER(r_ptr_ofst);

		// read the buffer in sram
		for (int i = 0; i < NPU_ENGINES_MAX; ++i)
			engine_on_time[i] = APUPW_READQ(
				(are_sram_base + ENG_ON_BUF_ENG_TS(r_ptr_ofst, i)));
		break;
	case NPU_STTS_ALL:
		break;
	default:
		break;
	}

	/* release mbox */
	// FIXME: use mbox sema
	apu_writel(0x0, mbox_pwr_idx_sync_reg);

	/* Postprocessing part */
	switch (req_type) {
	case NPU_STTS_NPU_ON:
		if (npu_on_ts != 0) {
			uint64_t curr_ts_ns = sched_clock();

			npu_on_stats += (curr_ts_ns - npu_on_ts);
		}
		p_npupw_stts->npu_on_time_us = npu_on_stats / 1000;
		break;
	case NPU_STTS_NPUFREQ:
		for (int _opp = 0; _opp < OPP_TABLE_SIZE; ++_opp)
			p_npupw_stts->time_in_states_us[_opp] = opp_time_stats[_opp] / 1000;

		if ((curr_opp_ts0 != 0) && (opp_footprint & BIT(4))) {
			uint64_t curr_ts_ns = sched_clock();
			int curr_opp = opp_footprint & 0xF;

			p_npupw_stts->time_in_states_us[curr_opp] +=
				((curr_ts_ns - curr_opp_ts0) / 1000);
		}
		break;
	case NPU_STTS_ENGINE_ON:
		for (int i = 0; i < NPU_ENGINES_MAX; ++i)
			p_npupw_stts->engine_on_time_us[i] = (engine_on_time[i] / 1000) * (154 * 2);
		break;
	case NPU_STTS_ALL:
		break;
	default:
		break;
	}

out:
	return ret;
}
