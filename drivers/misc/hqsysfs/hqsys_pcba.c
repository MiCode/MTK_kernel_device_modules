#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/export.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <linux/of_platform.h>
#include <linux/iio/consumer.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/printk.h>
#include <linux/err.h>
#include <linux/libfdt.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/types.h>
#include "hqsys_pcba.h"

struct PCBA_MSG pcba_msg = {PCBA_INFO_UNKNOW, STAGE_UNKNOW, 0, 0, NULL, NULL};

static void read_project_stage(void)
{
	int ret = 0, auxadc_voltage = 0, i = 0;
	struct iio_channel *channel;
	struct device_node *board_id_node;
	struct platform_device *board_id_dev;

	if (pcba_msg.pcba_config == 0)
		return;



	board_id_node = of_find_node_by_name(NULL, "board_id");

	if (board_id_node == NULL) {
		pr_err("[%s] find board_id node fail \n", __func__);
		return;
	} else {
		pr_err("[%s] find board_id node success %s \n", __func__, board_id_node->name);
	}

	board_id_dev = of_find_device_by_node(board_id_node);
	if (board_id_dev == NULL) {
		pr_err("[%s] find board_id dev fail \n", __func__);
		return;
	} else {
		pr_err("[%s] find board_id dev success %s \n", __func__, board_id_dev->name);
	}


	channel = iio_channel_get(&(board_id_dev->dev), "board_id-channel");

	if (IS_ERR(channel)) {
		ret = PTR_ERR(channel);
		pr_err("[%s] iio channel not found %d\n", __func__, ret);
		return;
	} else {
		pr_err("[%s] get channel success\n", __func__);
	}

	if (channel != NULL) {
		ret = iio_read_channel_processed(channel, &auxadc_voltage);
	} else {
		pr_err("[%s] no channel to processed \n", __func__);
		return;
	}

	if (ret < 0) {
		pr_err("[%s] IIO channel read failed %d \n", __func__, ret);
		return;
	} else {
		pr_err("[%s] auxadc_voltage is %d\n", __func__, auxadc_voltage);
	}

	for (i = 0; i < sizeof(stage_map)/sizeof(struct project_stage); i++) {
        if (stage_map[i].voltage_min <= auxadc_voltage && auxadc_voltage <= stage_map[i].voltage_max) {
			pcba_msg.pcba_stage = stage_map[i].project_stage;
			break;
		}
	}
	return;
}

static int pcba_config_cmdline_match( char *match_str, char *result, int size)
{
	struct device_node *cmdline_node = NULL;
	const char *cmdline;
	char *match, *match_end;
	int len, match_str_len, ret;
	if (!result || !match_str)
	return -EINVAL;
	memset(result, '\0', size);
	match_str_len = strlen(match_str);
	cmdline_node = of_find_node_by_path("/chosen");
	if (!cmdline_node) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}
	ret = of_property_read_string(cmdline_node, "bootargs", &cmdline);
	if (ret) {
		pr_err("%s failed to read bootargs\n", __func__);
		return -EINVAL;
	}
	match = strstr(cmdline, match_str);
	if (!match) {
		pr_err("Mmatch: %s fail in cmdline\n", match_str);
		return -EINVAL;
	}
	match_end = strstr((match + match_str_len), " ");
	if (!match_end) {
		pr_err("Match end of : %s fail in cmdline\n", match_str);
		return -EINVAL;
	}
	len = match_end - (match + match_str_len);
	if (len < 0 || len > size) {
		pr_err("Match cmdline :%s fail, len = %d\n", match_str, len);
		return -EINVAL;
	}
	memcpy(result, (match + match_str_len), len);
	return 0;
}

static int board_id_probe(struct platform_device *pdev)
{
	char result[32] = {};
	char *str_config ="pcba_config=";
	char *str_count ="pcba_config_count=";
	char *str_rsc = "rsc.name=";
	int id = 0, ret;

	ret = pcba_config_cmdline_match(str_config,result,sizeof(result));
	if (!ret) {
		ret = kstrtoint(result, 10, &id);
		if (ret) {
			id = 0;
		    pr_err("Covert bat_id fail, ret = %d, result = %s\n",ret, result);
		}
	}
	pcba_msg.pcba_config = id;
	ret = pcba_config_cmdline_match(str_count,result,sizeof(result));
	if (!ret) {
		ret = kstrtoint(result, 10, &id);
		if (ret) {
			id = 0;
			pr_err("Covert bat_id fail, ret = %d, result = %s\n",ret, result);
		}
	}
	pcba_msg.pcba_config_count = id;

	ret = pcba_config_cmdline_match(str_rsc, result, sizeof(result));
	if (!ret) {
		if (strstr(result, "cn"))
			pcba_msg.rsc = "cn";
		else if (strstr(result, "in"))
			pcba_msg.rsc = "in";
		else
			pcba_msg.rsc = "else";

		pr_err("rsc result = %s\n",result);
	}

	read_project_stage();

	pr_info("pcba_config = %d\n", pcba_msg.pcba_config);
	pr_info("pcba_config_count = %d\n", pcba_msg.pcba_config_count);
	pr_info("pcba_stage = %d\n", pcba_msg.pcba_stage);
	pr_info("rsc = %s\n", pcba_msg.rsc);

	pcba_msg.huaqin_pcba_config = (pcba_msg.pcba_stage - 1) * (pcba_msg.pcba_config_count) + pcba_msg.pcba_config;
	pr_info("[%s]: huaqin_pcba_config = %d\n", __func__, pcba_msg.huaqin_pcba_config);

	if (pcba_msg.huaqin_pcba_config < PCBA_INFO_UNKNOW || pcba_msg.huaqin_pcba_config >= PCBA_INFO_END) {
		pcba_msg.huaqin_pcba_config = PCBA_INFO_UNKNOW;
	}

	return 0;
}

static int board_id_remove(struct platform_device *pdev)
{
	pr_err("enter [%s] \n", __func__);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id boardId_of_match[] = {
	{.compatible = "mediatek,board_id",},
	{},
};
#endif

static struct platform_driver boardId_driver = {
	.probe = board_id_probe,
	.remove = board_id_remove,
	.driver = {
		.name = "board_id",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = boardId_of_match,
#endif
	},
};

struct PCBA_MSG* get_pcba_msg(void)
{
	return &pcba_msg;
}
EXPORT_SYMBOL_GPL(get_pcba_msg);

static int __init huaqin_pcba_early_init(void)
{
	int ret;
	pr_err("[%s]start to register boardId driver\n", __func__);

	ret = platform_driver_register(&boardId_driver);
	if (ret) {
		pr_err("[%s]Failed to register boardId driver\n", __func__);
		return ret;
	}
	return 0;
}

static void __exit huaqin_pcba_exit(void)
{
	platform_driver_unregister(&boardId_driver);
}

module_init(huaqin_pcba_early_init);//before device_initcall
module_exit(huaqin_pcba_exit);
MODULE_DESCRIPTION("huaqin sys pcba");
MODULE_LICENSE("GPL");
