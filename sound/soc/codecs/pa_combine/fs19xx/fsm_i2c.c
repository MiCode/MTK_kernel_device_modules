/*
 * Copyright (C) Fourier Semiconductor Inc. 2016-2020. All rights reserved.
 */
#include "fsm_public.h"
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/version.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_gpio.h>
#endif
#if defined(CONFIG_REGULATOR)
#include <linux/regulator/consumer.h>
static struct regulator *g_fsm_vdd = NULL;
#endif

static DEFINE_MUTEX(g_fsm_mutex);
static struct device *g_fsm_pdev = NULL;

/* customize configrature */
#include "fsm_firmware.c"
#include "fsm_class.c"
#include "fsm_misc.c"
#include "fsm_q6afe.c"
#include "fsm_mtk_ipi.c"
#include "fsm_codec.c"
#include  "../../../mediatek/common/mtk-sp-spk-amp.h"

void fsm_mutex_lock(void)
{
	mutex_lock(&g_fsm_mutex);
}

void fsm_mutex_unlock(void)
{
	mutex_unlock(&g_fsm_mutex);
}

int fsm_i2c_reg_read(fsm_dev_t *fsm_dev, uint8_t reg, uint16_t *pVal)
{
	struct i2c_msg msgs[2];
	uint8_t retries = 0;
	uint8_t buffer[2];
	int ret;

	if (!fsm_dev || !fsm_dev->i2c || !pVal) {
		return -EINVAL;
	}

	// write register address.
	msgs[0].addr = fsm_dev->i2c->addr;
	msgs[0].flags = 0;
	msgs[0].len = 1;
	msgs[0].buf = &reg;
	// read register buffer.
	msgs[1].addr = fsm_dev->i2c->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = 2;
	msgs[1].buf = &buffer[0];

	do {
		mutex_lock(&fsm_dev->i2c_lock);
		ret = i2c_transfer(fsm_dev->i2c->adapter, &msgs[0], ARRAY_SIZE(msgs));
		mutex_unlock(&fsm_dev->i2c_lock);
		if (ret != ARRAY_SIZE(msgs)) {
			fsm_delay_ms(5);
			retries++;
		}
	} while (ret != ARRAY_SIZE(msgs) && retries < FSM_I2C_RETRY);

	if (ret != ARRAY_SIZE(msgs)) {
		pr_err("read %02x transfer error: %d", reg, ret);
		return -EIO;
	}

	*pVal = ((buffer[0] << 8) | buffer[1]);

	return 0;
}

int fsm_i2c_reg_write(fsm_dev_t *fsm_dev, uint8_t reg, uint16_t val)
{
	struct i2c_msg msgs[1];
	uint8_t retries = 0;
	uint8_t buffer[3];
	int ret;

	if (!fsm_dev || !fsm_dev->i2c) {
		return -EINVAL;
	}

	buffer[0] = reg;
	buffer[1] = (val >> 8) & 0x00ff;
	buffer[2] = val & 0x00ff;
	msgs[0].addr = fsm_dev->i2c->addr;
	msgs[0].flags = 0;
	msgs[0].len = sizeof(buffer);
	msgs[0].buf = &buffer[0];

	do {
		mutex_lock(&fsm_dev->i2c_lock);
		ret = i2c_transfer(fsm_dev->i2c->adapter, &msgs[0], ARRAY_SIZE(msgs));
		mutex_unlock(&fsm_dev->i2c_lock);
		if (ret != ARRAY_SIZE(msgs)) {
			fsm_delay_ms(5);
			retries++;
		}
	} while (ret != ARRAY_SIZE(msgs) && retries < FSM_I2C_RETRY);

	if (ret != ARRAY_SIZE(msgs)) {
		pr_err("write %02x transfer error: %d", reg, ret);
		return -EIO;
	}

	return 0;
}

int fsm_i2c_bulkwrite(fsm_dev_t *fsm_dev, uint8_t reg,
				uint8_t *data, int len)
{
	uint8_t retries = 0;
	uint8_t *buf;
	int size;
	int ret;

	if (!fsm_dev || !fsm_dev->i2c || !data) {
		return -EINVAL;
	}

	size = sizeof(uint8_t) + len;
	buf = (uint8_t *)fsm_alloc_mem(size);
	if (!buf) {
		pr_err("alloc memery failed");
		return -ENOMEM;
	}

	buf[0] = reg;
	memcpy(&buf[1], data, len);
	do {
		mutex_lock(&fsm_dev->i2c_lock);
		ret = i2c_master_send(fsm_dev->i2c, buf, size);
		mutex_unlock(&fsm_dev->i2c_lock);
		if (ret < 0) {
			fsm_delay_ms(5);
			retries++;
		} else if (ret == size) {
			break;
		}
	} while (ret != size && retries < FSM_I2C_RETRY);

	fsm_free_mem((void **)&buf);

	if (ret != size) {
		pr_err("write %02x transfer error: %d", reg, ret);
		return -EIO;
	}

	return 0;
}

bool fsm_set_pdev(struct device *dev)
{
	if (g_fsm_pdev == NULL || dev == NULL) {
		g_fsm_pdev = dev;
		// pr_debug("dev_name: %s", dev_name(dev));
		return true;
	}
	return false; // already got device
}

struct device *fsm_get_pdev(void)
{
	return g_fsm_pdev;
}

int fsm_vddd_on(struct device *dev)
{
	fsm_config_t *cfg = fsm_get_config();
	int ret = 0;

	if (!cfg || cfg->vddd_on) {
		return 0;
	}
#if defined(CONFIG_REGULATOR)
	g_fsm_vdd = regulator_get(dev, "fsm_vddd");
	if (IS_ERR(g_fsm_vdd) != 0) {
		pr_err("error getting fsm_vddd regulator");
		ret = PTR_ERR(g_fsm_vdd);
		g_fsm_vdd = NULL;
		return ret;
	}
	pr_info("enable regulator");
	regulator_set_voltage(g_fsm_vdd, 1800000, 1800000);
	ret = regulator_enable(g_fsm_vdd);
	if (ret < 0) {
		pr_err("enabling fsm_vddd failed: %d", ret);
	}
#endif
	cfg->vddd_on = 1;
	fsm_delay_ms(10);

	return ret;
}

void fsm_vddd_off(void)
{
	fsm_config_t *cfg = fsm_get_config();

	if (!cfg || !cfg->vddd_on || cfg->dev_count > 0) {
		return;
	}
#if defined(CONFIG_REGULATOR)
	if (g_fsm_vdd) {
		pr_info("disable regulator");
		regulator_disable(g_fsm_vdd);
		regulator_put(g_fsm_vdd);
		g_fsm_vdd = NULL;
	}
#endif
	cfg->vddd_on = 0;
}

int fsm_get_amb_tempr(void)
{
	union power_supply_propval psp = { 0 };
	struct power_supply *psy;
	int tempr = FSM_DFT_AMB_TEMPR;
	int vbat = FSM_DFT_AMB_VBAT;

	psy = power_supply_get_by_name("battery");
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 0))
	if (psy && psy->get_property) {
		// battery temperatrue
		psy->get_property(psy, POWER_SUPPLY_PROP_TEMP, &psp);
		tempr = DIV_ROUND_CLOSEST(psp.intval, 10);
		psy->get_property(psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &psp);
		vbat = DIV_ROUND_CLOSEST(psp.intval, 1000);
	}
#else
	if (psy && psy->desc && psy->desc->get_property) {
		// battery temperatrue
		psy->desc->get_property(psy, POWER_SUPPLY_PROP_TEMP, &psp);
		tempr = DIV_ROUND_CLOSEST(psp.intval, 10);
		psy->desc->get_property(psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &psp);
		vbat = DIV_ROUND_CLOSEST(psp.intval, 1000);
	}
#endif
	pr_info("vbat:%d, tempr:%d", vbat, tempr);

	return tempr;
}

void *fsm_devm_kstrdup(struct device *dev, void *buf, size_t size)
{
	char *devm_buf = devm_kzalloc(dev, size + 1, GFP_KERNEL);

	if (!devm_buf) {
		return devm_buf;
	}
	memcpy(devm_buf, buf, size);

	return devm_buf;
}

static int fsm_set_irq(fsm_dev_t *fsm_dev, bool enable)
{
	if (!fsm_dev || fsm_dev->irq_id <= 0) {
		return -EINVAL;
	}
	if (enable)
		enable_irq(fsm_dev->irq_id);
	else
		disable_irq(fsm_dev->irq_id);

	return 0;
}

int fsm_set_monitor(fsm_dev_t *fsm_dev, bool enable)
{
	fsm_config_t *cfg = fsm_get_config();

	if (!cfg || !fsm_dev || !fsm_dev->fsm_wq) {
		return -EINVAL;
	}
	if (!cfg->use_monitor) {
		return 0;
	}
	if (fsm_dev->use_irq) {
		return fsm_set_irq(fsm_dev, enable);
	}
	if (enable) {
		queue_delayed_work(fsm_dev->fsm_wq,
				&fsm_dev->monitor_work, 5*HZ);
	}
	else {
		if (delayed_work_pending(&fsm_dev->monitor_work)) {
			cancel_delayed_work_sync(&fsm_dev->monitor_work);
		}
	}

	return 0;
}

static int fsm_ext_reset(fsm_dev_t *fsm_dev)
{
	fsm_config_t *cfg = fsm_get_config();

	if (!cfg || !fsm_dev) {
		return -EINVAL;
	}
	// if (cfg->reset_chip) {
	// 	return 0;
	// }
	if (gpio_is_valid(fsm_dev->rst_gpio)) {
		gpio_set_value(fsm_dev->rst_gpio, 0);
		fsm_delay_ms(10); // mdelay
		gpio_set_value(fsm_dev->rst_gpio, 1);
		fsm_delay_ms(1); // mdelay
		cfg->reset_chip = true;
	}

	return 0;
}

static irqreturn_t fsm_irq_hander(int irq, void *data)
{
	fsm_dev_t *fsm_dev = data;

	queue_delayed_work(fsm_dev->fsm_wq, &fsm_dev->interrupt_work, 0);

	return IRQ_HANDLED;
}

static void fsm_work_monitor(struct work_struct *work)
{
	fsm_config_t *cfg = fsm_get_config();
	fsm_dev_t *fsm_dev;
	//int ret;

	fsm_dev = container_of(work, struct fsm_dev, monitor_work.work);
	if (!cfg || cfg->skip_monitor || !fsm_dev) {
		return;
	}
	fsm_mutex_lock();
	//ret = fsm_dev_recover(fsm_dev);
	fsm_mutex_unlock();
	if (fsm_dev->rec_count >= 5) { // 5 time max
		pr_addr(err, "recover max time, stop it");
		return;
	}
	/* reschedule */
	queue_delayed_work(fsm_dev->fsm_wq, &fsm_dev->monitor_work,
			5*HZ);

}

static void fsm_work_interrupt(struct work_struct *work)
{
	fsm_config_t *cfg = fsm_get_config();
	fsm_dev_t *fsm_dev;
	//int ret;

	fsm_dev = container_of(work, struct fsm_dev, interrupt_work.work);
	if (!cfg || cfg->skip_monitor || !fsm_dev) {
		return;
	}
	fsm_mutex_lock();
	//ret = fsm_dev_recover(fsm_dev);
	//fsm_get_spkr_tempr(fsm_dev);

	fsm_mutex_unlock();
}

static int fsm_request_irq(fsm_dev_t *fsm_dev)
{
	struct i2c_client *i2c;
	int irq_flags;
	int ret;

	if (fsm_dev == NULL || fsm_dev->i2c == NULL) {
		return -EINVAL;
	}
	fsm_dev->irq_id = -1;
	if (!fsm_dev->use_irq || !gpio_is_valid(fsm_dev->irq_gpio)) {
		pr_addr(info, "skip to request irq");
		return 0;
	}
	i2c = fsm_dev->i2c;
	/* register irq handler */
	fsm_dev->irq_id = gpio_to_irq(fsm_dev->irq_gpio);
	if (fsm_dev->irq_id <= 0) {
		dev_err(&i2c->dev, "invalid irq %d\n", fsm_dev->irq_id);
		return -EINVAL;
	}
	irq_flags = IRQF_TRIGGER_FALLING | IRQF_ONESHOT;
	ret = devm_request_threaded_irq(&i2c->dev, fsm_dev->irq_id,
				NULL, fsm_irq_hander, irq_flags, "fs19xx", fsm_dev);
	if (ret) {
		dev_err(&i2c->dev, "failed to request IRQ %d: %d\n",
				fsm_dev->irq_id, ret);
		return ret;
	}
	disable_irq(fsm_dev->irq_id);

	return 0;
}

#ifdef CONFIG_OF
static int fsm_parse_dts(struct i2c_client *i2c, fsm_dev_t *fsm_dev)
{
	struct device_node *np = i2c->dev.of_node;
	int ret;
	char *str_name;
	fsm_config_t *cfg = fsm_get_config();
	int spksw_gpio;

	if (fsm_dev == NULL || np == NULL) {
		return -EINVAL;
	}

	str_name = devm_kzalloc(&i2c->dev, 32, GFP_KERNEL);
	fsm_dev->rst_gpio = of_get_named_gpio(np, "reset-gpio", 0);
	pr_info("rst_gpio = %d \n", fsm_dev->rst_gpio);
	if (gpio_is_valid(fsm_dev->rst_gpio)) {
		snprintf(str_name, 32, "FS19XX_RST_%02X", i2c->addr);
		ret = devm_gpio_request_one(&i2c->dev, fsm_dev->rst_gpio,
			GPIOF_OUT_INIT_LOW, str_name);
#if IS_ENABLED(CONFIG_SND_SMARTPA_AW882XX_GPIO_CONFIG)
        /*2 PA use same reset GPIO, GPIO request fail don't return*/
                if(ret){
                   pr_info("FS19XX_RST_%02X request failed,2 PA use same rst_gpio,don't return", i2c->addr);
                }
#else
		if (ret){
			pr_err("FS19XX_RST_%02X request failed", i2c->addr);
			return ret;
		}
#endif
	}else{
		pr_info("0x%02x: fsm,rst-gpio cannot find", i2c->addr);
	}

	fsm_dev->irq_gpio = of_get_named_gpio(np, "irq-gpio", 0);
	if (gpio_is_valid(fsm_dev->irq_gpio)) {
		ret = devm_gpio_request_one(&i2c->dev, fsm_dev->irq_gpio,
			GPIOF_OUT_INIT_LOW, "FS19XX_IRQ");
		if (ret)
			return ret;
	}
	ret = of_property_read_u32(np, "fsm,re25-dft", &fsm_dev->re25_dft);
	if (ret) {
		fsm_dev->re25_dft = 0;
	}
	pr_info("re25 default:%d", fsm_dev->re25_dft);

	spksw_gpio = of_get_named_gpio(np, "spk-sw-gpio", 0);
	pr_info("spksw_gpio = %d \n", spksw_gpio);
	if (gpio_is_valid(spksw_gpio)) {
		ret = devm_gpio_request_one(&i2c->dev, spksw_gpio,
			GPIOF_OUT_INIT_LOW, "FS19XX_SPKSW");
		if (ret){
			pr_err("0x%02x: spksw-gpio request failed", i2c->addr);
			return ret;
		}
		cfg->spksw_gpio = spksw_gpio;
	}else{
		pr_info("0x%02x: spksw-gpio cannot find", i2c->addr);
	}

	return 0;
}


static struct of_device_id fsm_match_tbl[] =
{
	{ .compatible = "foursemi,fs16xx" },
	{},
};
MODULE_DEVICE_TABLE(of, fsm_match_tbl);

#endif

static int fsm_i2c_probe(struct i2c_client *i2c)
{
	fsm_config_t *cfg = fsm_get_config();
	fsm_dev_t *fsm_dev;
	int ret = 0;

	if (!i2c_check_functionality(i2c->adapter, I2C_FUNC_I2C)) {
		dev_err(&i2c->dev, "check I2C_FUNC_I2C failed");
		return -EIO;
	}

	if (!check_smartpa_type("fs19xx")) {
		dev_err(&i2c->dev, "other smartpa type already set, no need to probe");
		return 0;
	}

	fsm_dev = devm_kzalloc(&i2c->dev, sizeof(struct fsm_dev), GFP_KERNEL);
	if (fsm_dev == NULL) {
		dev_err(&i2c->dev, "alloc memory fialed");
		return -ENOMEM;
	}

	memset(fsm_dev, 0, sizeof(struct fsm_dev));
	mutex_init(&fsm_dev->i2c_lock);
	fsm_dev->i2c = i2c;

#ifdef CONFIG_OF
	ret = fsm_parse_dts(i2c, fsm_dev);
	if (ret) {
		dev_err(&i2c->dev, "failed to parse DTS node");
	}
#endif
#if defined(CONFIG_FSM_REGMAP)
	fsm_dev->regmap = fsm_regmap_i2c_init(i2c);
	if (fsm_dev->regmap == NULL) {
		devm_kfree(&i2c->dev, fsm_dev);
		dev_err(&i2c->dev, "regmap init fialed");
		return -EINVAL;
	}
#endif

	fsm_vddd_on(&i2c->dev);
	fsm_ext_reset(fsm_dev);
	ret = fsm_probe(fsm_dev, i2c->addr);
	if (ret) {
		dev_err(&i2c->dev, "detect device failed");
#if defined(CONFIG_FSM_REGMAP)
		fsm_regmap_i2c_deinit(fsm_dev->regmap);
#endif
		fsm_vddd_off();
		if (gpio_is_valid(fsm_dev->rst_gpio)) {
			gpio_set_value(fsm_dev->rst_gpio, 1);
		}
		devm_kfree(&i2c->dev, fsm_dev);
		return ret;
	}
	fsm_dev->id = cfg->dev_count - 1;
	i2c_set_clientdata(i2c, fsm_dev);
	pr_addr(info, "index:%d", fsm_dev->id);
	fsm_dev->fsm_wq = create_singlethread_workqueue("fs19xx");
	INIT_DELAYED_WORK(&fsm_dev->monitor_work, fsm_work_monitor);
	INIT_DELAYED_WORK(&fsm_dev->interrupt_work, fsm_work_interrupt);
	fsm_request_irq(fsm_dev);

	set_smartpa_type("fs19xx", sizeof("fs19xx"));
	mtk_spk_set_type(MTK_SPK_FS_FS19xx);

	if(fsm_dev->id == 0) {
		// reigster only in the first device
#if !defined(CONFIG_FSM_CODEC)
		fsm_set_pdev(&i2c->dev);
#endif
		fsm_misc_init();
		fsm_sysfs_init(&i2c->dev);
		fsm_codec_register(&i2c->dev, fsm_dev->id);
	}
	dev_info(&i2c->dev, "i2c probe completed");
	return 0;
}

static void fsm_i2c_remove(struct i2c_client *i2c)
{
	fsm_dev_t *fsm_dev = i2c_get_clientdata(i2c);

	pr_debug("enter");
	if (fsm_dev == NULL) {
		pr_err("bad parameter");
		return;
	}
	if (fsm_dev->fsm_wq) {
		cancel_delayed_work_sync(&fsm_dev->interrupt_work);
		cancel_delayed_work_sync(&fsm_dev->monitor_work);
		destroy_workqueue(fsm_dev->fsm_wq);
	}
#if defined(CONFIG_FSM_REGMAP)
	fsm_regmap_i2c_deinit(fsm_dev->regmap);
#endif
	if (fsm_dev->id == 0) {
		fsm_codec_unregister(&i2c->dev);
		fsm_sysfs_deinit(&i2c->dev);
		fsm_misc_deinit();
		fsm_set_pdev(NULL);
	}

	fsm_remove(fsm_dev);
	fsm_vddd_off();
	devm_kfree(&i2c->dev, fsm_dev);
	dev_info(&i2c->dev, "i2c removed");
}

int exfsm_i2c_probe(struct i2c_client *i2c)
{
	return fsm_i2c_probe(i2c);
}
EXPORT_SYMBOL(exfsm_i2c_probe);

void exfsm_i2c_remove(struct i2c_client *i2c)
{
	fsm_i2c_remove(i2c);
}
EXPORT_SYMBOL(exfsm_i2c_remove);

static const struct i2c_device_id fsm_i2c_id[] =
{
	{ "fs16xx", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, fsm_i2c_id);

