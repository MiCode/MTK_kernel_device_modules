//#define CONFIG_RUST_DETECTION 1 //for switch debug
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/power_supply.h>
#include <linux/regmap.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/printk.h>
#include <linux/timer.h>
#include <linux/delay.h>

#include <linux/pinctrl/consumer.h>

/*
 * use tcpc dev to detect audio plug in
 */
#include "../typec/tcpc/inc/tcpci_core.h"
#include "../typec/tcpc/inc/tcpm.h"
#include "../../drivers/power/supply/charger_class.h"

enum switch_ic_vendor_type {
		SWITCH_NOT_SUPPORT = 0,
		SWITCH_DIO4485 = 0xF6,
		SWITCH_DIO4480 = 0xF1,
		SWITCH_ET7480 = 0x88,
		SWITCH_HL5281M = 0x40,
};

enum SWITCH_STATUS {
	SWITCH_STATUS_INVALID = 0,
	SWITCH_STATUS_NOT_CONNECTED,
	SWITCH_STATUS_USB_MODE,
	SWITCH_STATUS_HEADSET_MODE,
	SWITCH_STATUS_MAX
};

#if IS_ENABLED(CONFIG_RUST_DETECTION)

#define MOSITRUE_DETECTED 1
#define MOSITRUE_NOT_DETECTED 0
#define MOSITRUE_DETECT_ERROR -1

#define THRESHOLD_CC 1850000  // 1.85v 1850000uV
#define THRESHOLD_SUB 1850000  // 1.85v 1850000uV

#define HL5281M_PARAM 9375  // 9.375*1000
#define DIO4485_PARAM 9000  // 9*1000

#define RES_DET_CONTROL1	0x13 // Detection control0,Moisture detection force off control
#define RES_DET_FUNCTION1	0x16 // Function control1,Select moisture detection mode:single or multi
#define RES_DET_CONTROL2	0x31 // Detection control1
#define RES_DET_THRESHOLD	0x32 // Threshold for moisturedetection with current source disabled

//Moisture detection result in multi-pin mode with current source disabled.
#define RES_DET_DATA5		0x38 // CC
#define RES_DET_DATA8		0x3B // SUB1
#define RES_DET_DATA9		0x3C // SUB2

#endif

char *switch_status_string[SWITCH_STATUS_MAX] = {
	"switch invalid",
	"switch not connected",
	"switch usb mode",
	"switch headset mode",
};

#define TYPEC_SWITCH_I2C_NAME	"typec-switch-driver"

#define TYPEC_SWITCH_REG_MAX_RANGE	0x51 //register map max range
#define DELAY_INIT_TIME				(10 * HZ) //10*1000ms
#define DBG_TYPE_MODE				0
#define DBG_REG_MODE				1
//COMMON REG
#define TYPEC_SWITCH_VENDOR_ID	0x00
#define SWITCH_SETTINGS			0x04
#define SWITCH_CONTROL			0x05
#define FUNCTION_ENABLE			0x12
//DIO INIT
#define DIO_INITREG1				0x4E // init write 8F->5A
#define DIO_INITREG3				0x50 // init write 44
#define DIO_INITREG2				0x51 // init write 90
//SLOW ON
#define L_SLOW_TURN_ON			0x08 // left channel slow turn-on
#define R_SLOW_TURN_ON			0x09 // right channel slow turn-on
#define MIC_SLOW_TURN_ON		0x0A // mic slow turn-on
#define SENSE_SLOW_TURN_ON		0x0B // sense slow turn-on
#define GND_SLOW_TURN_ON		0x0C // Audio ground switch slow turn-on 
//DELAY
#define DELAY_SENSE_TO_SWITCH	0x0F //Timing delay between sense switch enable and switch on order
#define DELAY_AGND_TO_SWITCH	0x10 //Timing delay between audio ground switch enable and switch on order
#define DELAY_MIC_TO_SWITCH		0x0E //Timing delay between MIC switch enable and switch on order
#define DELAY_R_TO_SWITCH		0x0D //Timing delay between R switch enable and switch on order
#define DELAY_L_TO_SWITCH		0x21 //Timing delay between L switch enable and switch on order
//RESET
#define SWITCH_RESET			0x1E //I2C reset
//SYSTEM FLAG
#define SWITCH_DETECTION   0x18 //Jack and Moistrue detection
//MIC DETECTION
#define SWITCH_JACK_STATUS     0x17 //MIC detection status 3 POLE OR 4 POLE
//STATUS REG
#define SWITCH_STATUS0  0x06

static struct timer_list enable_timer;
static struct work_struct enable_typec_switch_work;
static struct workqueue_struct *enable_switch_workqueue;

static struct timer_list delay_init_timer;
static struct work_struct delay_init_work;
static struct workqueue_struct *delay_init_workqueue;

struct ts_priv {
	struct regmap *regmap;
	struct device *dev;
	struct tcpc_device *tcpc_dev;
	struct notifier_block psy_nb;
	struct blocking_notifier_head ts_notifier;
	struct pinctrl *uart_en_gpio_pinctrl;
	struct pinctrl_state *pinctrl_state_enable;
	struct pinctrl_state *pinctrl_state_disable;
	int vendor_type;
	int audio_plug_status;
#if IS_ENABLED(CONFIG_RUST_DETECTION)
	struct charger_device *chgdev;
#endif
};

static struct ts_priv *global_data = NULL;

struct ts_reg_val {
	u16 reg;
	u8 val;
};

static const struct regmap_config switch_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = TYPEC_SWITCH_REG_MAX_RANGE,
};

static const struct ts_reg_val ts_reg_i2c_defaults[] = {
	{SWITCH_SETTINGS, 0x98},//default sbu1 and sbu2 disable
	{SWITCH_CONTROL, 0x18},
//SLOW ON START
	{L_SLOW_TURN_ON, 0x00},
	{R_SLOW_TURN_ON, 0x00},
	{MIC_SLOW_TURN_ON, 0x00},
	{SENSE_SLOW_TURN_ON, 0x00},
	{GND_SLOW_TURN_ON, 0x00},
//DELAY START
	{DELAY_R_TO_SWITCH, 0x00},
	{DELAY_MIC_TO_SWITCH, 0x00},
	{DELAY_SENSE_TO_SWITCH, 0x00},
	{DELAY_AGND_TO_SWITCH, 0x00},
//DIO INIT START
	{DIO_INITREG1, 0x8f},
	{DIO_INITREG1, 0x5a},
	{DIO_INITREG2, 0x90},
	{DIO_INITREG3, 0x45},
};

enum SWITCH_STATUS ts_get_switch_mode(void);

extern void accdet_eint_callback_wrapper(unsigned int plug_status);
static void ts_dump_register(void);

static void ts_usbc_update_settings(struct ts_priv *ts_priv,
		u32 switch_control, u32 switch_enable)
{
	if (!ts_priv) {
		pr_err("%s: invalid ts_priv %p\n", __func__, ts_priv);
		return;
	}
	if (!ts_priv->regmap) {
		dev_err(ts_priv->dev, "%s: regmap invalid\n", __func__);
		return;
	}

	regmap_write(ts_priv->regmap, SWITCH_SETTINGS, 0x80);
	regmap_write(ts_priv->regmap, SWITCH_CONTROL, switch_control);
	/* chip hardware requirement */
	usleep_range(50, 55);
	regmap_write(ts_priv->regmap, SWITCH_SETTINGS, switch_enable);
}

//step3
static int tcpc_event_changed(struct notifier_block *nb,
				      unsigned long evt, void *ptr)
{
	struct tcp_notify *noti = ptr;
	struct ts_priv *ts_priv = container_of(nb, struct ts_priv, psy_nb);

	if (NULL == noti) {
		pr_err("%s: data is NULL. \n", __func__);
		return NOTIFY_DONE;
	}

	if (evt != TCP_NOTIFY_TYPEC_STATE) {
		pr_info("%s: event is not typec state . \n", __func__);
		return NOTIFY_DONE;
	}
	pr_info("%s: event is typec state . \n", __func__);

	ts_priv->audio_plug_status = 0;

	if (noti->typec_state.old_state == TYPEC_UNATTACHED &&
		noti->typec_state.new_state == TYPEC_ATTACHED_AUDIO) {
		/* Audio Plug in */
		pr_info("%s: Audio Plug In \n", __func__);
		ts_priv->audio_plug_status = 1;
		switch (ts_priv->vendor_type) {
			case SWITCH_DIO4485:
				//reset
				regmap_write(ts_priv->regmap, SWITCH_RESET, 0x01);
				mdelay(5);
				//slow turn on start
				regmap_write(ts_priv->regmap, L_SLOW_TURN_ON, 0xaf);
				regmap_write(ts_priv->regmap, R_SLOW_TURN_ON, 0xaf);
				regmap_write(ts_priv->regmap, MIC_SLOW_TURN_ON, 0xaf);
				regmap_write(ts_priv->regmap, SENSE_SLOW_TURN_ON, 0xaf);
				regmap_write(ts_priv->regmap, GND_SLOW_TURN_ON, 0xaf);
				//delay config start
				regmap_write(ts_priv->regmap, DELAY_SENSE_TO_SWITCH, 0x00);
				regmap_write(ts_priv->regmap, DELAY_AGND_TO_SWITCH, 0x00);
				regmap_write(ts_priv->regmap, DELAY_MIC_TO_SWITCH, 0x1f);
				regmap_write(ts_priv->regmap, DELAY_R_TO_SWITCH, 0x2f);
				regmap_write(ts_priv->regmap, DELAY_L_TO_SWITCH, 0x2f);
				//function enable
				regmap_write(ts_priv->regmap, SWITCH_SETTINGS, 0x9F);
				regmap_write(ts_priv->regmap, SWITCH_CONTROL, 0x00);
				regmap_write(ts_priv->regmap, FUNCTION_ENABLE, 0x49);
				//switch enable
				mod_timer(&enable_timer, jiffies + (int)(0.5 * HZ));
				pr_info("%s: dio4485 delay 500ms \n", __func__);
				break;
			case SWITCH_DIO4480:
				//reset
				regmap_write(ts_priv->regmap, SWITCH_RESET, 0x01);
				mdelay(1);
				//function enable
				regmap_write(ts_priv->regmap, FUNCTION_ENABLE, 0x49);
				//switch enable
				mod_timer(&enable_timer, jiffies + (int)(1 * HZ));
				pr_info("%s: dio4480 delay 1000ms \n", __func__);
				break;
			default://HL5281M et7480
				//function enable
				regmap_write(ts_priv->regmap, SWITCH_SETTINGS, 0x9F);
				regmap_write(ts_priv->regmap, SWITCH_CONTROL, 0x00);
				regmap_write(ts_priv->regmap, FUNCTION_ENABLE, 0x49);
				//switch enable
				mod_timer(&enable_timer, jiffies + (int)(0.05 * HZ));
				pr_info("%s: default delay 50ms \n", __func__);
				break;
		}
	} else if (noti->typec_state.old_state == TYPEC_ATTACHED_AUDIO &&
		noti->typec_state.new_state == TYPEC_UNATTACHED) {
		/* Audio Plug out */
		pr_info("%s: Audio Plug Out \n", __func__);
		switch (ts_priv->vendor_type) {
			case SWITCH_DIO4485:
				ts_usbc_update_settings(ts_priv, 0x18, 0x98);  // switch to usb
				break;
			case SWITCH_DIO4480:
				regmap_write(ts_priv->regmap, SWITCH_RESET, 0x01);
				mdelay(1);
				regmap_write(ts_priv->regmap, SWITCH_CONTROL, 0x18);
				regmap_write(ts_priv->regmap, SWITCH_SETTINGS, 0x98);
				break;
			default://HL5281M et7480
				regmap_write(ts_priv->regmap, FUNCTION_ENABLE, 0x48);
				ts_usbc_update_settings(ts_priv, 0x18, 0x98);  // switch to usb
				break;
		}
		accdet_eint_callback_wrapper(0);
	} else if (noti->typec_state.old_state == TYPEC_UNATTACHED &&
			noti->typec_state.new_state != TYPEC_ATTACHED_AUDIO) {
			/*Other usb device Plug in*/
			if (ts_get_switch_mode() != SWITCH_STATUS_USB_MODE) {
				ts_usbc_update_settings(ts_priv, 0x18, 0x98);
			}
	}
	return NOTIFY_OK;
}

static void ts_update_reg_defaults(struct regmap *regmap)
{
	u8 i;
	for (i = 0; i < ARRAY_SIZE(ts_reg_i2c_defaults); i++)
		regmap_write(regmap, ts_reg_i2c_defaults[i].reg,
					ts_reg_i2c_defaults[i].val);
}

/* get typec switch status  node */
enum SWITCH_STATUS ts_get_switch_mode(void)
{
	uint val = 0;
	enum SWITCH_STATUS state = SWITCH_STATUS_INVALID;
	regmap_read(global_data->regmap, SWITCH_STATUS0, &val);

	switch (val & 0xf) {
	case 0x0:
		state = SWITCH_STATUS_NOT_CONNECTED;
		break;
	case 0x5:
		state = SWITCH_STATUS_USB_MODE;
		break;
	case 0xA:
		state = SWITCH_STATUS_HEADSET_MODE;
		break;
	default:
		state = SWITCH_STATUS_INVALID;
		break;
	}

	return state;
}

int ts_switch_mode(enum SWITCH_STATUS val)
{
	if (val == SWITCH_STATUS_HEADSET_MODE) {
		ts_usbc_update_settings(global_data, 0x00, 0x9F); // switch to headset
	} else if (val == SWITCH_STATUS_USB_MODE) {
		ts_usbc_update_settings(global_data, 0x18, 0x98); // switch to USB
	}
	return 0;
}

static ssize_t sysfs_show(struct device *dev,
			      struct device_attribute *attr,
			      char *buf, u32 type)
{
	int value = 0;
	char *mode = "Unknown mode";
	int i = 0;
	ssize_t ret_size = 0;

	switch (type) {
	case DBG_TYPE_MODE:
		value = ts_get_switch_mode();
		mode = switch_status_string[value];
		ret_size += sprintf(buf, "%s: %d \n", mode, value);
		break;

	case DBG_REG_MODE:
		for (i = 0; i <= TYPEC_SWITCH_REG_MAX_RANGE; i++) {
			regmap_read(global_data->regmap, i, &value);
			ret_size += sprintf(buf + ret_size, "Reg: 0x%x, Value: 0x%x \n", i, value);
		}
	    break;
	default:
		pr_warn("%s: invalid type %d\n", __func__, type);
		break;
	}
	return ret_size;
}

static ssize_t sysfs_set(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t count, u32 type)
{
	int err;
	unsigned long value;

	err = kstrtoul(buf, 10, &value);
	if (err) {
		pr_err("%s: get data of type %d failed\n", __func__, type);
		return err;
	}
	pr_info("%s: set type %d, data %ld\n", __func__, type, value);
	switch (type) {
		case DBG_TYPE_MODE:
			ts_switch_mode((enum SWITCH_STATUS)value);
			break;
		default:
			pr_warn("%s: invalid type %d\n", __func__, type);
			break;
	}
	return count;
}

#define TS_DEVICE_SHOW(_name, _type) static ssize_t \
show_##_name(struct device *dev, \
			  struct device_attribute *attr, char *buf) \
{ \
	return sysfs_show(dev, attr, buf, _type); \
}

#define TS_DEVICE_SET(_name, _type) static ssize_t \
set_##_name(struct device *dev, \
			 struct device_attribute *attr, \
			 const char *buf, size_t count) \
{ \
	return sysfs_set(dev, attr, buf, count, _type); \
}

#define TYPEC_SWITCH_DEVICE_SHOW_SET(name, type) \
TS_DEVICE_SHOW(name, type) \
TS_DEVICE_SET(name, type) \
static DEVICE_ATTR(name, S_IWUSR | S_IRUGO, show_##name, set_##name);

TYPEC_SWITCH_DEVICE_SHOW_SET(ts_switch_mode, DBG_TYPE_MODE);
TYPEC_SWITCH_DEVICE_SHOW_SET(ts_switch_reg, DBG_REG_MODE);

static struct attribute *typec_switch_attrs[] = {
	&dev_attr_ts_switch_mode.attr,
	&dev_attr_ts_switch_reg.attr,
	NULL
};

static const struct attribute_group ts_group = {
	.attrs = typec_switch_attrs,
};
/* end of info node */

static int ts_gpio_pinctrl_init(struct ts_priv *ts_priv)
{
	int retval = 0;

	pr_info("%s \n", __func__);

	ts_priv->uart_en_gpio_pinctrl = devm_pinctrl_get(ts_priv->dev);
	if (IS_ERR_OR_NULL(ts_priv->uart_en_gpio_pinctrl)) {
		retval = PTR_ERR(ts_priv->uart_en_gpio_pinctrl);
		goto gpio_err_handle;
	}

	ts_priv->pinctrl_state_disable
		= pinctrl_lookup_state(ts_priv->uart_en_gpio_pinctrl, "uart_disable");
	if (IS_ERR_OR_NULL(ts_priv->pinctrl_state_disable)) {
		retval = PTR_ERR(ts_priv->pinctrl_state_disable);
		goto gpio_err_handle;
	}

	ts_priv->pinctrl_state_enable
		= pinctrl_lookup_state(ts_priv->uart_en_gpio_pinctrl, "uart_enable");
	if (IS_ERR_OR_NULL(ts_priv->pinctrl_state_enable)) {
		retval = PTR_ERR(ts_priv->pinctrl_state_enable);
		goto gpio_err_handle;
	}

	return 0;

gpio_err_handle:
	pr_info("%s: init failed \n", __func__);
	ts_priv->uart_en_gpio_pinctrl = NULL;

	return retval;
}

static void ts_enable_handler(struct timer_list *t)
{
	int ret = 0;

	ret = queue_work(enable_switch_workqueue, &enable_typec_switch_work);
	if (!ret)
		pr_info("%s, queue work return: %d!\n", __func__, ret);
}

//step2 or step4
static void ts_enable_callback(struct work_struct *work)
{
	u32 int_status = 0;
	u32 jack_status = 0;
	u32 switch_control = 0;
	u32 switch_setting = 0;
	pr_info("%s()\n", __func__);
	regmap_read(global_data->regmap, SWITCH_DETECTION, &int_status);
	regmap_read(global_data->regmap, SWITCH_JACK_STATUS, &jack_status);
	pr_info( "%s system flag reg (0x18) =0x%x , jack status reg (0x17) =0x%x\n", __func__, int_status, jack_status);
	switch (global_data->vendor_type) {
		case SWITCH_DIO4480:
			if (jack_status == 0x01) {
				pr_info("%s() write reg\n", __func__);
				regmap_write(global_data->regmap, SWITCH_RESET, 0x01);
				mdelay(1);
				regmap_write(global_data->regmap, SWITCH_CONTROL, 0x00);
				regmap_write(global_data->regmap, SWITCH_SETTINGS, 0x9f);
				mdelay(20);
			}
			break;
		case SWITCH_DIO4485:
			if (jack_status == 0x01) {
				pr_info("%s() dio4485 write reg\n", __func__);
				//reset
				regmap_write(global_data->regmap, SWITCH_RESET, 0x01);
				mdelay(5);
				//init
				regmap_write(global_data->regmap, DIO_INITREG1, 0x8f);
				regmap_write(global_data->regmap, DIO_INITREG1, 0x5a);
				regmap_write(global_data->regmap, DIO_INITREG2, 0x90);
				regmap_write(global_data->regmap, DIO_INITREG3, 0x45);
				//delay config start
				regmap_write(global_data->regmap, DELAY_SENSE_TO_SWITCH, 0x00);
				regmap_write(global_data->regmap, DELAY_AGND_TO_SWITCH, 0x00);
				regmap_write(global_data->regmap, DELAY_MIC_TO_SWITCH, 0x1f);
				regmap_write(global_data->regmap, DELAY_R_TO_SWITCH, 0x2f);
				regmap_write(global_data->regmap, DELAY_L_TO_SWITCH, 0x2f);

				regmap_write(global_data->regmap, SWITCH_SETTINGS, 0x9f);
				regmap_write(global_data->regmap, SWITCH_CONTROL, 0x00);
				mdelay(20);
				regmap_write(global_data->regmap, FUNCTION_ENABLE, 0x09);
				mdelay(1000);
			} else if ((int_status & (1 << 2) ) && (jack_status & (1 << 1))) {
				pr_info("%s: 3-pole detect\n", __func__);
				regmap_read(global_data->regmap, SWITCH_SETTINGS, &switch_setting);
				regmap_read(global_data->regmap, SWITCH_CONTROL, &switch_control);

				/* Set Bit 1 to 0, Enable Mic <---> SBU2 */
				regmap_write(global_data->regmap, SWITCH_CONTROL, switch_control & (~(1 << 1)));
				/* chip hardware requirement */
				usleep_range(50, 55);
				/* Enable Bit 1, Enable Mic <---> SBU2 Switch */
				regmap_write(global_data->regmap, SWITCH_SETTINGS, switch_setting | (1 << 1));
			}
			break;
		default:
			if (jack_status == 0x01) {
				pr_info("%s() others write reg\n", __func__);
				regmap_write(global_data->regmap, SWITCH_RESET, 0x01);
				mdelay(1);
				regmap_write(global_data->regmap, SWITCH_SETTINGS, 0x9f);
				regmap_write(global_data->regmap, SWITCH_CONTROL, 0x00);
				mdelay(20);
				regmap_write(global_data->regmap, FUNCTION_ENABLE, 0x09);
				mdelay(1000);
			} else if ((int_status & (1 << 2) ) && (jack_status & (1 << 1))) {
				pr_info("%s: 3-pole detect\n", __func__);
				regmap_read(global_data->regmap, SWITCH_SETTINGS, &switch_setting);
				regmap_read(global_data->regmap, SWITCH_CONTROL, &switch_control);

				/* Set Bit 1 to 0, Enable Mic <---> SBU2 */
				regmap_write(global_data->regmap, SWITCH_CONTROL, switch_control & (~(1 << 1)));
				/* chip hardware requirement */
				usleep_range(50, 55);
				/* Enable Bit 1, Enable Mic <---> SBU2 Switch */
				regmap_write(global_data->regmap, SWITCH_SETTINGS, switch_setting | (1 << 1));
			}
			break;
	}

	accdet_eint_callback_wrapper(1);
	ts_dump_register();
}

static void ts_delay_init_handler(struct timer_list *t)
{
	int ret = 0;

	ret = queue_work(delay_init_workqueue, &delay_init_work);
	pr_info("%s \n", __func__);
	if (!ret)
		pr_info("%s, queue work return: %d!\n", __func__, ret);
}

//step1
static void ts_delay_init_callback(struct work_struct *work)
{
	int rc = 0;
	pr_info("%s() \n", __func__);

	if (global_data && global_data->tcpc_dev) {
		/* check tcpc status at startup */
		if (TYPEC_ATTACHED_AUDIO == tcpm_inquire_typec_attach_state(global_data->tcpc_dev)) {
			/* Audio Plug in */
			pr_info("%s: Audio is Plug In status at startup\n", __func__);
                        global_data->audio_plug_status = 1;
			if(global_data->vendor_type == SWITCH_DIO4480) {
			regmap_write(global_data->regmap, SWITCH_RESET, 0x01);
			mdelay(1);
			regmap_write(global_data->regmap, FUNCTION_ENABLE, 0x49);
			//step2
			mod_timer(&enable_timer, jiffies + (int)(1* HZ));//1000ms
			} else {
			regmap_write(global_data->regmap, SWITCH_SETTINGS, 0x9F);
			regmap_write(global_data->regmap, SWITCH_CONTROL, 0x00);
			regmap_write(global_data->regmap, FUNCTION_ENABLE, 0x49);
			//step2
			mod_timer(&enable_timer, jiffies + (int)(0.2 * HZ));//200ms
			}
			accdet_eint_callback_wrapper(1);
		}

		/* register tcpc_event */
		global_data->psy_nb.notifier_call = tcpc_event_changed;//step3
		global_data->psy_nb.priority = 0;
		rc = register_tcp_dev_notifier(global_data->tcpc_dev, &global_data->psy_nb, TCP_NOTIFY_TYPE_USB);
		if (rc) {
			pr_err("%s: register_tcp_dev_notifier failed\n", __func__);
		}
	}
}

#if IS_ENABLED(CONFIG_RUST_DETECTION)
int ts_moisture_detection_enable(struct charger_device *chgdev, int en)
{
	if (!chgdev) {
		pr_err( "%s chgdev is NULL\n", __func__);
		return MOSITRUE_DETECT_ERROR;
	}

	struct ts_priv *ts_priv = charger_get_data(chgdev);
	if (!ts_priv) {
		pr_err( "%s ts_priv is NULL\n", __func__);
		return MOSITRUE_DETECT_ERROR;
	}

	if (ts_priv->audio_plug_status) {
		pr_info("%s dont need to enable res\n", __func__);
		return MOSITRUE_NOT_DETECTED;
	}

	if (en) {
		pr_info("%s enable res\n", __func__);
		u32 flag_status = 0;
		//0x13 bit3=1 ,Voltage source for moisture detection
		regmap_write(ts_priv->regmap, RES_DET_CONTROL1, 0x08);
		//0x16 bit2=1 ,multi pin mode
		regmap_write(ts_priv->regmap, RES_DET_FUNCTION1, 0x04);
		//0x31 10011 ,cc sub1 sub2
		regmap_write(ts_priv->regmap, RES_DET_CONTROL2, 0x13);
		//0x32 FF, max value
		regmap_write(ts_priv->regmap, RES_DET_THRESHOLD, 0xFF);
		//0x18 read to remove flag
		regmap_read(ts_priv->regmap, SWITCH_DETECTION, &flag_status);
		//0x12 bit1 enable res
		regmap_update_bits(ts_priv->regmap, FUNCTION_ENABLE, 0x02, 0x02);

		regmap_read(ts_priv->regmap, FUNCTION_ENABLE, &flag_status);
		pr_info( "%s function reg (0x12) =0x%x\n", __func__, flag_status);
	} else {
		pr_info("%s disable res\n", __func__);
		//0x12 bit1 disable res
		regmap_update_bits(ts_priv->regmap, FUNCTION_ENABLE, 0x02, 0x00);
	}
	return MOSITRUE_NOT_DETECTED;
}

int ts_moisture_detection_read_res(struct charger_device *chgdev)
{
	if (!chgdev) {
		pr_err( "%s chgdev is NULL\n", __func__);
		return MOSITRUE_DETECT_ERROR;
	}

	struct ts_priv *ts_priv = charger_get_data(chgdev);
	if (!ts_priv) {
		pr_err( "%s ts_priv is NULL\n", __func__);
		return MOSITRUE_DETECT_ERROR;
	}

	if (ts_priv->audio_plug_status) {
		pr_info("%s dont need to read res\n", __func__);
		return MOSITRUE_NOT_DETECTED;
	}

	u32 flag_status = 0;
	u8 cnt = 10;
	u32 cc_val = 0;
	u32 sub1_val = 0;
	u32 sub2_val = 0;

	while(cnt>0)
	{
		pr_info( "%s cnt =%d\n", __func__, cnt);
		cnt--;
		mdelay(5);
		regmap_read(ts_priv->regmap, SWITCH_DETECTION, &flag_status);
		if((flag_status&0x01) == 0x01) {
			pr_info( "%s flag detected \n", __func__);
            break;
		}
	}
	regmap_read(ts_priv->regmap, RES_DET_DATA5, &cc_val);
	regmap_read(ts_priv->regmap, RES_DET_DATA8, &sub1_val);
	regmap_read(ts_priv->regmap, RES_DET_DATA9, &sub2_val);

	pr_info("%s: cc_val=0x%x, sub1_val=0x%x, sub2_val=0x%x\n", __func__ , cc_val, sub1_val, sub2_val);

	switch (ts_priv->vendor_type) {
		case SWITCH_HL5281M:
			cc_val = cc_val ? ((cc_val + 1) * HL5281M_PARAM) : cc_val;
			sub1_val = sub1_val ? ((sub1_val + 1) * HL5281M_PARAM) : sub1_val;
			sub2_val = sub2_val? ((sub2_val + 1) * HL5281M_PARAM) : sub2_val;
			pr_info("%s: HL5281M cc_val=%duV, sub1_val=%duV, sub2_val=%duV\n", __func__ , cc_val, sub1_val, sub2_val);
			break;
		case SWITCH_DIO4485:
			cc_val = cc_val * DIO4485_PARAM;
			sub1_val = sub1_val * DIO4485_PARAM;
			sub2_val = sub2_val * DIO4485_PARAM;
			pr_info("%s: DIO4485 cc_val=%duV, sub1_val=%duV, sub2_val=%duV\n", __func__ , cc_val, sub1_val, sub2_val);
			break;
		default:
			pr_info("%s() NOT SUPPORT\n", __func__);
			break;
	}

	if (cc_val > THRESHOLD_CC && (sub1_val > THRESHOLD_SUB || sub2_val > THRESHOLD_SUB)) {
		pr_info("%s MOSITRUE_DETECTED\n", __func__);
		return MOSITRUE_DETECTED;
	}

	pr_info("%s MOSITRUE_NOT_DETECTED\n", __func__);

	return MOSITRUE_NOT_DETECTED;
}

static const struct charger_ops ts_chg_ops = {
	.rust_detection_enable = ts_moisture_detection_enable,
	.rust_detection_read_res = ts_moisture_detection_read_res,
};

static const struct charger_properties ts_chg_props = {
	.alias_name = "typec_switch_chg",
};

static int ts_chg_init_chgdev(struct ts_priv *ddata)
{
	pr_info("%s\n", __func__);
	ddata->chgdev = charger_device_register("typec_switch_chg", ddata->dev,
						ddata, &ts_chg_ops,
						&ts_chg_props);
	return IS_ERR(ddata->chgdev) ? PTR_ERR(ddata->chgdev) : 0;
}

#endif

static void ts_dump_register(void)
{
	int adr = 0, value = 0;
	pr_info("%s:dump reg begin\n",__func__);
	for (adr = 0; adr <= TYPEC_SWITCH_REG_MAX_RANGE; adr++) {
		regmap_read(global_data->regmap, adr, &value);
		pr_info("(0x%x)=0x%x",adr,value);
	}
}

/*
	step1:switch ic reset control
	step2:start enable_timer(ts_enable_callback) to enable switch
	step3:register tcpc_event(tcpc_event_changed) receive typec event
	step4:if plug in audio device then step2 again.
*/
static int ts_probe(struct i2c_client *i2c,
			 const struct i2c_device_id *id)
{
	struct ts_priv *ts_priv;
	int rc = 0;
	int reg_val = 0;

	ts_priv = devm_kzalloc(&i2c->dev, sizeof(*ts_priv),
				GFP_KERNEL);
	if (!ts_priv)
		return -ENOMEM;

	global_data = ts_priv; // add for debug
	ts_priv->dev = &i2c->dev;
	//get tcpc device
	ts_priv->tcpc_dev = tcpc_dev_get_by_name("type_c_port0");
	if (!ts_priv->tcpc_dev) {
		rc = -EPROBE_DEFER;
		pr_err("%s get tcpc device type_c_port0 fail \n", __func__);
		goto err_data;
	}

	ts_priv->regmap = devm_regmap_init_i2c(i2c, &switch_regmap_config);
	if (IS_ERR_OR_NULL(ts_priv->regmap)) {
		dev_err(ts_priv->dev, "%s: Failed to initialize regmap: %d\n",
			__func__, rc);
		if (!ts_priv->regmap) {
			rc = -EINVAL;
			goto err_data;
		}
		rc = PTR_ERR(ts_priv->regmap);
		goto err_data;
	}

	//enable uart
	ts_gpio_pinctrl_init(ts_priv);
	if (ts_priv->uart_en_gpio_pinctrl) {
		rc = pinctrl_select_state(ts_priv->uart_en_gpio_pinctrl,
					ts_priv->pinctrl_state_enable);
		if (rc < 0) {
			pr_err("Failed to select enable pinstate %d\n", rc);
		}
	}

	//reg map init
	ts_update_reg_defaults(ts_priv->regmap);
	//private data point set in client
	i2c_set_clientdata(i2c, ts_priv);

	ts_priv->ts_notifier.rwsem =
		(struct rw_semaphore)__RWSEM_INITIALIZER
		((ts_priv->ts_notifier).rwsem);
	ts_priv->ts_notifier.head = NULL;

	//create sys node
	rc = sysfs_create_group(&i2c->dev.kobj, &ts_group);
	if (rc) {
		pr_err("%s: create attr error %d\n", __func__, rc);
	}

	//WORK1:create AND setup switch enable workqueue wait mod_timer 
	enable_switch_workqueue = create_singlethread_workqueue("enableSwitchQueue");
	INIT_WORK(&enable_typec_switch_work, ts_enable_callback);//ts_enable_callback:step2&step4
	if (!enable_switch_workqueue) {
		rc = -1;
		pr_notice("%s create enable_switch workqueue fail.\n", __func__);
		goto err_data;
	}
	pr_info("%s(), setup enable timer", __func__);
	timer_setup(&enable_timer, ts_enable_handler, 0);

	//WORK2:create and setup delay init workqueue
	delay_init_workqueue = create_singlethread_workqueue("delayInitQueue");
	INIT_WORK(&delay_init_work, ts_delay_init_callback);//ts_delay_init_callback:step1
	/* delay 10s to register tcpc event change, after accdet init done */
	timer_setup(&delay_init_timer, ts_delay_init_handler, 0);
	mod_timer(&delay_init_timer, jiffies + DELAY_INIT_TIME);

	// init vendor flags
	ts_priv->vendor_type = SWITCH_NOT_SUPPORT;

	regmap_read(ts_priv->regmap, TYPEC_SWITCH_VENDOR_ID, &reg_val);
	ts_priv->vendor_type = reg_val;
    pr_info("%s audio switch use vendor reg_val is %x", __func__, reg_val);

	if (ts_priv->vendor_type == SWITCH_DIO4485) {
		pr_info("%s audio switch use dio4485 vendor_type is %x", __func__, ts_priv->vendor_type);
	} else if (ts_priv->vendor_type == SWITCH_DIO4480){
		pr_info("%s audio switch use dio4480 vendor_type is %x", __func__, ts_priv->vendor_type);
	} else if (ts_priv->vendor_type == SWITCH_ET7480){
		pr_info("%s audio switch use et7480 vendor_type is %x", __func__, ts_priv->vendor_type);
	} else if (ts_priv->vendor_type == SWITCH_HL5281M){
		pr_info("%s audio switch use hl5281m vendor_type is %x", __func__, ts_priv->vendor_type);
	} else {
		pr_info("%s audio switch unknown vendor_type is %x", __func__, ts_priv->vendor_type);
	}

#if IS_ENABLED(CONFIG_RUST_DETECTION)
	rc = ts_chg_init_chgdev(ts_priv);
	if (rc < 0) {
		pr_err( "%s :failed to init chgdev\n", __func__);
	}
#endif
	return 0;
err_data:
	devm_kfree(&i2c->dev, ts_priv);
	return rc;
}

static void ts_remove(struct i2c_client *i2c)
{
	struct ts_priv *ts_priv =
			(struct ts_priv *)i2c_get_clientdata(i2c);

	if (!ts_priv) {
		pr_err( "ts_priv is NULL\n");
		return;
	}

	ts_usbc_update_settings(ts_priv, 0x18, 0x98);

	dev_set_drvdata(&i2c->dev, NULL);
}

static void ts_shutdown(struct i2c_client *client)
{
	struct ts_priv *ts_priv =
			(struct ts_priv *)i2c_get_clientdata(client);

	if (!ts_priv) {
		pr_err( "ts_priv is NULL\n");
		return;
	}
	pr_info("%s poweroff reset switch.", __func__);
	regmap_write(ts_priv->regmap, SWITCH_RESET, 0x01);
}

static const struct of_device_id switch_i2c_dt_match[] = {
	{
		.compatible = "mediatek,typec-audioswitch",
	},
	{}
};

static struct i2c_driver switch_i2c_driver = {
	.driver = {
		.name = TYPEC_SWITCH_I2C_NAME,
		.of_match_table = switch_i2c_dt_match,
	},
	.probe = ts_probe,
	.remove = ts_remove,
	.shutdown = ts_shutdown,
};

static int __init switch_init(void)
{
	int rc;
	rc = i2c_add_driver(&switch_i2c_driver);
	if (rc)
		pr_err("TS_INIT: Failed to register I2C driver: %d\n", rc);

	return rc;
}
late_initcall(switch_init);

static void __exit switch_exit(void)
{
	i2c_del_driver(&switch_i2c_driver);
}
module_exit(switch_exit);

MODULE_DESCRIPTION("TYPEC SWITCH I2C driver");
MODULE_LICENSE("GPL v2");
