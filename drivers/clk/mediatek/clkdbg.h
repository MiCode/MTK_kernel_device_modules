/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 * Author: Owen Chen <owen.chen@mediatek.com>
 */

#include <linux/platform_device.h>

struct seq_file;

#define CMDFN(_cmd, _fn) {	\
	.cmd = _cmd,		\
	.fn = _fn,		\
}

struct cmd_fn {
	const char	*cmd;
	int (*fn)(struct seq_file *s, void *v);
};

#define TEST_CLK_NUM (20)

enum {
	CLK_TEST_TASK_ON_OFF = 0,
	CLK_TEST_TASK_SET_PARENT,
	CLK_TEST_TASK_NONE,
};

#define TEST_CLK_TO_CLK(t_clk) (t_clk.test_clk_p.test_clk)
#define TEST_CLK_TO_GENPD(t_clk) (t_clk.test_clk_p.test_genpd_dev)

union _test_clk_p {
	struct clk *test_clk;
	struct device *test_genpd_dev;
};

enum {
	TEST_TYPE_NONE = 0,
	TEST_TYPE_CLK = 1,
	TEST_TYPE_GENPD = 2,
};

struct test_clk {
	int test_clk_type;
	union _test_clk_p test_clk_p;
};


struct test_task_clk {
	int type;
	struct test_clk test_clk[TEST_CLK_NUM];
	int test_clk_num;
	int repeat_time;
};

struct clkdbg_ops {
	const struct fmeter_clk *(*get_all_fmeter_clks)(void);
	void *(*prepare_fmeter)(void);
	void (*unprepare_fmeter)(void *data);
	u32 (*fmeter_freq)(const struct fmeter_clk *fclk);
	const char * const *(*get_all_clk_names)(void);
	const char * const *(*get_pwr_names)(void);
	u32 (*get_spm_pwr_status)(void);
	int (*start_task)(void *data);
};

void set_clkdbg_ops(const struct clkdbg_ops *ops);
void unset_clkdbg_ops(void);
void clkdbg_set_cfg(void);
int clk_dbg_driver_register(struct platform_driver *drv, const char *name);
struct device *clkdbg_dev_from_name(const char *name);
struct clk *__clk_dbg_lookup(const char *name);

extern const struct regname *get_all_regnames(void);
extern struct provider_clk *get_all_provider_clks(bool is_internal);
extern int pdchk_pd_is_on(int pd_id);

