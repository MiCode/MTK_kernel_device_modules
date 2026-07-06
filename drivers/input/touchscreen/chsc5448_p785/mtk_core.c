// SPDX-License-Identifier: GPL-2.0-only
/*
* Copyright (c) 2014 MediaTek Inc.
*/
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/cdev.h>
//#include <linux/ide.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/irq.h>
#include <linux/of_irq.h>
#include "tpd.h"
#include "semi_touch_interface.h"

extern  int  fix_tp_proc_info(void  *tp_data, u8 data_len);//add for 9375 info

int semi_touch_get_irq(int rst_pin)
{
    int irq_no = 0;
    struct device_node* node = NULL;
    unsigned int ints[2] = { 0, 0 };

    node = of_find_matching_node(node, touch_of_match);
    check_return_if_zero(node, NULL);

    of_property_read_u32_array(node, "debounce", ints, ARRAY_SIZE(ints));
    gpiod_set_debounce(gpio_to_desc(ints[0]), ints[1]);

    irq_no = irq_of_parse_and_map(node, 0);
    check_return_if_fail(irq_no, NULL);

    return irq_no;
}

int semi_touch_work_done(void)
{
    return 0;
}

int semi_touch_resource_release(void)
{
    return 0;
}



/********************************************************************************************************************************/
/*proximity support*/
#if SEMI_TOUCH_PROXIMITY_OPEN
#include "hwmsensor.h"
#include "hwmsen_dev.h"
#include "sensors_io.h"
struct hwmsen_object sm_obj_ps;

// int semi_touch_proximity_update(unsigned char enter)
// {
//     int ret = 0;

//     if(is_proximity_function_en(st_dev.stc.custom_function_en))
//     {
//         if(enter)
//         {
//             enter_proximity_gate(st_dev.stc.ctp_run_status);
//         }
//         else
//         {
//             leave_proximity_gate(st_dev.stc.ctp_run_status);
//         }

//         ret = semi_touch_proximity_switch(enter);
//         check_return_if_fail(ret, NULL);

//         kernel_log_d("proximity %s...\n", enter ? "enter" : "leave");
//     }

//     return ret;
// }

int semi_touch_proximity_operate(void* self, uint32_t command, void* buff_in, int size_in, void* buff_out, int size_out, int* actualout)
{
    int ret = -SEMI_DRV_INVALID_CMD;
    int value = 0;
    //struct hwm_sensor_data *sensor_data;

    switch (command)
    {
        case SENSOR_DELAY:
            if((buff_out == NULL) || (size_out< sizeof(int)))
            {
                ret = -SEMI_DRV_INVALID_PARAM;
            }
            else
            {
                ret = SEMI_DRV_ERR_OK;  //do nothing
            }
            break;
        case SENSOR_ENABLE:
            if((buff_in == NULL) || (size_in < sizeof(int)))
            {
                ret = -SEMI_DRV_INVALID_PARAM;
            }
            else
            {                
                value = *(int*)buff_in;
                ret = semi_touch_proximity_switch(value > 0);
            }
            break;
        case SENSOR_GET_DATA:
            if((buff_out == NULL) || (size_out < sizeof(struct hwm_sensor_data)))
            {
                ret = -SEMI_DRV_INVALID_PARAM;
            }
            else
            {
                //trigger mode, so not needed
                //sensor_data = (struct hwm_sensor_data *)buff_out;
                //sensor_data->values[0] = 0;
                //sensor_data->value_divide = 1;
                //sensor_data->status = SENSOR_STATUS_ACCURACY_MEDIUM;
            }
            break;
        default:
            break;
    }

    check_return_if_fail(ret, NULL);

    return ret;
}

int semi_touch_proximity_init(void)
{
    int ret = 0;

    open_proximity_function(st_dev.stc.custom_function_en);
    
    sm_obj_ps.polling = 0;   //0--interrupt mode;1--polling mode;
    sm_obj_ps.sensor_operate = semi_touch_proximity_operate;

    ret = hwmsen_attach(ID_PROXIMITY, &sm_obj_ps);
    check_return_if_fail(ret, NULL);

    return ret;
}

bool semi_touch_proximity_report(unsigned char proximity)
{
    struct hwm_sensor_data sensor_data;

    if(is_proximity_function_en(st_dev.stc.custom_function_en))
    {
        sensor_data.values[0] = proximity;
        sensor_data.value_divide = 1;
        sensor_data.status = SENSOR_STATUS_ACCURACY_MEDIUM;

        hwmsen_get_interrupt_data(ID_PROXIMITY, &sensor_data);
    }

    return true;
}

int semi_touch_proximity_stop(void)
{
    hwmsen_detach(ID_PROXIMITY);

    return 0;
}
#endif

/********************************************************************************************************************************/
/*mtk touch screen device*/
static const struct of_device_id sm_of_match[] = 
{
    {.compatible = "mediatek,cap_touch", },
    {}
};

static const struct i2c_device_id sm_ts_id[] = 
{
    {CHSC_DEVICE_NAME, 0},
    {}
};

static ssize_t cts_touch_gesture_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int databuf[1]={0};
	if (1 == sscanf(buf,"%x",&databuf[0]))
	{
		if(databuf[0]==1)
		{
			
			open_guesture_function(st_dev.stc.custom_function_en);
		}
		else
		{
			close_guesture_function(st_dev.stc.custom_function_en); 
		}
	}
	return count;
}

static DEVICE_ATTR(cts_gesture_mode, S_IRUGO | S_IWUSR, NULL , cts_touch_gesture_store);

static ssize_t cts_touch_glove_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int databuf[1]={0};
	if (1 == sscanf(buf,"%x",&databuf[0]))
	{
		if(databuf[0]==1)
		{
			open_glove_function(st_dev.stc.custom_function_en);
			semi_touch_glove_switch(1);
		}
		else
		{
			semi_touch_glove_switch(0);
			close_glove_function(st_dev.stc.custom_function_en); 
		}
	}
	
	
	
	return count;
}

static DEVICE_ATTR(cts_glove_mode, S_IRUGO | S_IWUSR, NULL , cts_touch_glove_store);

static struct attribute *cts_dev_gesture_attrs[] = {
    &dev_attr_cts_gesture_mode.attr,
    NULL
};
static struct attribute *cts_dev_glove_attrs[] = {
    &dev_attr_cts_glove_mode.attr,
    NULL
};
static const struct attribute_group cts_dev_gesture_attr_group = {
    .name  = "cts_gesture_mode",
    .attrs = cts_dev_gesture_attrs,
};
static const struct attribute_group cts_dev_glove_attr_group = {
    .name  = "cts_glove_mode",
    .attrs = cts_dev_glove_attrs,
};
static const struct attribute_group *cts_dev_attr_groups[] = {
    &cts_dev_gesture_attr_group,
    &cts_dev_glove_attr_group,
    NULL
};

static int semi_touch_probe(struct i2c_client *client)
{
    int ret = 0;
	//add for 9375 mode TP information start
	int len;
	unsigned char tp_info[512];
	int rgk_flag;
	u8 rgk_buf[4]={0x00};
	//u8 rgk_write_buf[4]={0x00};
	int rgk_num=3;
	//struct device_node *node = NULL; //crystal
	//add for 9375 mode TP information end

	client->addr = 0x2e;
    ret = semi_touch_init(client);
    if(-SEMI_DRV_ERR_HAL_IO == ret)
    {
        semi_touch_deinit(client);
        check_return_if_fail(ret, NULL);
    }

    tpd_button_setting(st_dev.stc.vkey_num, st_dev.stc.vkey_evt_arr, st_dev.stc.vkey_dim_map);

    tpd_load_status = 1;

    kernel_log_d("probe finished(result:%d) driver ver(%s)\r\n", ret, CHSC_DRIVER_VERSION);
	
	ret = sysfs_create_group(&client->dev.kobj, cts_dev_attr_groups[0]);
    ret = sysfs_create_group(&client->dev.kobj, cts_dev_attr_groups[1]);
	
	//add for 9375 mode TP information start
	rgk_buf[0]=0x3;
	//semi_touch_write_bytes(0x2e, rgk_buf, 4);
	rgk_flag=semi_touch_read_bytes(0x20000080, rgk_buf, 4);
	printk("rgk_buf[0] = %d, rgk_buf[1] = %d, rgk_buf[2] = %d, rgk_buf[3] = %d, \n", rgk_buf[0], rgk_buf[1], rgk_buf[2], rgk_buf[3]);
	
	while(rgk_flag<0)
	{
		rgk_flag=semi_touch_read_bytes(0x20000080, rgk_buf, 4);
		printk("rgk_buf[0] = %d, rgk_buf[1] = %d, rgk_buf[2] = %d, rgk_buf[3] = %d, \n", rgk_buf[0], rgk_buf[1], rgk_buf[2], rgk_buf[3]);
		rgk_num--;
		if(rgk_num<=0)
		break;
	}

	len=sprintf(tp_info,"TP IC:%s,TP MOUDLE:%s,TP I2C ADR:0x%x,SW FirmWare:0x%02x,Sample FirmWare:0x%02x,","chsc5448","CHSC",0x2e,rgk_buf[1],rgk_buf[1]);
	
	fix_tp_proc_info(tp_info,len);
	//add for 9375 mode TP information end

    return ret;
}

static void semi_touch_remove(struct i2c_client *client)
{

    semi_touch_deinit(client);

    kernel_log_d("semitouch remove complete\r\n");
}

static int semi_touch_i2c_detect(struct i2c_client *client, struct i2c_board_info *info)
{
    strcpy(info->type, TPD_DEVICE);

    return 0;
}

static struct i2c_driver sm_touch_driver = 
{
    .driver = 
    {
        .owner = THIS_MODULE,
        .name  = "semi_touch",
        .of_match_table = of_match_ptr(sm_of_match),
    },
    .id_table = sm_ts_id,
    .probe = semi_touch_probe,
    .remove = semi_touch_remove,
    .detect = semi_touch_i2c_detect,
};

int semi_touch_power_init(void)
{

    int ret;
    /*set TP volt*/
    tpd->reg = regulator_get(tpd->tpd_dev, "vtouch");
    ret = regulator_set_voltage(tpd->reg, 2800000, 2800000);
    check_return_if_fail(ret, NULL);

    ret = regulator_enable(tpd->reg);
    check_return_if_fail(ret, NULL);

    return 0;
}

static int semi_touch_local_init(void)
{
    int ret = 0;

    ret = semi_touch_power_init();
    check_return_if_fail(ret, NULL);

    ret = i2c_add_driver(&sm_touch_driver);
    check_return_if_fail(ret, NULL);

    return ret;
}

void semi_touch_suspend_entry(struct device* dev)
{
    //struct i2c_client *client = st_dev.client;

    if(is_proximity_function_en(st_dev.stc.custom_function_en))
    {
        if(is_proximity_activate(st_dev.stc.ctp_run_status))
        {
            kernel_log_d("proximity is active, so fake suspend...");
            return;
        }
    }

    if(is_guesture_function_en(st_dev.stc.custom_function_en))
    {
        semi_touch_guesture_switch(1);
        enable_irq_wake(st_dev.client->irq);
    }
    else
    {
        semi_touch_suspend_ctrl(1);
        semi_touch_clear_report();
        //disable_irq(client->irq);
        kernel_log_d("tpd real suspend...\n");
    }
}

void semi_touch_resume_entry(struct device* dev)
{
    unsigned char bootCheckOk = 0;
    unsigned char glove_activity = is_glove_activate(st_dev.stc.ctp_run_status);

    if(is_proximity_function_en(st_dev.stc.custom_function_en))
    {
        if(is_proximity_activate(st_dev.stc.ctp_run_status))
        {
            kernel_log_d("proximity is active, so fake resume...");
            return;
        }
    }
    if(is_guesture_function_en(st_dev.stc.custom_function_en))
    {
        disable_irq_wake(st_dev.client->irq);
    }

    //reset tp + iic detected
    semi_touch_reset_and_detect();
    //set_status_pointing(st_dev.stc.ctp_run_status);
    semi_touch_clear_report();
    //enable_irq(client->irq);

    if(glove_activity)
    {
        semi_touch_start_up_check(&bootCheckOk, only_sp_check);
        if(bootCheckOk)
        {
            semi_touch_glove_switch(1);
        }
    }
    kernel_log_d("tpd_resume...\r\n");
}

static struct tpd_driver_t tpd_device_driver = 
{
    .tpd_device_name = CHSC_DEVICE_NAME,
    .tpd_local_init = semi_touch_local_init,
    .suspend = semi_touch_suspend_entry,
    .resume = semi_touch_resume_entry,

};

int tpd_driver_init(void)
{
    int ret = 0;

    tpd_get_dts_info();
    ret = tpd_driver_add(&tpd_device_driver);
    check_return_if_fail(ret, NULL);

    return ret;
}
EXPORT_SYMBOL_GPL(tpd_driver_init);

void tpd_driver_exit(void)
{
    tpd_driver_remove(&tpd_device_driver);
}
EXPORT_SYMBOL_GPL(tpd_driver_exit);