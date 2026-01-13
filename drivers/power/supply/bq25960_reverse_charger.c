
// SPDX-License-Identifier: GPL-2.0
/*
* Copyright (c) 2022 Southchip Semiconductor Technology(Shanghai) Co., Ltd.
*/
#include "bq25960.h"

static struct bq25960 *g_bq;
/************************************I2C COMMON API************************************/
__maybe_unused
static int __bq25960_read_byte(struct bq25960 *bq, u8 reg, u8 *data)
{
	s32 ret;
	ret = i2c_smbus_read_byte_data(bq->client, reg);
	if (ret < 0) {
		dev_err(bq->dev, "i2c read fail: can't read from reg 0x%02X\n", reg);
		return ret;
	}
	*data = (u8) ret;
	return 0;
}
__maybe_unused
int bq25960_read_byte(struct bq25960 *bq, u8 reg, u8 *data)
{
	int ret;
	//if (bq->skip_reads) {
	//	*data = 0;
	//	return 0;
	//}
	mutex_lock(&bq->i2c_rw_lock);
	ret = __bq25960_read_byte(bq, reg, data);
	mutex_unlock(&bq->i2c_rw_lock);
	return ret;
}
__maybe_unused
static int __bq25960_write_byte(struct bq25960 *bq, int reg, u8 val)
{
	s32 ret;
	ret = i2c_smbus_write_byte_data(bq->client, reg, val);
	if (ret < 0) {
		dev_err(bq->dev, "i2c write fail: can't write 0x%02X to reg 0x%02X: %d\n", val, reg, ret);
		return ret;
	}
	return 0;
}
__maybe_unused
static int bq25960_write_byte(struct bq25960 *bq, u8 reg, u8 data)
{
	int ret;
	//if (bq->skip_writes)
	//	return 0;
	mutex_lock(&bq->i2c_rw_lock);
	ret = __bq25960_write_byte(bq, reg, data);
	mutex_unlock(&bq->i2c_rw_lock);
	return ret;
}
__maybe_unused
static int bq25960_update_bits(struct bq25960 *bq, u8 reg,
				u8 mask, u8 data)
{
	int ret;
	u8 tmp;
	//if (bq->skip_reads || bq->skip_writes)
	//	return 0;
	mutex_lock(&bq->i2c_rw_lock);
	ret = __bq25960_read_byte(bq, reg, &tmp);
	if (ret) {
		dev_err(bq->dev, "Read Failed: reg=%02X, ret=%d\n", reg, ret);
		goto out;
	}
	tmp &= ~mask;
	tmp |= data & mask;
	ret = __bq25960_write_byte(bq, reg, tmp);
	if (ret)
		dev_err(bq->dev, "Write Failed: reg=%02X, ret=%d\n", reg, ret);
out:
	mutex_unlock(&bq->i2c_rw_lock);
	return ret;
}
/************************************I2C COMMON API************************************/
__maybe_unused static int bq25960_dump_reg(struct bq25960 *bq)
{
    int ret;
    int i;
    int val;
    for (i = 0; i <= BQ25960_REGMAX; i++) {
        ret = regmap_read(bq->regmap, i, &val);
        dev_info(bq->dev, "%s reg[0x%02x] = 0x%02x\n",
                __func__, i, val);
    }
    return ret;
}

static void bq25960h_set_rcp_workfunc(struct work_struct *work)
{
	int ret = 0;
	int val = 0;

	ret = bq25960_write_byte(g_bq, BQ25960_REG_05, 0x9E);
	if (ret != 0) {
		pr_err("%s [BQ25960H]:  regmap_write 0x05 fail\n", __func__);
	}

	//[REVCHG] For debug
	ret = regmap_read(g_bq->regmap, BQ25960_REG_0F, &val);
	if (ret)
		return;
	pr_err("%s [REVCHG]: reg[0x0f] = 0x%02x\n", __func__, val);

	ret = regmap_read(g_bq->regmap, BQ25960_REG_FA, &val);
	if (ret)
		return;
	pr_err("%s [REVCHG]: in 9V reg[0xFA] = 0x%02x\n", __func__, val);

	ret = regmap_read(g_bq->regmap, BQ25960_REG_9A, &val);
	if (ret)
		return;
	pr_err("%s [REVCHG]: in 9V reg[0x9A] = 0x%02x\n", __func__, val);

	ret = regmap_read(g_bq->regmap, BQ25960_REG_9B, &val);
	if (ret)
		return;
	pr_err("%s [REVCHG]: in 9V reg[0x9B] = 0x%02x\n", __func__, val);

	bq25960_dump_reg(g_bq);

	pr_err("%s [BQ25960H] the end\n", __func__);
}

int bq25960h_set_otg_preconfigure(bool en)
{
	int ret = 0;
	int val = 0;
	pr_err("%s [BQ25960H]\n", __func__);
	if (en) { //i2c addr:0x3f
		ret = bq25960_write_byte(g_bq, BQ25960_REG_F9, 0x40);	//reverse config
		ret |= bq25960_write_byte(g_bq, BQ25960_REG_FA, 0x31);	//reverse config
		//turn on vac1_ovpfet to output 5v
		ret |= bq25960_write_byte(g_bq, BQ25960_REG_A8, 0xD7);

	}

	//[REVCHG] for debug
	ret = regmap_read(g_bq->regmap, BQ25960_REG_F9, &val);
	if (ret)
		return ret;
	pr_err("%s [REVCHG]: in 5V reg[0xF9] = 0x%02x\n", __func__, val);

	ret = regmap_read(g_bq->regmap, BQ25960_REG_A8, &val);
	if (ret)
		return ret;
	pr_err("%s [REVCHG]: in 5V reg[0xA8] = 0x%02x\n", __func__, val);

	//bq25960_dump_reg(g_bq);
	pr_err("%s [BQ25960H] in end\n", __func__);

	return ret;
}
EXPORT_SYMBOL(bq25960h_set_otg_preconfigure);

int bq25960h_enable_otg(bool en)
{
	int ret = 0;
	int val = 0; //add for log
	int i = 5;

	pr_err("%s [BQ25960H] I2C address:0x%x, enable is %d\n", __func__, g_bq->client->addr, en);
	if (en) {
		mdelay(10);
		ret = bq25960_write_byte(g_bq, BQ25960_REG_A8, 0x00);//reset A8

		//disable the busucp and busrcp
		ret |= bq25960_write_byte(g_bq, BQ25960_REG_05, 0xAE);
		//enable cp reverse and output 9V on vac1_ovpfet
		ret |= bq25960_write_byte(g_bq, BQ25960_REG_0F, 0x12);

		mdelay(10);

		while (i--) {
			ret = bq25960_write_byte(g_bq, BQ25960_REG_0F, 0x12);
			if (ret != 0) {
				pr_err("%s regmap_write 0x0F fail \n", __func__);
			}

			ret = regmap_read(g_bq->regmap, BQ25960_REG_0F, &val);
			if (ret == 0) {
				pr_err("%s [BQ25960H] regmap_read 0x0F = 0x%02x\n", __func__, val);
			} else {
				pr_err("%s [BQ25960H] regmap_read REG 0x0F error\n", __func__);
			}

            if (0x12 == val)
                break;

            mdelay(10);
        }

		//recovery to default value for 0xfa register
		ret |= bq25960_write_byte(g_bq, BQ25960_REG_FA, 0x21);
		//set 3A rcp as cp reverse current limit
		ret |= bq25960_write_byte(g_bq, BQ25960_REG_05, 0xBE);

		schedule_delayed_work(&g_bq->set_rcp_work, msecs_to_jiffies(500));
	} else {
		pr_err("%s [BQ25960H] in bq25960_exit_reverse_output\n", __func__);
		//disable cp reverse
		ret = bq25960_write_byte(g_bq, BQ25960_REG_0F, 0x00);
		//configure the busucp and busrcp back to the desired setting
		ret |= bq25960_write_byte(g_bq, BQ25960_REG_05, 0x0E);
		//recover vac1ovp to default 6.5v
		ret |= bq25960_write_byte(g_bq, BQ25960_REG_FA, 0x20);	//exit cp reverse
		ret |= bq25960_write_byte(g_bq, BQ25960_REG_F9, 0x00);	//exit cp reverse
		mdelay(10);
		//exits 0x3f and return to 0x65 i2c 7bit address
		ret |= bq25960_write_byte(g_bq, BQ25960_REG_A0, 0x00);
	}

	pr_err("[BQ25960H] in end of bq25960h_set_otg\n");

	return ret;
}
EXPORT_SYMBOL(bq25960h_enable_otg);

int bq25960_exit_reverse_output(void)
{
	int ret = 0;
	pr_err("%s [REVCHG] in bq25960_exit_reverse_output\n", __func__);
	//disable cp reverse
	ret = bq25960_write_byte(g_bq, BQ25960_REG_0F, 0x00);
	//configure the busucp and busrcp back to the desired setting
	ret |= bq25960_write_byte(g_bq, BQ25960_REG_05, 0x0E);
	//recover vac1ovp to default 6.5v
	//ret |= bq25960_write_byte(g_bq, BQ25960_REG_0E, 0x0c);
	ret |= bq25960_write_byte(g_bq, BQ25960_REG_FA, 0x20);	//exit cp reverse
	ret |= bq25960_write_byte(g_bq, BQ25960_REG_F9, 0);	//exit cp reverse
	mdelay(10);
	//exits 0x3f and return to 0x65 i2c 7bit address
	ret |= bq25960_write_byte(g_bq, BQ25960_REG_A0, 0);
	return ret;
}
EXPORT_SYMBOL(bq25960_exit_reverse_output);

static const struct of_device_id bq25960_charger_match_table[] = {
	{
		.compatible = "ti,bq25960-reverse",
		.data = &bq25960_mode_data[BQ25960_REVERSE],
	},
	{},
};
MODULE_DEVICE_TABLE(of, bq25960_charger_match_table);
static int bq25960_reverse_charger_probe(struct i2c_client *client,
					const struct i2c_device_id *id)
{
	struct bq25960 *bq;
	const struct of_device_id *match;
	struct device_node *node = client->dev.of_node;
	int ret = 0;
	int val = 0;
	bq =  devm_kzalloc(&client->dev, sizeof(*bq), GFP_KERNEL);
	if (!bq) {
		ret = -ENOMEM;
		goto err_1;
	}
	bq->dev = &client->dev;
	bq->client = client;
	bq->regmap = devm_regmap_init_i2c(client,
                            &bq25960_regmap_config);
	if (IS_ERR(bq->regmap)) {
		dev_info(bq->dev, "Failed to initialize regmap\n");
		ret = PTR_ERR(bq->regmap);
	    goto err_regmap_init;
    }



	mutex_init(&bq->i2c_rw_lock);
	i2c_set_clientdata(client, bq);
	match = of_match_node(bq25960_charger_match_table, node);
	if (match == NULL) {
		dev_err(bq->dev, "device tree match not found!\n");

		return -ENODEV;
	}

	g_bq = bq;
	INIT_DELAYED_WORK(&bq->set_rcp_work, bq25960h_set_rcp_workfunc);
	ret = regmap_read(bq->regmap, BQ25960_REG_05, &val);
	if (ret!= 0) {
		dev_err(bq->dev, "%s detect device fail\n", __func__);
		ret = -ENODEV;
	} else {
		bq25960h_enable_otg(false);
	}
	dev_err(bq->dev, "bq25960 reverse probe successfully!\n");
	return 0;


err_regmap_init:
	devm_kfree(&client->dev, bq);
err_1:
	dev_err(bq->dev, "bq25960 reverse probe fail!\n");
	return ret;

}
static int bq25960_suspend(struct device *dev)
{
	pr_info("bq25960 enter suspend!\n");
	return 0;
}
static int bq25960_suspend_noirq(struct device *dev)
{
	pr_info("bq25960 enter suspend_noirq!\n");
	return 0;
}
static int bq25960_resume(struct device *dev)
{
	pr_info("bq25960 enter resume!\n");
	return 0;
}
static void bq25960_charger_remove(struct i2c_client *client)
{
	struct bq25960 *bq = i2c_get_clientdata(client);
	cancel_delayed_work_sync(&bq->set_rcp_work);
	power_supply_unregister(bq->psy);
	devm_kfree(&client->dev, bq);

	return ;
}
static void bq25960_charger_shutdown(struct i2c_client *client)
{
	pr_info("bq25960 enter resume!\n");
}
static const struct dev_pm_ops bq25960_pm_ops = {
	.resume		= bq25960_resume,
	.suspend_noirq = bq25960_suspend_noirq,
	.suspend	= bq25960_suspend,
};
static const struct i2c_device_id bq25960_charger_id[] = {
	{"bq25960-reverse", BQ25960_REVERSE},
	{},
};
MODULE_DEVICE_TABLE(i2c, bq25960_charger_id);
static struct i2c_driver bq25960_charger_driver = {
	.driver		= {
		.name	= "bq25960-reverse-charger",
		.owner	= THIS_MODULE,
		.of_match_table = bq25960_charger_match_table,
		.pm	= &bq25960_pm_ops,
	},
	.id_table	= bq25960_charger_id,
	.probe		= bq25960_reverse_charger_probe,
	.remove		= bq25960_charger_remove,
	.shutdown	= bq25960_charger_shutdown,
};
module_i2c_driver(bq25960_charger_driver);

MODULE_DESCRIPTION("TI BQ25960 Driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("South Chip <Aiden-yu@southchip.com>");
