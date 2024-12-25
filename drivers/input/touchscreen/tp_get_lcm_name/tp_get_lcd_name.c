#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/module.h>
#include <linux/kernel.h>

#include "tp_get_lcd_name.h"

const char *get_lcd_name;
void get_panel_name(const char *flag)
{
	get_lcd_name = flag;
	printk("[GTP][FTS_TS]%s get_lcd_name:%s \n", __func__, get_lcd_name);
}
EXPORT_SYMBOL_GPL(get_panel_name);

int goodix_focal_panel_name(void)
{
	int panel_id = -1;
	if(strstr(get_lcd_name,"panel_name=dsi_o16u_42_02_0a_dsc_vdo")) {
		panel_id = 0;
	} else if (strstr(get_lcd_name, "panel_name=dsi_o16u_36_02_0b_dsc_vdo")) {
		panel_id = 1;
	} else if (strstr(get_lcd_name, "panel_name=dsi_o16u_44_0f_0c_dsc_vdo")){
		panel_id = 2;
	} else {
		pr_info("[GTP][FTS_TS]no driver match!!!");
	}

	return panel_id;
}
EXPORT_SYMBOL_GPL(goodix_focal_panel_name);


static int __init tp_get_lcd_name_init(void)
{
	pr_info("[GTP][FTS_TS]%s Entry\n", __func__);

	return 0;

}

static void __exit tp_get_lcd_name_exit(void)
{
	pr_info("[GTP][FTS_TS]%s Entry\n", __func__);
}

MODULE_LICENSE("GPL v2");
module_init(tp_get_lcd_name_init);
module_exit(tp_get_lcd_name_exit);

