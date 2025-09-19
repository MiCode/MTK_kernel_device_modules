// SPDX-License-Identifier: GPL-2.0
/*
 * Awinic high voltage LRA haptic driver
 *
 * Copyright (c) 2021-2023 awinic. All Rights Reserved.
 *
 * Author: Ethan <renzhiqiang@awinic.com>
 */

#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/firmware.h>
#include <linux/proc_fs.h>
#include <linux/page_size_compat.h>
#include "haptic_hv.h"
#include "haptic_hv_reg.h"
#include <linux/miscdevice.h>
#include "ringbuffer.h"
#include "xm-haptic.h"

#define HAPTIC_HV_DRIVER_VERSION	"v1.9.0.9"

char *aw_ram_name = "aw8697_haptic.bin";

#ifdef AAC_RICHTAP_SUPPORT
struct aw_haptic *g_aw_haptic = NULL;
#endif
#ifdef AW_TIKTAP
static struct aw_haptic *g_aw_haptic;
#endif
#ifdef AW_DOUBLE
struct aw_haptic *left;
struct aw_haptic *right;
#endif
#ifdef TEST_RTP
char aw_rtp_name[][AW_RTP_NAME_MAX] = {
	{"aw86927_rtp_1.bin"},
};
#else
char aw_rtp_name[][AW_RTP_NAME_MAX] = {
	{"aw8697_rtp_1.bin"}, /*8*/
	{"aw8697_rtp_1.bin"},
	{"aw8697_rtp_1.bin"},
	{"aw8697_rtp_1.bin"},
	{"aw8697_rtp_1.bin"},
	{"aw8697_rtp_1.bin"},
	{"aw8697_rtp_1.bin"},
	{"aw8697_rtp_1.bin"},
	{"aw8697_rtp_1.bin"},
	{"aw8697_rtp_1.bin"},
	{"aw8697_rtp_1.bin"},
	{"AcousticGuitar_RTP.bin"}, /*21*/
	{"Blues_RTP.bin"},
	{"Candy_RTP.bin"},
	{"Carousel_RTP.bin"},
	{"Celesta_RTP.bin"},
	{"Childhood_RTP.bin"},
	{"Country_RTP.bin"},
	{"Cowboy_RTP.bin"},
	{"Echo_RTP.bin"},
	{"Fairyland_RTP.bin"},
	{"Fantasy_RTP.bin"},
	{"Field_Trip_RTP.bin"},
	{"Glee_RTP.bin"},
	{"Glockenspiel_RTP.bin"},
	{"Ice_Latte_RTP.bin"},
	{"Kung_Fu_RTP.bin"},
	{"Leisure_RTP.bin"},
	{"Lollipop_RTP.bin"},
	{"MiMix2_RTP.bin"},
	{"Mi_RTP.bin"},
	{"MiHouse_RTP.bin"},
	{"MiJazz_RTP.bin"},
	{"MiRemix_RTP.bin"},
	{"Mountain_Spring_RTP.bin"},
	{"Orange_RTP.bin"},
	{"WindChime_RTP.bin"},
	{"Space_Age_RTP.bin"},
	{"ToyRobot_RTP.bin"},
	{"Vigor_RTP.bin"},
	{"Bottle_RTP.bin"},
	{"Bubble_RTP.bin"},
	{"Bullfrog_RTP.bin"},
	{"Burst_RTP.bin"},
	{"Chirp_RTP.bin"},
	{"Clank_RTP.bin"},
	{"Crystal_RTP.bin"},
	{"FadeIn_RTP.bin"},
	{"FadeOut_RTP.bin"},
	{"Flute_RTP.bin"},
	{"Fresh_RTP.bin"},
	{"Frog_RTP.bin"},
	{"Guitar_RTP.bin"},
	{"Harp_RTP.bin"},
	{"IncomingMessage_RTP.bin"},
	{"MessageSent_RTP.bin"},
	{"Moment_RTP.bin"},
	{"NotificationXylophone_RTP.bin"},
	{"Potion_RTP.bin"},
	{"Radar_RTP.bin"},
	{"Spring_RTP.bin"},
	{"Swoosh_RTP.bin"}, /*71*/
	{"Gesture_UpSlide_RTP.bin"},
	{"FOD_Motion_Planet_RTP.bin"},
	{"Charge_Wire_RTP.bin"},
	{"Charge_Wireless_RTP.bin"},
	{"Unlock_Failed_RTP.bin"},
	{"FOD_Motion1_RTP.bin"},
	{"FOD_Motion2_RTP.bin"},
	{"FOD_Motion3_RTP.bin"},
	{"FOD_Motion4_RTP.bin"},
	{"FOD_Motion_Aurora_RTP.bin"},
	{"FaceID_Wrong2_RTP.bin"}, /*82*/
	{"uninstall_animation_rtp.bin"},
	{"uninstall_dialog_rtp.bin"},
	{"screenshot_rtp.bin"},
	{"lockscreen_camera_entry_rtp.bin"},
	{"launcher_edit_rtp.bin"},
	{"launcher_icon_selection_rtp.bin"},
	{"taskcard_remove_rtp.bin"},
	{"task_cleanall_rtp.bin"},
	{"new_iconfolder_rtp.bin"},
	{"notification_remove_rtp.bin"},
	{"notification_cleanall_rtp.bin"},
	{"notification_setting_rtp.bin"},
	{"game_turbo_rtp.bin"},
	{"NFC_card_rtp.bin"},
	{"wakeup_voice_assistant_rtp.bin"},
	{"NFC_card_slow_rtp.bin"},
	{"aw8697_rtp_1.bin"}, /*99*/
	{"aw8697_rtp_1.bin"}, /*100*/
	{"offline_countdown_RTP.bin"},
	{"scene_bomb_injury_RTP.bin"},
	{"scene_bomb_RTP.bin"}, /*103*/
	{"door_open_RTP.bin"},
	{"aw8697_rtp_1.bin"},
	{"scene_step_RTP.bin"}, /*106*/
	{"crawl_RTP.bin"},
	{"scope_on_RTP.bin"},
	{"scope_off_RTP.bin"},
	{"magazine_quick_RTP.bin"},
	{"grenade_RTP.bin"},
	{"scene_getshot_RTP.bin"}, /*112*/
	{"grenade_explosion_RTP.bin"},
	{"punch_RTP.bin"},
	{"pan_RTP.bin"},
	{"bandage_RTP.bin"},
	{"aw8697_rtp_1.bin"},
	{"scene_jump_RTP.bin"},
	{"vehicle_plane_RTP.bin"}, /*119*/
	{"scene_openparachute_RTP.bin"}, /*120*/
	{"scene_closeparachute_RTP.bin"}, /*121*/
	{"vehicle_collision_RTP.bin"},
	{"vehicle_buggy_RTP.bin"}, /*123*/
	{"vehicle_dacia_RTP.bin"}, /*124*/
	{"vehicle_moto_RTP.bin"}, /*125*/
	{"firearms_akm_RTP.bin"}, /*126*/
	{"firearms_m16a4_RTP.bin"}, /*127*/
	{"aw8697_rtp_1.bin"},
	{"firearms_awm_RTP.bin"}, /*129*/
	{"firearms_mini14_RTP.bin"}, /*130*/
	{"firearms_vss_RTP.bin"}, /*131*/
	{"firearms_qbz_RTP.bin"}, /*132*/
	{"firearms_ump9_RTP.bin"}, /*133*/
	{"firearms_dp28_RTP.bin"}, /*134*/
	{"firearms_s1897_RTP.bin"}, /*135*/
	{"aw8697_rtp_1.bin"},
	{"firearms_p18c_RTP.bin"}, /*137*/
	{"aw8697_rtp_1.bin"},
	{"aw8697_rtp_1.bin"},
	{"CFM_KillOne_RTP.bin"},
	{"CFM_Headshot_RTP.bin"}, /*141*/
	{"CFM_MultiKill_RTP.bin"},
	{"CFM_KillOne_Strong_RTP.bin"},
	{"CFM_Headshot_Strong_RTP.bin"},
	{"CFM_MultiKill_Strong_RTP.bin"},
	{"CFM_Weapon_Grenade_Explode_RTP.bin"},
	{"CFM_Weapon_Grenade_KillOne_RTP.bin"},
	{"CFM_ImpactFlesh_Normal_RTP.bin"},
	{"CFM_Weapon_C4_Installed_RTP.bin"},
	{"CFM_Hero_Appear_RTP.bin"},
	{"CFM_UI_Reward_OpenBox_RTP.bin"},
	{"CFM_UI_Reward_Task_RTP.bin"},
	{"CFM_Weapon_BLT_Shoot_RTP.bin"}, /*153*/
	{"Atlantis_RTP.bin"},
	{"DigitalUniverse_RTP.bin"},
	{"Reveries_RTP.bin"},
	{"FOD_Motion_Triang_RTP.bin"},
	{"FOD_Motion_Flare_RTP.bin"},
	{"FOD_Motion_Ripple_RTP.bin"},
	{"FOD_Motion_Spiral_RTP.bin"},
	{"gamebox_launch_rtp.bin"}, /*161*/
	{"Gesture_Back_Pull_RTP.bin"}, /*162*/
	{"Gesture_Back_Release_RTP.bin"}, /*163*/
	{"alert_rtp.bin"}, /*164*/
	{"feedback_negative_light_rtp.bin"}, /*165*/
	{"feedback_neutral_rtp.bin"}, /*166*/
	{"feedback_positive_rtp.bin"}, /*167*/
	{"fingerprint_record_rtp.bin"}, /*168*/
	{"lockdown_rtp.bin"}, /*169*/
	{"sliding_damping_rtp.bin"}, /*170*/
	{"todo_alldone_rtp.bin"}, /*171*/
	{"uninstall_animation_icon_rtp.bin"}, /*172*/
	{"signal_button_highlight_rtp.bin"}, /*173*/
	{"signal_button_negative_rtp.bin"},
	{"signal_button_rtp.bin"},
	{"signal_clock_high_rtp.bin"}, /*176*/
	{"signal_clock_rtp.bin"},
	{"signal_clock_unit_rtp.bin"},
	{"signal_inputbox_rtp.bin"},
	{"signal_key_high_rtp.bin"},
	{"signal_key_unit_rtp.bin"}, /*181*/
	{"signal_list_highlight_rtp.bin"},
	{"signal_list_rtp.bin"},
	{"signal_picker_rtp.bin"},
	{"signal_popup_rtp.bin"},
	{"signal_seekbar_rtp.bin"}, /*186*/
	{"signal_switch_rtp.bin"},
	{"signal_tab_rtp.bin"},
	{"signal_text_rtp.bin"},
	{"signal_transition_light_rtp.bin"},
	{"signal_transition_rtp.bin"}, /*191*/
	{"haptics_video_rtp.bin"}, /*192*/
	{"keyboard_clicky_down_rtp.bin"},
	{"keyboard_clicky_up_rtp.bin"},
	{"keyboard_linear_down_rtp.bin"},
	{"keyboard_linear_up_rtp.bin"}, /*196*/
};
#endif
int awinic_rtp_name_len = sizeof(aw_rtp_name) / AW_RTP_NAME_MAX;
int CUSTOME_WAVE_ID;
#ifdef A2HAPTIC_SUPPORT
EXPORT_SYMBOL_GPL(CUSTOME_WAVE_ID);

struct aw_haptic *vir_aw_haptic = NULL;
EXPORT_SYMBOL_GPL(vir_aw_haptic);
#endif

/*********************************************************
 *
 * I2C Read/Write
 *
 *********************************************************/
int haptic_hv_i2c_reads(struct aw_haptic *aw_haptic, uint8_t reg_addr, uint8_t *buf, uint32_t len)
{
	int ret;
	bool change_addr_flag = false;

#ifdef DUAL_RTP_TEST
	if ((aw_haptic == right) && (aw_haptic->i2c->addr == AW8693X_BROADCAST_ADDR)) {
		change_addr_flag = true;
		aw_haptic->i2c->addr = right->record_i2c_addr;
	}
#endif
	struct i2c_msg msg[] = {
		[0] = {
			.addr = aw_haptic->i2c->addr,
			.flags = 0,
			.len = sizeof(uint8_t),
			.buf = &reg_addr,
			},
		[1] = {
			.addr = aw_haptic->i2c->addr,
			.flags = I2C_M_RD,
			.len = len,
			.buf = buf,
			},
	};

	ret = i2c_transfer(aw_haptic->i2c->adapter, msg, ARRAY_SIZE(msg));
#ifdef DUAL_RTP_TEST
	if (change_addr_flag)
		aw_haptic->i2c->addr = AW8693X_BROADCAST_ADDR;
#endif
	if (ret < 0) {
		aw_err("transfer failed.");
		return ret;
	} else if (ret != AW_I2C_READ_MSG_NUM) {
		aw_err("transfer failed(size error).");
		return -ENXIO;
	}

	return ret;
}

int haptic_hv_i2c_writes(struct aw_haptic *aw_haptic, uint8_t reg_addr, uint8_t *buf, uint32_t len)
{
	uint8_t *data = NULL;
	int ret = -1;

	data = kmalloc(len + 1, GFP_KERNEL);
	data[0] = reg_addr;
	memcpy(&data[1], buf, len);
	ret = i2c_master_send(aw_haptic->i2c, data, len + 1);
	if (ret < 0)
		aw_err("i2c master send 0x%02x err", reg_addr);
	kfree(data);
	return ret;
}

int haptic_hv_i2c_write_bits(struct aw_haptic *aw_haptic, uint8_t reg_addr,
			     uint32_t mask, uint8_t reg_data)
{
	uint8_t reg_val = 0;
	int ret = -1;

	ret = haptic_hv_i2c_reads(aw_haptic, reg_addr, &reg_val, AW_I2C_BYTE_ONE);
	if (ret < 0) {
		aw_err("i2c read error, ret=%d", ret);
		return ret;
	}
	reg_val &= mask;
	reg_val |= (reg_data & (~mask));
	ret = haptic_hv_i2c_writes(aw_haptic, reg_addr, &reg_val, AW_I2C_BYTE_ONE);
	if (ret < 0) {
		aw_err("i2c write error, ret=%d", ret);
		return ret;
	}

	return 0;
}

static int parse_dt_gpio(struct device *dev, struct aw_haptic *aw_haptic, struct device_node *np)
{
	int val = 0;
    aw_haptic->reset_gpio = of_get_named_gpio(np, "reset-gpio", 0);
    if (aw_haptic->reset_gpio < 0) {
        aw_err("no reset gpio provide");
        return -EPERM;
    }
    aw_info("reset gpio provide ok %d", aw_haptic->reset_gpio);
    aw_haptic->irq_gpio = of_get_named_gpio(np, "irq-gpio", 0);
    if (aw_haptic->irq_gpio < 0)
        aw_err("no irq gpio provided.");
    else
        aw_info("irq gpio provide ok irq = %d.", aw_haptic->irq_gpio);
	val = of_property_read_u8(np, "mode", &aw_haptic->info.mode);
	if (val != 0)
		aw_info("mode not found");
#ifdef AW_DOUBLE
	if (of_device_is_compatible(np, "awinic,haptic_hv_l")) {
		aw_info("compatible left vibrator.");
		memcpy(aw_haptic->name, "left", sizeof("left"));
		left = NULL;
		left = aw_haptic;
	} else if (of_device_is_compatible(np, "awinic,haptic_hv_r")) {
		aw_info("compatible right vibrator.");
		memcpy(aw_haptic->name, "right", sizeof("right"));
		right = NULL;
		right = aw_haptic;
	} else {
		aw_err("compatible failed.");
		return -ERANGE;
	}
#endif

	return 0;
}

static void hw_reset(struct aw_haptic *aw_haptic)
{
	aw_info("enter");
	if (gpio_is_valid(aw_haptic->reset_gpio)) {
		gpio_set_value_cansleep(aw_haptic->reset_gpio, 0);
		usleep_range(1000, 2000);
		gpio_set_value_cansleep(aw_haptic->reset_gpio, 1);
		usleep_range(8000, 8500);
	} else {
		aw_err("failed");
	}
}

void sw_reset(struct aw_haptic *aw_haptic)
{
	uint8_t reset = AW_BIT_RESET;

	aw_dbg("enter");
	haptic_hv_i2c_writes(aw_haptic, AW_REG_CHIPID, &reset, AW_I2C_BYTE_ONE);
	usleep_range(3000, 3500);
}

static int judge_value(uint8_t reg)
{
	int ret = 0;

	if (!reg)
		return -ERANGE;
	switch (reg) {
	case AW86925_BIT_RSTCFG_PRE_VAL:
	case AW86926_BIT_RSTCFG_PRE_VAL:
	case AW86927_BIT_RSTCFG_PRE_VAL:
	case AW86928_BIT_RSTCFG_PRE_VAL:
	case AW86925_BIT_RSTCFG_VAL:
	case AW86926_BIT_RSTCFG_VAL:
	case AW86927_BIT_RSTCFG_VAL:
	case AW86928_BIT_RSTCFG_VAL:
		ret = -ERANGE;
		break;
	default:
		break;
	}

	return ret;
}

static int read_chipid(struct aw_haptic *aw_haptic, uint32_t *reg_val)
{
	uint8_t value[2] = {0};
	int ret = 0;

	/* try the old way of read chip id */
	ret = haptic_hv_i2c_reads(aw_haptic, AW_REG_CHIPID, &value[0], AW_I2C_BYTE_ONE);
	if (ret < 0)
		return ret;

	ret = judge_value(value[0]);
	if (!ret) {
		*reg_val = value[0];
		return ret;
	}
	/* try the new way of read chip id */
	ret = haptic_hv_i2c_reads(aw_haptic, AW_REG_CHIPIDH, value, AW_I2C_BYTE_TWO);
	if (ret < 0)
		return ret;
	*reg_val = value[0] << 8 | value[1];

	return ret;
}

static int ctrl_init(struct aw_haptic *aw_haptic)
{
	uint32_t reg = 0;
	uint8_t cnt = 0;

	aw_info("enter");
	for (cnt = 0; cnt < AW_READ_CHIPID_RETRIES; cnt++) {
		/* hardware reset */
		hw_reset(aw_haptic);
		if (read_chipid(aw_haptic, &reg) < 0)
			aw_err("read chip id fail");
		switch (reg) {
#ifdef AW869X_DRIVER_ENABLE
		case AW8695_CHIPID:
		case AW8697_CHIPID:
			aw_haptic->func = &aw869x_func_list;
			aw_haptic->ram_vbat_comp = AW_RAM_VBAT_COMP_ENABLE;
			return 0;
#endif
#ifdef AW869XX_DRIVER_ENABLE
		case AW86905_CHIPID:
		case AW86907_CHIPID:
		case AW86915_CHIPID:
		case AW86917_CHIPID:
			aw_haptic->func = &aw869xx_func_list;
			aw_haptic->ram_vbat_comp = AW_RAM_VBAT_COMP_ENABLE;
			return 0;
#endif
#ifdef AW8671X_DRIVER_ENABLE
		case AW86715_CHIPID:
		case AW86716_CHIPID:
		case AW86717_CHIPID:
		case AW86718_CHIPID:
			aw_haptic->func = &aw8671x_func_list;
			aw_haptic->ram_vbat_comp = AW_RAM_VBAT_COMP_ENABLE;
			return 0;
#endif
#ifdef AW8672X_DRIVER_ENABLE
		case AW86727_CHIPID:
		case AW86728_CHIPID:
			aw_haptic->func = &aw8672x_func_list;
			aw_haptic->ram_vbat_comp = AW_RAM_VBAT_COMP_DISABLE;
			return 0;
#endif
#ifdef AW8692X_DRIVER_ENABLE
		case AW86925_CHIPID:
		case AW86926_CHIPID:
		case AW86927_CHIPID:
		case AW86928_CHIPID:
			aw_haptic->func = &aw8692x_func_list;
			aw_haptic->ram_vbat_comp = AW_RAM_VBAT_COMP_ENABLE;
			return 0;
#endif
#ifdef AW8693X_DRIVER_ENABLE
		case AW86936_CHIPID:
		case AW86937_CHIPID:
		case AW86938_CHIPID:
			aw_haptic->func = &aw8693x_func_list;
			aw_haptic->ram_vbat_comp = AW_RAM_VBAT_COMP_DISABLE;
			return 0;
#endif
#ifdef AW8693XS_DRIVER_ENABLE
		case AW86937S_CHIPID:
		case AW86938S_CHIPID:
			aw_haptic->func = &aw8693xs_func_list;
			aw_haptic->ram_vbat_comp = AW_RAM_VBAT_COMP_DISABLE;
			return 0;
#endif
		default:
			aw_info("unexpected chipid!");
			break;
		}
		usleep_range(2000, 2500);
	}

	return -EINVAL;
}

static int parse_chipid(struct aw_haptic *aw_haptic)
{
	int ret = -1;
	uint32_t reg = 0;
	uint8_t cnt = 0;

	for (cnt = 0; cnt < AW_READ_CHIPID_RETRIES; cnt++) {
		ret = read_chipid(aw_haptic, &reg);
		if (ret < 0)
			aw_err("read chip id fail: %d", ret);
		switch (reg) {
#ifdef AW869X_DRIVER_ENABLE
		case AW8695_CHIPID:
			aw_haptic->chipid = AW8695_CHIPID;
			aw_haptic->bst_pc = AW_BST_PC_L1;
			aw_haptic->i2s_config = false;
			aw_info("detected aw8695.");
			aw_haptic->trim_lra_boundary = AW_TRIM_LRA_BOUNDARY;
			return ret;
		case AW8697_CHIPID:
			aw_haptic->chipid = AW8697_CHIPID;
			aw_haptic->bst_pc = AW_BST_PC_L2;
			aw_haptic->i2s_config = false;
			aw_haptic->trim_lra_boundary = AW_TRIM_LRA_BOUNDARY;
			aw_info("detected aw8697.");
			return ret;
#endif
#ifdef AW869XX_DRIVER_ENABLE
		case AW86905_CHIPID:
			aw_haptic->chipid = AW86905_CHIPID;
			aw_haptic->bst_pc = AW_BST_PC_L1;
			aw_haptic->i2s_config = false;
			aw_haptic->trim_lra_boundary = AW_TRIM_LRA_BOUNDARY;
			aw_info("detected aw86905.");
			return 0;
		case AW86907_CHIPID:
			aw_haptic->chipid = AW86907_CHIPID;
			aw_haptic->bst_pc = AW_BST_PC_L2;
			aw_haptic->i2s_config = false;
			aw_haptic->trim_lra_boundary = AW_TRIM_LRA_BOUNDARY;
			aw_info("detected aw86907.");
			return 0;
		case AW86915_CHIPID:
			aw_haptic->chipid = AW86915_CHIPID;
			aw_haptic->bst_pc = AW_BST_PC_L1;
			aw_haptic->i2s_config = true;
			aw_haptic->trim_lra_boundary = AW_TRIM_LRA_BOUNDARY;
			aw_info("detected aw86915.");
			return 0;
		case AW86917_CHIPID:
			aw_haptic->chipid = AW86917_CHIPID;
			aw_haptic->bst_pc = AW_BST_PC_L2;
			aw_haptic->i2s_config = true;
			aw_haptic->trim_lra_boundary = AW_TRIM_LRA_BOUNDARY;
			aw_info("detected aw86917.");
			return 0;
#endif
#ifdef AW8671X_DRIVER_ENABLE
		case AW86715_CHIPID:
			aw_haptic->chipid = AW86715_CHIPID;
			aw_haptic->i2s_config = false;
			aw_haptic->trim_lra_boundary = AW_TRIM_LRA_BOUNDARY;
			aw_info("detected aw86715.");
			return 0;
		case AW86716_CHIPID:
			aw_haptic->chipid = AW86716_CHIPID;
			aw_haptic->i2s_config = true;
			aw_haptic->trim_lra_boundary = AW_TRIM_LRA_BOUNDARY;
			aw_info("detected aw86716.");
			return 0;
		case AW86717_CHIPID:
			aw_haptic->chipid = AW86717_CHIPID;
			aw_haptic->i2s_config = true;
			aw_haptic->trim_lra_boundary = AW_TRIM_LRA_BOUNDARY;
			aw_info("detected aw86717.");
			return 0;
		case AW86718_CHIPID:
			aw_haptic->chipid = AW86718_CHIPID;
			aw_haptic->i2s_config = true;
			aw_haptic->trim_lra_boundary = AW_TRIM_LRA_BOUNDARY;
			aw_info("detected aw86718.");
			return 0;
#endif
#ifdef AW8672X_DRIVER_ENABLE
		case AW86727_CHIPID:
			aw_haptic->chipid = AW86727_CHIPID;
			aw_haptic->i2s_config = true;
			aw_haptic->trim_lra_boundary = AW8672X_TRIM_LRA_BOUNDARY;
			aw_info("detected aw86727.");
			return 0;
		case AW86728_CHIPID:
			aw_haptic->chipid = AW86728_CHIPID;
			aw_haptic->i2s_config = true;
			aw_haptic->trim_lra_boundary = AW8672X_TRIM_LRA_BOUNDARY;
			aw_info("detected aw86728.");
			return 0;
#endif
#ifdef AW8692X_DRIVER_ENABLE
		case AW86925_CHIPID:
			aw_haptic->chipid = AW86925_CHIPID;
			aw_haptic->bst_pc = AW_BST_PC_L1;
			aw_haptic->trim_lra_boundary = AW_TRIM_LRA_BOUNDARY;
			aw_info("detected aw86925.");
			return 0;
		case AW86926_CHIPID:
			aw_haptic->chipid = AW86926_CHIPID;
			aw_haptic->bst_pc = AW_BST_PC_L1;
			aw_haptic->trim_lra_boundary = AW_TRIM_LRA_BOUNDARY;
			aw_info("detected aw86926.");
			return 0;
		case AW86927_CHIPID:
			aw_haptic->chipid = AW86927_CHIPID;
			aw_haptic->bst_pc = AW_BST_PC_L1;
			aw_haptic->trim_lra_boundary = AW_TRIM_LRA_BOUNDARY;
			aw_info("detected aw86927.");
			return 0;
		case AW86928_CHIPID:
			aw_haptic->chipid = AW86928_CHIPID;
			aw_haptic->bst_pc = AW_BST_PC_L1;
			aw_haptic->trim_lra_boundary = AW_TRIM_LRA_BOUNDARY;
			aw_info("detected aw86928.");
			return 0;
#endif
#ifdef AW8693X_DRIVER_ENABLE
		case AW86936_CHIPID:
			aw_haptic->chipid = AW86936_CHIPID;
			aw_haptic->i2s_config = true;
			aw_haptic->trim_lra_boundary = AW8693X_TRIM_LRA_BOUNDARY;
			aw_info("detected aw86936.");
			return 0;
		case AW86937_CHIPID:
			aw_haptic->chipid = AW86937_CHIPID;
			aw_haptic->i2s_config = true;
			aw_haptic->trim_lra_boundary = AW8693X_TRIM_LRA_BOUNDARY;
			aw_info("detected aw86937.");
			return 0;
		case AW86938_CHIPID:
			aw_haptic->chipid = AW86938_CHIPID;
			aw_haptic->i2s_config = true;
            aw_haptic->trim_lra_boundary = AW8693X_TRIM_LRA_BOUNDARY;
			aw_info("detected aw86938.");
			return 0;
#endif
#ifdef AW8693XS_DRIVER_ENABLE
		case AW86937S_CHIPID:
			aw_haptic->chipid = AW86937S_CHIPID;
			aw_haptic->i2s_config = true;
			aw_haptic->trim_lra_boundary = AW8693XS_TRIM_LRA_BOUNDARY;
			aw_info("detected aw86937S.");
			return 0;
		case AW86938S_CHIPID:
			aw_haptic->chipid = AW86938S_CHIPID;
			aw_haptic->i2s_config = true;
			aw_haptic->trim_lra_boundary = AW8693XS_TRIM_LRA_BOUNDARY;
			aw_info("detected aw86938S.");
			return 0;
#endif
		default:
			aw_info("unsupport device revision (0x%02X)", reg);
			break;
		}
		usleep_range(2000, 2500);
}

		return -EINVAL;
}

static void ram_play(struct aw_haptic *aw_haptic, uint8_t mode)
{
#ifdef AW_DOUBLE
#ifndef DUAL_RTP_TEST
	int ret = 0;
#endif
	if (mode == AW_RAM_MODE)
		aw_haptic->func->set_wav_loop(aw_haptic, 0x00, 0x00);
	aw_haptic->func->play_mode(aw_haptic, mode);
#ifndef DUAL_RTP_TEST
	if (aw_haptic->dual_flag) {
		aw_haptic->dual_flag = false;
		ret = down_trylock(&left->sema);
		if (ret == 0) {
			ret = down_interruptible(&left->sema);
		} else {
			up(&left->sema);
			up(&left->sema);
		}
	}
#endif
#else
	aw_haptic->func->play_mode(aw_haptic, mode);
#endif
	aw_haptic->func->play_go(aw_haptic, true);
}

static int get_ram_num(struct aw_haptic *aw_haptic)
{
	uint8_t wave_addr[2] = {0};
	uint32_t first_wave_addr = 0;

	if (!aw_haptic->ram_init) {
		aw_err("ram init faild, ram_num = 0!");
		return -EPERM;
	}
	mutex_lock(&aw_haptic->lock);
#ifdef DUAL_RTP_TEST
	if (aw_haptic == right)
		aw8693x_broadcast_config(aw_haptic, false, AW_RTP_MODE);
#endif
	aw_haptic->func->play_stop(aw_haptic);
	/* RAMINIT Enable */
	aw_haptic->func->ram_init(aw_haptic, true);
	aw_haptic->func->set_ram_addr(aw_haptic);
	aw_haptic->func->get_first_wave_addr(aw_haptic, wave_addr);
	first_wave_addr = (wave_addr[0] << 8 | wave_addr[1]);
	aw_haptic->ram.ram_num = (first_wave_addr - aw_haptic->ram.base_addr - 1) / 4;
	aw_info("first wave addr = 0x%04x", first_wave_addr);
	aw_info("ram_num = %d", aw_haptic->ram.ram_num);
	/* RAMINIT Disable */
	aw_haptic->func->ram_init(aw_haptic, false);
	mutex_unlock(&aw_haptic->lock);

	return 0;
}

static void aw_haptic_effect_strength(struct aw_haptic *aw_haptic)
{
	aw_info("aw_haptic->play.vmax_mv =0x%x\n", aw_haptic->play.vmax_mv);

	if (aw_haptic->play.vmax_mv >= 0x7FFF)
		aw_haptic->gain = 0x80; /*128*/
	else if (aw_haptic->play.vmax_mv <= 0x3FFF)
		aw_haptic->gain = 0x1E; /*30*/
	else
		aw_haptic->gain = (aw_haptic->play.vmax_mv - 16383) / 128;

	if (aw_haptic->gain < 0x1E)
		aw_haptic->gain = 0x1E; /*30*/

	aw_info("aw_haptic->gain =0x%x\n", aw_haptic->gain);
}

static void ram_vbat_comp(struct aw_haptic *aw_haptic, bool flag, unsigned char gain)
{
	int temp_gain = 0;

	if (flag) {
		if (aw_haptic->ram_vbat_comp == AW_RAM_VBAT_COMP_ENABLE) {
			aw_haptic->func->get_vbat(aw_haptic);
			temp_gain = gain * AW_VBAT_REFER / aw_haptic->vbat;
			if (temp_gain > (128 * AW_VBAT_REFER / AW_VBAT_MIN)) {
				temp_gain = 128 * AW_VBAT_REFER / AW_VBAT_MIN;
				aw_dbg("gain limit=%d", temp_gain);
			}
			aw_haptic->func->set_gain(aw_haptic, temp_gain);
			aw_info("ram vbat comp open");
		} else {
			aw_haptic->func->set_gain(aw_haptic, gain);
			aw_info("ram vbat comp close");
		}
	} else {
		aw_haptic->func->set_gain(aw_haptic, gain);
		aw_info("ram vbat comp close");
	}
}

static void calculate_cali_data(struct aw_haptic *aw_haptic)
{
	char f0_cali_lra = 0;
	int f0_cali_step = 0;

	/* calculate cali step */
	if (aw_haptic->chipid == AW86937S_CHIPID || aw_haptic->chipid == AW86938S_CHIPID) {
		f0_cali_step = AW8693XS_CALI_DATA_FORMULA(aw_haptic->f0, aw_haptic->info.f0_pre, aw_haptic->osc_trim_s);
	} else {
		f0_cali_step = 100000 * ((int)aw_haptic->f0 - (int)aw_haptic->info.f0_pre) /
				((int)aw_haptic->info.f0_pre * AW_OSC_CALI_ACCURACY);
	}
		aw_info("f0_cali_step = %d", f0_cali_step);
		if (f0_cali_step >= 0) {	/*f0_cali_step >= 0 */
			if (f0_cali_step % 10 >= 5)
				f0_cali_step = aw_haptic->trim_lra_boundary + (f0_cali_step / 10 + 1);
			else
				f0_cali_step = aw_haptic->trim_lra_boundary + f0_cali_step / 10;
		} else {	/* f0_cali_step < 0 */
			if (f0_cali_step % 10 <= -5)
				f0_cali_step = aw_haptic->trim_lra_boundary + (f0_cali_step / 10 - 1);
			else
				f0_cali_step = aw_haptic->trim_lra_boundary + f0_cali_step / 10;
		}
		if (f0_cali_step >= aw_haptic->trim_lra_boundary)
			f0_cali_lra = (char)f0_cali_step - aw_haptic->trim_lra_boundary;
		else
			f0_cali_lra = (char)f0_cali_step + aw_haptic->trim_lra_boundary;
		/* update cali step */
		aw_haptic->f0_cali_data = (int)f0_cali_lra;
		aw_info("f0_cali_data = 0x%02X", aw_haptic->f0_cali_data);
}

static int judge_cali_range(struct aw_haptic *aw_haptic)
{
	uint32_t f0_cali_min = 0;
	uint32_t f0_cali_max = 0;

	aw_info("enter");
	f0_cali_min = aw_haptic->info.f0_pre * (100 - aw_haptic->info.f0_cali_percent) / 100;
	f0_cali_max = aw_haptic->info.f0_pre * (100 + aw_haptic->info.f0_cali_percent) / 100;

	aw_info("f0_pre = %d, f0_cali_min = %d, f0_cali_max = %d, f0 = %d",
		aw_haptic->info.f0_pre, f0_cali_min, f0_cali_max, aw_haptic->f0);

	if (aw_haptic->f0 < f0_cali_min) {
		aw_err("f0 is too small, f0 = %d!", aw_haptic->f0);
#ifdef AW_USE_MAXIMUM_F0_CALI_DATA
		if (aw_haptic->chipid == AW86937S_CHIPID || aw_haptic->chipid == AW86938S_CHIPID) {
			aw_haptic->f0 = f0_cali_min;
			calculate_cali_data(aw_haptic);
		} else {
			aw_haptic->f0_cali_data = aw_haptic->trim_lra_boundary;
	}
	aw_haptic->func->upload_lra(aw_haptic, AW_F0_CALI_LRA);
#endif
		return -ERANGE;
	}

	if (aw_haptic->f0 > f0_cali_max) {
		aw_err("f0 is too large, f0 = %d!", aw_haptic->f0);
#ifdef AW_USE_MAXIMUM_F0_CALI_DATA
		if (aw_haptic->chipid == AW86937S_CHIPID || aw_haptic->chipid == AW86938S_CHIPID) {
			aw_haptic->f0 = f0_cali_max;
			calculate_cali_data(aw_haptic);
		} else {
			aw_haptic->f0_cali_data = aw_haptic->trim_lra_boundary - 1;
		}
		aw_haptic->func->upload_lra(aw_haptic, AW_F0_CALI_LRA);
#endif
		return -ERANGE;
	}

	return 0;
}

static int f0_cali(struct aw_haptic *aw_haptic)
{
	int ret = 0;

	aw_haptic->func->upload_lra(aw_haptic, AW_WRITE_ZERO);
	if (aw_haptic->func->get_f0(aw_haptic)) {
		aw_err("get f0 error");
	} else {
		ret = judge_cali_range(aw_haptic);
		if (ret < 0)
			return -ERANGE;
		calculate_cali_data(aw_haptic);
		aw_haptic->func->upload_lra(aw_haptic, AW_F0_CALI_LRA);
	}
	/* restore standby work mode */
	aw_haptic->func->play_stop(aw_haptic);

	return ret;
}

void aw_get_sample_params(struct aw_haptic *aw_haptic)
{
	uint8_t restore_d2s_gain = 0;

	restore_d2s_gain = aw_haptic->info.d2s_gain;
	aw_haptic->info.d2s_gain = aw_haptic->info.algo_d2s_gain;

	aw_haptic->func->offset_cali(aw_haptic);
	aw_haptic->func->get_lra_resistance(aw_haptic);
	msleep(200);
	f0_cali(aw_haptic);
	aw_haptic->sample_param[3] = aw_haptic->f0;
	aw_haptic->sample_param[4] = aw_haptic->cont_f0;
	aw_haptic->sample_param[5] = aw_haptic->lra;
	msleep(200);
	aw_haptic->func->get_f0(aw_haptic);
	aw_haptic->func->get_bemf_peak(aw_haptic, aw_haptic->sample_param);
	//restore d2s_gain
	aw_haptic->info.d2s_gain = restore_d2s_gain;
	aw_haptic->f0 = aw_haptic->sample_param[3];
	aw_haptic->func->set_d2s_gain(aw_haptic);
	aw_info("Restore d2s_gain = %d, after used algo d2s_gain", aw_haptic->info.d2s_gain);
}

static int ram_f0_cali(struct aw_haptic *aw_haptic)
{
	int ret = 0;

	aw_haptic->func->upload_lra(aw_haptic, AW_WRITE_ZERO);
	if (aw_haptic->func->ram_get_f0(aw_haptic)) {
		aw_err("get f0 error");
	} else {
		ret = judge_cali_range(aw_haptic);
		if (ret < 0)
			return -ERANGE;
		calculate_cali_data(aw_haptic);
		aw_haptic->func->upload_lra(aw_haptic, AW_F0_CALI_LRA);
	}
	/* restore standby work mode */
	aw_haptic->func->play_stop(aw_haptic);

	return ret;
}

static void pm_qos_enable(struct aw_haptic *aw_haptic, bool enable)
{
#ifdef KERNEL_OVER_5_10
	if (enable) {
		if (!cpu_latency_qos_request_active(&aw_haptic->aw_pm_qos_req_vb))
			cpu_latency_qos_add_request(&aw_haptic->aw_pm_qos_req_vb,
						    CPU_LATENCY_QOC_VALUE);
	} else {
		if (cpu_latency_qos_request_active(&aw_haptic->aw_pm_qos_req_vb))
			cpu_latency_qos_remove_request(&aw_haptic->aw_pm_qos_req_vb);
	}
#else
	if (enable) {
		if (!pm_qos_request_active(&aw_haptic->aw_pm_qos_req_vb))
			pm_qos_add_request(&aw_haptic->aw_pm_qos_req_vb,
					   PM_QOS_CPU_DMA_LATENCY, AW_PM_QOS_VALUE_VB);
	} else {
		if (pm_qos_request_active(&aw_haptic->aw_pm_qos_req_vb))
			pm_qos_remove_request(&aw_haptic->aw_pm_qos_req_vb);
	}
#endif
}

static int rtp_osc_cali(struct aw_haptic *aw_haptic)
{
	uint32_t buf_len = 0;
	int ret = -1;
	const struct firmware *rtp_file;

	aw_haptic->rtp_cnt = 0;
	aw_haptic->timeval_flags = 1;

	/* fw loaded */
	ret = request_firmware(&rtp_file, aw_rtp_name[0], aw_haptic->dev);
	if (ret < 0) {
		aw_err("failed to read %s", aw_rtp_name[0]);
		return ret;
	}
	/*aw_haptic add stop,for irq interrupt during calibrate */
	aw_haptic->func->play_stop(aw_haptic);
	aw_haptic->rtp_init = false;
	mutex_lock(&aw_haptic->rtp_lock);
	vfree(aw_haptic->aw_rtp);
	aw_haptic->aw_rtp = vmalloc(rtp_file->size + sizeof(int));
	if (!aw_haptic->aw_rtp) {
		release_firmware(rtp_file);
		mutex_unlock(&aw_haptic->rtp_lock);
		aw_err("error allocating memory");
		return -ENOMEM;
	}
	aw_haptic->aw_rtp->len = rtp_file->size;
	aw_haptic->rtp_len = rtp_file->size;
	aw_info("rtp file:[%s] size = %dbytes", aw_rtp_name[0], aw_haptic->aw_rtp->len);
	memcpy(aw_haptic->aw_rtp->data, rtp_file->data, rtp_file->size);
	release_firmware(rtp_file);
	mutex_unlock(&aw_haptic->rtp_lock);
	/* gain */
	aw_haptic_effect_strength(aw_haptic);
	ram_vbat_comp(aw_haptic, false, aw_haptic->gain);
	/* rtp mode config */
	aw_haptic->func->play_mode(aw_haptic, AW_RTP_MODE);
	/* bst mode */
	aw_haptic->func->bst_mode_config(aw_haptic, AW_BST_BYPASS_MODE);
	disable_irq(gpio_to_irq(aw_haptic->irq_gpio));
	/* haptic go */
	aw_haptic->func->play_go(aw_haptic, true);
	mutex_lock(&aw_haptic->rtp_lock);
	pm_qos_enable(aw_haptic, true);
	while (1) {
		if (!aw_haptic->func->rtp_get_fifo_afs(aw_haptic)) {
#ifdef AW_ENABLE_RTP_PRINT_LOG
			aw_info("not almost_full, aw_haptic->rtp_cnt=%d", aw_haptic->rtp_cnt);
#endif
			if ((aw_haptic->aw_rtp->len - aw_haptic->rtp_cnt) <
			    (aw_haptic->ram.base_addr >> 2))
				buf_len = aw_haptic->aw_rtp->len - aw_haptic->rtp_cnt;
			else
				buf_len = (aw_haptic->ram.base_addr >> 2);
			if (aw_haptic->rtp_cnt != aw_haptic->aw_rtp->len) {
				if (aw_haptic->timeval_flags == 1) {
					aw_haptic->kstart = ktime_get();
					aw_haptic->timeval_flags = 0;
				}
				aw_haptic->func->set_rtp_data(aw_haptic,
					&aw_haptic->aw_rtp->data[aw_haptic->rtp_cnt], buf_len);
				aw_haptic->rtp_cnt += buf_len;
			}
		}
		if (aw_haptic->func->get_osc_status(aw_haptic)) {
			aw_haptic->kend = ktime_get();
			aw_info("osc trim playback done aw_haptic->rtp_cnt= %d",
				aw_haptic->rtp_cnt);
			break;
		}
		aw_haptic->kend = ktime_get();
		aw_haptic->microsecond = ktime_to_us(ktime_sub(aw_haptic->kend, aw_haptic->kstart));
		if (aw_haptic->microsecond > AW_OSC_CALI_MAX_LENGTH) {
			aw_info("osc trim time out! aw_haptic->rtp_cnt %d", aw_haptic->rtp_cnt);
			break;
		}
	}
	pm_qos_enable(aw_haptic, false);
	mutex_unlock(&aw_haptic->rtp_lock);
	enable_irq(gpio_to_irq(aw_haptic->irq_gpio));
	aw_haptic->microsecond = ktime_to_us(ktime_sub(aw_haptic->kend, aw_haptic->kstart));
	/* calibration osc */
	aw_info("microsecond: %llu", aw_haptic->microsecond);

	return 0;
}

static void rtp_trim_lra_cali(struct aw_haptic *aw_haptic)
{
#ifdef AW_OSC_MULTI_CALI
	uint8_t last_trim_code = 0;
	int temp = 0;
	int count = 5;
#endif
	uint32_t lra_trim_code = 0;
	/* 0.1 percent below no need to calibrate */
	uint32_t osc_cali_threshold = 10;
	int32_t real_code = 0;
	uint32_t theory_time = 0;
	uint32_t real_time = 0;

	aw_haptic->func->upload_lra(aw_haptic, AW_WRITE_ZERO);
#ifdef AW_OSC_MULTI_CALI
	while (count) {
#endif
	rtp_osc_cali(aw_haptic);
	real_time = aw_haptic->microsecond;
	theory_time = aw_haptic->func->get_theory_time(aw_haptic);
	if (theory_time == real_time) {
			aw_info("theory_time == real_time: %d, no need to calibrate!", real_time);
		return;
	} else if (theory_time < real_time) {
			if ((real_time - theory_time) > (theory_time / AW_OSC_TRIM_PARAM)) {
			aw_info("(real_time - theory_time) > (theory_time/50), can't calibrate!");
			return;
		}

			if ((real_time - theory_time) < (osc_cali_threshold * theory_time / 10000)) {
			aw_info("real_time: %d, theory_time: %d, no need to calibrate!",
				real_time, theory_time);
			return;
		}

		real_code = 100000 * ((real_time - theory_time)) /
			    (theory_time * AW_OSC_CALI_ACCURACY);
		real_code = ((real_code % 10 < 5) ? 0 : 1) + real_code / 10;
		real_code = aw_haptic->trim_lra_boundary + real_code;
	} else if (theory_time > real_time) {
			if ((theory_time - real_time) > (theory_time / AW_OSC_TRIM_PARAM)) {
			aw_info("(theory_time - real_time) > (theory_time / 50), can't calibrate!");
			return;
		}
			if ((theory_time - real_time) < (osc_cali_threshold * theory_time / 10000)) {
			aw_info("real_time: %d, theory_time: %d, no need to calibrate!",
				real_time, theory_time);
			return;
		}

		real_code = (theory_time - real_time) / (theory_time / 100000) / AW_OSC_CALI_ACCURACY;
		real_code = ((real_code % 10 < 5) ? 0 : 1) + real_code / 10;
		real_code = aw_haptic->trim_lra_boundary - real_code;
	}

		if (aw_haptic->chipid == AW86937S_CHIPID || aw_haptic->chipid == AW86938S_CHIPID) {
			real_code = (10 * ((long)theory_time * (10000 + aw_haptic->osc_trim_s * AW8693XS_OSC_CALI_ACCURACY) -
							10000 * (long)real_time) / ((long)real_time * AW8693XS_OSC_CALI_ACCURACY));
			aw_info("real_code: %d", real_code);
			if (real_code >= 0) {	/*f0_cali_step >= 0 */
				if (real_code % 10 >= 5)
					real_code = aw_haptic->trim_lra_boundary + (real_code / 10 + 1);
				else
					real_code = aw_haptic->trim_lra_boundary + real_code / 10;
			} else {	/* f0_cali_step < 0 */
				if (real_code % 10 <= -5)
					real_code = aw_haptic->trim_lra_boundary + (real_code / 10 - 1);
				else
					real_code = aw_haptic->trim_lra_boundary + real_code / 10;
			}
		}
	if (real_code >= aw_haptic->trim_lra_boundary)
		lra_trim_code = real_code - aw_haptic->trim_lra_boundary;
	else
		lra_trim_code = real_code + aw_haptic->trim_lra_boundary;
#ifdef AW_OSC_MULTI_CALI
		if (count == 5)
			last_trim_code = 0;
		else
	last_trim_code = aw_haptic->func->get_trim_lra(aw_haptic);
	if (last_trim_code) {
		if (lra_trim_code >= aw_haptic->trim_lra_boundary) {
			temp = last_trim_code - (aw_haptic->trim_lra_boundary * 2 - lra_trim_code);
			if (temp < aw_haptic->trim_lra_boundary && temp > 0 && last_trim_code >= aw_haptic->trim_lra_boundary)
				temp = aw_haptic->trim_lra_boundary;
			if (temp < 0)
				temp = lra_trim_code + last_trim_code;
		} else if (lra_trim_code > 0 && lra_trim_code < aw_haptic->trim_lra_boundary) {
			temp = (last_trim_code + lra_trim_code) & (aw_haptic->trim_lra_boundary * 2 - 1);
			if (temp >= aw_haptic->trim_lra_boundary && last_trim_code < aw_haptic->trim_lra_boundary)
				temp = aw_haptic->trim_lra_boundary - 1;
			if ((aw_haptic->trim_lra_boundary * 2 - last_trim_code) <= lra_trim_code)
				temp = last_trim_code + lra_trim_code - aw_haptic->trim_lra_boundary * 2;
		} else {
			temp = last_trim_code;
		}
		aw_haptic->osc_cali_data = temp;
	} else {
		aw_haptic->osc_cali_data = lra_trim_code;
	}
#else
	aw_haptic->osc_cali_data = lra_trim_code;
#endif
	aw_haptic->func->upload_lra(aw_haptic, AW_OSC_CALI_LRA);
	aw_info("real_time: %d, theory_time: %d", real_time, theory_time);
	aw_info("real_code: %d, trim_lra: 0x%02X", real_code, lra_trim_code);
#ifdef AW_OSC_MULTI_CALI
	count--;
	}
#endif
}

#ifdef AW_INPUT_FRAMEWORK
static void input_stop_work_routine(struct work_struct *work)
{
	struct aw_haptic *aw_haptic = container_of(work, struct aw_haptic, stop_work);

	mutex_lock(&aw_haptic->lock);
	hrtimer_cancel(&aw_haptic->timer);
	aw_haptic->func->play_stop(aw_haptic);
	mutex_unlock(&aw_haptic->lock);
}

static void input_gain_work_routine(struct work_struct *work)
{
	struct aw_haptic *aw_haptic = container_of(work, struct aw_haptic, gain_work);

	mutex_lock(&aw_haptic->lock);
	aw_haptic->func->set_gain(aw_haptic, aw_haptic->gain);
	mutex_unlock(&aw_haptic->lock);
}

static void input_vib_work_routine(struct work_struct *work)
{
	struct aw_haptic *aw_haptic = container_of(work, struct aw_haptic, input_vib_work);

	aw_info("input_vib_work_routine: enter\n");
	aw_info("input_vib_work_routine: index = %d state=%d activate_mode = %d duration = %d\n",
		aw_haptic->index, aw_haptic->state, aw_haptic->activate_mode,
		aw_haptic->duration);
	mutex_lock(&aw_haptic->lock);
	/* Enter standby mode */
	hrtimer_cancel(&aw_haptic->timer);
	aw_haptic->func->play_stop(aw_haptic);
	if (aw_haptic->state) {
		aw_haptic->func->upload_lra(aw_haptic, AW_F0_CALI_LRA);
		if (aw_haptic->activate_mode == AW_RAM_MODE) {
			aw_haptic_effect_strength(aw_haptic);
			ram_vbat_comp(aw_haptic, false, aw_haptic->gain);
			aw_haptic->func->set_wav_seq(aw_haptic, 0x00, aw_haptic->index + 1);
			aw_haptic->func->set_wav_seq(aw_haptic, 0x01, 0x00);
			aw_haptic->func->set_wav_loop(aw_haptic, 0x00, 0x00);
			ram_play(aw_haptic, AW_RAM_MODE);
		} else if (aw_haptic->activate_mode == AW_RAM_LOOP_MODE) {
			aw_haptic_effect_strength(aw_haptic);
			ram_vbat_comp(aw_haptic, true, aw_haptic->gain);
			aw_haptic->func->set_repeat_seq(aw_haptic, aw_haptic->index);
			ram_play(aw_haptic, AW_RAM_LOOP_MODE);
			/* run ms timer */
			hrtimer_start(&aw_haptic->timer, ktime_set(aw_haptic->duration / 1000,
				      (aw_haptic->duration % 1000) * 1000000), HRTIMER_MODE_REL);
			pm_stay_awake(aw_haptic->dev);
  			aw_haptic->wk_lock_flag = 1;
		} else if (aw_haptic->activate_mode == AW_CONT_MODE) {
			aw_haptic->func->cont_config(aw_haptic);
			/* run ms timer */
			hrtimer_start(&aw_haptic->timer, ktime_set(aw_haptic->duration / 1000,
				      (aw_haptic->duration % 1000) * 1000000), HRTIMER_MODE_REL);
		} else {
			aw_err("activate_mode error");
		}
	}else {
  		if (aw_haptic->wk_lock_flag == 1) {
  			pm_relax(aw_haptic->dev);
  			aw_haptic->wk_lock_flag = 0;
  		}
  	}
	mutex_unlock(&aw_haptic->lock);
}

static int input_upload_effect(struct input_dev *dev, struct ff_effect *effect,
			       struct ff_effect *old)
{
	struct aw_haptic *aw_haptic = input_get_drvdata(dev);
	struct qti_hap_play_info *play = &aw_haptic->play;
	s16 data[CUSTOM_DATA_LEN];
	ktime_t rem;
	s64 time_us;
	int ret = 0;

	aw_info("effect->type=0x%x,FF_CONSTANT=0x%x,FF_PERIODIC=0x%x\n",
		effect->type, FF_CONSTANT, FF_PERIODIC);

#ifdef DUAL_RTP_TEST
	if (!left->ram_init || !right->ram_init) {
		aw_err("ram init failed, not allow to play!");
		return -ERANGE;
	}
else
	if (!aw_haptic->ram_init) {
		aw_err("ram init failed, not allow to play!");
		return -ERANGE;
	}
#endif

  	if (hrtimer_active(&aw_haptic->timer)) {
  		rem = hrtimer_get_remaining(&aw_haptic->timer);
  		time_us = ktime_to_us(rem);
  		aw_info("waiting for playing clear sequence: %lld us\n",
  			time_us);
  		usleep_range(time_us, time_us + 100);
  	}
	aw_haptic->effect_type = effect->type;
	mutex_lock(&aw_haptic->lock);
	while (atomic_read(&aw_haptic->exit_in_rtp_loop)) {
		aw_info("goint to waiting rtp  exit\n");
		mutex_unlock(&aw_haptic->lock);
		ret = wait_event_interruptible(aw_haptic->stop_wait_q,
				atomic_read(&aw_haptic->exit_in_rtp_loop) == 0);
		aw_info("wakeup\n");
		if (ret == -ERESTARTSYS) {
			aw_err("wake up by signal return erro\n");
			return ret;
		}
		mutex_lock(&aw_haptic->lock);
	}

	if (aw_haptic->effect_type == FF_CONSTANT) {
		/*cont mode set duration */
		aw_haptic->duration = effect->replay.length;
		aw_haptic->activate_mode = AW_RAM_LOOP_MODE;
		aw_haptic->index = aw_haptic->ram.ram_num;
		aw_info("waveform id = %d", aw_haptic->index);
#ifdef DUAL_RTP_TEST
		aw_haptic->func->play_dual_ram();
#endif
	} else if (aw_haptic->effect_type == FF_PERIODIC) {
		if (copy_from_user(data, effect->u.periodic.custom_data,
				   sizeof(s16) * CUSTOM_DATA_LEN)) {
			aw_err("copy from user error %d!!", ret);
			mutex_unlock(&aw_haptic->lock);
			return -EFAULT;
		}
		aw_info("waveform id = %d", data[0]);
		aw_haptic->index = data[0];
		play->vmax_mv = effect->u.periodic.magnitude;
		if (left != NULL) {
			left->play.vmax_mv = play->vmax_mv;
		}
		aw_info("aw_haptic->info.effect_max = %d", aw_haptic->info.effect_max);
		if (aw_haptic->index < 0) {
			mutex_unlock(&aw_haptic->lock);
			return 0;
		}
		if (aw_haptic->index > aw_haptic->info.effect_max) {
			aw_haptic->index = aw_haptic->info.effect_max;
		}
		aw_haptic->is_custom_wave = 0;
		aw_info("aw_haptic->ram.ram_num = %d", aw_haptic->ram.ram_num);
		if (aw_haptic->index < aw_haptic->ram.ram_num) {
			aw_haptic->activate_mode = AW_RAM_MODE;
			aw_info("aw_haptic->index=%d , aw_haptic->activate_mode = %d\n",
				aw_haptic->index,aw_haptic->activate_mode);
 			if(aw_haptic->index == 4){
  				/*second data*/
  				data[1] = 0;
  				/*millisecond data*/
  				data[2] = 28;
  			}else{
  				/*second data*/
  				data[1] = 0;
  				/*millisecond data*/
  				data[2] = 20;
  			}
#ifdef DUAL_RTP_TEST
			aw_haptic->func->play_dual_ram();
#endif
		}else{
			if (aw_haptic->index >= aw_haptic->ram.ram_num) {
				aw_haptic->activate_mode = AW_RTP_MODE;
				aw_info("aw_haptic->index=%d , aw_haptic->activate_mode = %d\n",
					aw_haptic->index,aw_haptic->activate_mode);
				/*second data*/
				data[1] = 30;
				/*millisecond data*/
				data[2] = 0;
			}
			if (aw_haptic->index == CUSTOME_WAVE_ID) {
				aw_haptic->activate_mode = AW_RTP_MODE;
				aw_info("aw_haptic->index=%d , aw_haptic->activate_mode = %d\n",
					aw_haptic->index,aw_haptic->activate_mode);
				/*second data*/
				data[1] = 30;
				/*millisecond data*/
				data[2] = 0;
				aw_haptic->is_custom_wave = 1;
				rb_init();
			}
#ifdef DUAL_RTP_TEST
			aw_haptic->func->play_dual_rtp();
#endif
		}
		if (copy_to_user(effect->u.periodic.custom_data, data,
			sizeof(s16) * CUSTOM_DATA_LEN)) {
			mutex_unlock(&aw_haptic->lock);
			return -EFAULT;
		}
	} else {
		aw_err("%s Unsupported effect type: %d\n", __func__,
		       effect->type);
	}
	mutex_unlock(&aw_haptic->lock);
	return 0;
}

int aw_haptic_upload_effect_to_ext(struct input_dev *dev, struct ff_effect *effect,
			       struct ff_effect *old)
{
	struct aw_haptic *aw_haptic = input_get_drvdata(dev);
	struct qti_hap_play_info *play = &aw_haptic->play;
	s16 data[CUSTOM_DATA_LEN];
	ktime_t rem;
	s64 time_us;
	int ret = 0;

	aw_info("input_upload_effect_to_ext: effect->type=0x%x,FF_CONSTANT=0x%x,FF_PERIODIC=0x%x\n",
		effect->type, FF_CONSTANT, FF_PERIODIC);

  	if (hrtimer_active(&aw_haptic->timer)) {
  		rem = hrtimer_get_remaining(&aw_haptic->timer);
  		time_us = ktime_to_us(rem);
  		aw_info("waiting for playing clear sequence: %lld us\n",
  			time_us);
  		usleep_range(time_us, time_us + 100);
  	}
	aw_haptic->effect_type = effect->type;
	mutex_lock(&aw_haptic->lock);
	while (atomic_read(&aw_haptic->exit_in_rtp_loop)) {
		aw_info("input_upload_effect_to_ext: goint to waiting rtp  exit\n");
		mutex_unlock(&aw_haptic->lock);
		ret = wait_event_interruptible(aw_haptic->stop_wait_q,
				atomic_read(&aw_haptic->exit_in_rtp_loop) == 0);
		aw_info("input_upload_effect_to_ext: wakeup\n");
		if (ret == -ERESTARTSYS) {
			aw_err("input_upload_effect_to_ext: wake up by signal return erro\n");
			return ret;
		}
		mutex_lock(&aw_haptic->lock);
	}

	if (aw_haptic->effect_type == FF_CONSTANT) {
		/*cont mode set duration */
		aw_haptic->duration = effect->replay.length;
		aw_haptic->activate_mode = AW_RAM_LOOP_MODE;
		aw_haptic->index = aw_haptic->ram.ram_num;
		aw_info("input_upload_effect_to_ext:waveform id = %d", aw_haptic->index);
#ifdef DUAL_RTP_TEST
		aw_haptic->func->play_dual_ram();
#endif
	} else if (aw_haptic->effect_type == FF_PERIODIC) {
		memcpy(data, effect->u.periodic.custom_data,
				   sizeof(s16) * CUSTOM_DATA_LEN);
		//mutex_unlock(&aw_haptic->lock);
		/*if (copy_from_user(data, effect->u.periodic.custom_data,
				   sizeof(s16) * CUSTOM_DATA_LEN)) {
			aw_err("input_upload_effect:copy from user error %d!!", ret);
			mutex_unlock(&aw_haptic->lock);
			return -EFAULT;
		}*/
		aw_info("input_upload_effect_to_ext:waveform id = %d", data[0]);
		aw_haptic->index = data[0];
		play->vmax_mv = effect->u.periodic.magnitude; /*vmax level*/
		aw_info("input_upload_effect_to_ext:aw_haptic->info.effect_max = %d", aw_haptic->info.effect_max);
		if (aw_haptic->index < 0 ||
			aw_haptic->index > aw_haptic->info.effect_max) {
			mutex_unlock(&aw_haptic->lock);
			return 0;
		}
		aw_haptic->is_custom_wave = 0;
		aw_info("input_upload_effect_to_ext:aw_haptic->ram.ram_num = %d", aw_haptic->ram.ram_num);
		if (aw_haptic->index < aw_haptic->ram.ram_num) {
			aw_haptic->activate_mode = AW_RAM_MODE;
			aw_info("input_upload_effect_to_ext: aw_haptic->index=%d , aw_haptic->activate_mode = %d\n",
				aw_haptic->index,aw_haptic->activate_mode);
 			if(aw_haptic->index == 4){
  				/*second data*/
  				data[1] = 0;
  				/*millisecond data*/
  				data[2] = 28;
  			}else{
  				/*second data*/
  				data[1] = 0;
  				/*millisecond data*/
  				data[2] = 20;
  			}
#ifdef DUAL_RTP_TEST
			aw_haptic->func->play_dual_ram();
#endif
		}else{
			if (aw_haptic->index >= aw_haptic->ram.ram_num) {
				aw_haptic->activate_mode = AW_RTP_MODE;
				aw_info("input_upload_effect_to_ext: aw_haptic->index=%d , aw_haptic->activate_mode = %d\n",
					aw_haptic->index,aw_haptic->activate_mode);
				/*second data*/
				data[1] = 30;
				/*millisecond data*/
				data[2] = 0;
			}
			if (aw_haptic->index == CUSTOME_WAVE_ID) {
				aw_haptic->activate_mode = AW_RTP_MODE;
				aw_info("input_upload_effect_to_ext: aw_haptic->index=%d , aw_haptic->activate_mode = %d\n",
					aw_haptic->index,aw_haptic->activate_mode);
				/*second data*/
				data[1] = 30;
				/*millisecond data*/
				data[2] = 0;
				aw_haptic->is_custom_wave = 1;
				rb_init();
			}
#ifdef DUAL_RTP_TEST
			aw_haptic->func->play_dual_rtp();
#endif
		}
		memcpy(data, effect->u.periodic.custom_data,
			sizeof(s16) * CUSTOM_DATA_LEN);
		//mutex_unlock(&aw_haptic->lock);
		/*if (copy_to_user(effect->u.periodic.custom_data, data,
			sizeof(s16) * CUSTOM_DATA_LEN)) {
			mutex_unlock(&aw_haptic->lock);
			return -EFAULT;
		}*/
	} else {
		aw_err("%s Unsupported effect type: %d\n", __func__,
		       effect->type);
	}
	mutex_unlock(&aw_haptic->lock);
	return 0;
}
EXPORT_SYMBOL_GPL(aw_haptic_upload_effect_to_ext);

int input_playback(struct input_dev *dev, int effect_id, int val)
{
	struct aw_haptic *aw_haptic = input_get_drvdata(dev);

	aw_info("index=%d , activate_mode = %d val = %d\n",
		aw_haptic->index, aw_haptic->activate_mode, val);
#ifdef DUAL_RTP_TEST
	if (!left->ram_init || !right->ram_init) {
		aw_err("ram init failed, not allow to play!");
		return -ERANGE;
	}
else
	if (!aw_haptic->ram_init) {
		aw_err("ram init failed, not allow to play!");
		return -ERANGE;
	}
#endif
	if (val > 0)
		aw_haptic->state = 1;
	if(val <= 0)
		aw_haptic->state = 0;
	hrtimer_cancel(&aw_haptic->timer);

	if (aw_haptic->effect_type == FF_CONSTANT &&
		aw_haptic->activate_mode == AW_RAM_LOOP_MODE) {
		aw_info("enter ram_loop_mode\n");
		if(atomic_read(&block_flag) != 0) {
			aw_info("rtp_work write data status is blocking, need to exit first!\n");
			rb_force_exit();
		}
		queue_work(aw_haptic->work_queue, &aw_haptic->input_vib_work);
	} else if (aw_haptic->effect_type == FF_PERIODIC &&
		aw_haptic->activate_mode == AW_RAM_MODE) {
		aw_info("enter ram_mode\n");
		if(atomic_read(&block_flag) != 0) {
			aw_info("rtp_work write data status is blocking, need to exit first!\n");
			rb_force_exit();
		}
		queue_work(aw_haptic->work_queue, &aw_haptic->input_vib_work);
	} else if ((aw_haptic->effect_type == FF_PERIODIC) &&
		aw_haptic->activate_mode == AW_RTP_MODE) {
		aw_info("enter rtp_mode\n");
		queue_work(aw_haptic->work_queue, &aw_haptic->rtp_work);
		/*if we are in the play mode, force to exit*/
		if (val == 0) {
			atomic_set(&aw_haptic->exit_in_rtp_loop, 1);
			rb_force_exit();
			wake_up_interruptible(&aw_haptic->stop_wait_q);
		}
	} else {
		/*other mode */
	}

	return 0;
}
EXPORT_SYMBOL_GPL(input_playback);

static int input_erase(struct input_dev *dev, int effect_id)
{
	struct aw_haptic *aw_haptic = input_get_drvdata(dev);

	aw_haptic->effect_type = 0;
	aw_haptic->is_custom_wave = 0;
	aw_haptic->duration = 0;

	return 0;
}

static void input_set_gain(struct input_dev *dev, uint16_t gain)
{
	struct aw_haptic *aw_haptic = input_get_drvdata(dev);

	aw_haptic->play.vmax_mv = gain;
	if (left != NULL) {
		left->play.vmax_mv = gain;
	}
	if (gain >= 0x7FFF)
		aw_haptic->gain = 0x80;	/*128 */
	else if (gain <= 0x3FFF)
		aw_haptic->gain = 0x1E;	/*30 */
	else
		aw_haptic->gain = (gain - 16383) / 128;

	if (aw_haptic->gain < 0x1E)
		aw_haptic->gain = 0x1E;	/*30 */

	queue_work(aw_haptic->work_queue, &aw_haptic->gain_work);
	aw_info("gain = 0x%02x , aw_haptic->gain = 0x%02x\n", gain, aw_haptic->gain);
}

static int input_framework_init(struct aw_haptic *aw_haptic)
{
	struct input_dev *input_dev;
	int ret = 0;

	input_dev = devm_input_allocate_device(aw_haptic->dev);
	if (input_dev == NULL)
		return -ENOMEM;
	input_dev->name = "aw-haptic-hv";
	input_set_drvdata(input_dev, aw_haptic);
	aw_haptic->input_dev = input_dev;
	input_set_capability(input_dev, EV_FF, FF_GAIN);
	input_set_capability(input_dev, EV_FF, FF_CONSTANT);
	input_set_capability(input_dev, EV_FF, FF_PERIODIC);
	input_set_capability(input_dev, EV_FF, FF_CUSTOM);
	ret = input_ff_create(input_dev, AW_EFFECT_NUMBER);
	if (ret < 0) {
		aw_err("create input FF device failed, rc=%d\n", ret);
		return ret;
	}
	input_dev->ff->upload = input_upload_effect;
	input_dev->ff->playback = input_playback;
	input_dev->ff->erase = input_erase;
	input_dev->ff->set_gain = input_set_gain;
	INIT_WORK(&aw_haptic->gain_work, input_gain_work_routine);
	INIT_WORK(&aw_haptic->stop_work, input_stop_work_routine);
	INIT_WORK(&aw_haptic->input_vib_work, input_vib_work_routine);
	ret = input_register_device(input_dev);
	if (ret < 0) {
		aw_err("register input device failed, rc=%d\n", ret);
		input_ff_destroy(aw_haptic->input_dev);
		return ret;
	}

	return ret;
}
#endif

static enum hrtimer_restart vibrator_timer_func(struct hrtimer *timer)
{
	struct aw_haptic *aw_haptic = container_of(timer, struct aw_haptic, timer);

	aw_haptic->state = 0;
	queue_work(aw_haptic->work_queue, &aw_haptic->vibrator_work);

	return HRTIMER_NORESTART;
}

#ifdef AW_DURATION_DECIDE_WAVEFORM
static int ram_select_waveform(struct aw_haptic *aw_haptic)
{
	uint8_t wavseq = 0;
	uint8_t wavloop = 0;

	if (aw_haptic->duration <= 0) {
		aw_err("duration time %d error", aw_haptic->duration);
		return -ERANGE;
	}
	aw_haptic->activate_mode = AW_RAM_MODE;
	if ((aw_haptic->duration > 0) && (aw_haptic->duration < aw_haptic->info.duration_time[0])) {
		wavseq = 3;
	} else if ((aw_haptic->duration >= aw_haptic->info.duration_time[0]) &&
		   (aw_haptic->duration < aw_haptic->info.duration_time[1])) {
		wavseq = 2;
	} else if ((aw_haptic->duration >= aw_haptic->info.duration_time[1]) &&
		   (aw_haptic->duration < aw_haptic->info.duration_time[2])) {
		wavseq = 1;
	} else if (aw_haptic->duration >= aw_haptic->info.duration_time[2]) {
		wavseq = 4;
		wavloop = 15;
		aw_haptic->activate_mode = AW_RAM_LOOP_MODE;
	}
	aw_info("duration %d, select index %d", aw_haptic->duration, wavseq);
	aw_haptic->func->set_wav_seq(aw_haptic, 0, wavseq);
	aw_haptic->func->set_wav_loop(aw_haptic, 0, wavloop);
	aw_haptic->func->set_wav_seq(aw_haptic, 1, 0);
	aw_haptic->func->set_wav_loop(aw_haptic, 1, 0);

	return 0;
}
#endif

static void vibrator_work_routine(struct work_struct *work)
{
	struct aw_haptic *aw_haptic = container_of(work, struct aw_haptic, vibrator_work);

	aw_info("vibrator_work_routine: enter\n");
	aw_info("vibrator_work_routine: index = %d state=%d activate_mode = %d duration = %d\n",
		aw_haptic->index, aw_haptic->state, aw_haptic->activate_mode,
		aw_haptic->duration);
	mutex_lock(&aw_haptic->lock);
	/* Enter standby mode */
	hrtimer_cancel(&aw_haptic->timer);
	aw_haptic->func->play_stop(aw_haptic);
	if (aw_haptic->state) {
#ifdef AW_DURATION_DECIDE_WAVEFORM
		ram_select_waveform(aw_haptic);
#endif
		aw_haptic->func->upload_lra(aw_haptic, AW_F0_CALI_LRA);
		if (aw_haptic->activate_mode == AW_RAM_MODE) {
			aw_haptic_effect_strength(aw_haptic);
			ram_vbat_comp(aw_haptic, false,aw_haptic->gain);
			ram_play(aw_haptic, AW_RAM_MODE);
		} else if (aw_haptic->activate_mode == AW_RAM_LOOP_MODE) {
			aw_haptic_effect_strength(aw_haptic);
			ram_vbat_comp(aw_haptic, true, aw_haptic->gain);
			ram_play(aw_haptic, AW_RAM_LOOP_MODE);
			/* run ms timer */
			hrtimer_start(&aw_haptic->timer, ktime_set(aw_haptic->duration / 1000,
				      (aw_haptic->duration % 1000) * 1000000), HRTIMER_MODE_REL);
			pm_stay_awake(aw_haptic->dev);
  			aw_haptic->wk_lock_flag = 1;
		} else if (aw_haptic->activate_mode == AW_CONT_MODE) {
			aw_haptic->func->cont_config(aw_haptic);
			/* run ms timer */
			hrtimer_start(&aw_haptic->timer, ktime_set(aw_haptic->duration / 1000,
				      (aw_haptic->duration % 1000) * 1000000), HRTIMER_MODE_REL);
		} else {
			aw_err("activate_mode error");
		}
	} else {
  		if (aw_haptic->wk_lock_flag == 1) {
  			pm_relax(aw_haptic->dev);
  			aw_haptic->wk_lock_flag = 0;
  		}
  	}
	mutex_unlock(&aw_haptic->lock);
}

static int rtp_play(struct aw_haptic *aw_haptic)
{
	uint32_t buf_len = 0;
	int ret = 0;
	unsigned int period_size = aw_haptic->ram.base_addr >> 2;
	struct aw_haptic_container *aw_rtp = aw_haptic->aw_rtp;

	aw_info("enter rtp_play\n");
	aw_haptic->rtp_cnt = 0;
	disable_irq(gpio_to_irq(aw_haptic->irq_gpio));
	while ((!aw_haptic->func->rtp_get_fifo_afs(aw_haptic))
	       && (aw_haptic->play_mode == AW_RTP_MODE) &&
		   !atomic_read(&aw_haptic->exit_in_rtp_loop)) {
		if (!aw_rtp) {
			aw_info("aw_rtp is null, break!");
			ret = -ERANGE;
			break;
		}
		if (aw_haptic->is_custom_wave == 0) {
			if (aw_haptic->rtp_cnt < (aw_haptic->ram.base_addr)) {
				if ((aw_rtp->len - aw_haptic->rtp_cnt) < (aw_haptic->ram.base_addr))
					buf_len = aw_rtp->len - aw_haptic->rtp_cnt;
				else
					buf_len = aw_haptic->ram.base_addr;
			} else if ((aw_rtp->len - aw_haptic->rtp_cnt) < (aw_haptic->ram.base_addr >> 2)) {
				buf_len = aw_rtp->len - aw_haptic->rtp_cnt;
			} else {
				buf_len = aw_haptic->ram.base_addr >> 2;
			}
#ifdef AW_ENABLE_RTP_PRINT_LOG
			aw_info("buf_len = %d", buf_len);
#endif
#ifdef AW_DOUBLE
#ifndef DUAL_RTP_TEST
			if (aw_haptic->rtp_cnt == 0 && aw_haptic->dual_flag) {
				aw_haptic->dual_flag = false;
				if (down_trylock(&left->sema) == 0) {
					(void)down_interruptible(&left->sema);
				} else {
					up(&left->sema);
					up(&left->sema);
				}
				aw_info("dual rtp play start");
			}
#endif
#endif
			aw_haptic->func->set_rtp_data(aw_haptic,
					      &aw_rtp->data[aw_haptic->rtp_cnt], buf_len);
			aw_haptic->rtp_cnt += buf_len;
			if (aw_haptic->rtp_cnt == aw_rtp->len) {
				aw_info("rtp update complete!");
				aw_haptic->rtp_cnt = 0;
				aw_haptic->func->set_rtp_aei(aw_haptic, false);
				break;
			}
		}else{
			buf_len = read_rb(aw_rtp->data,  period_size);
			aw_haptic->func->set_rtp_data(aw_haptic, aw_rtp->data, buf_len);
			if (buf_len < period_size) {
				aw_info("%s: custom rtp update complete\n",
					__func__);
				aw_haptic->rtp_cnt = 0;
				aw_haptic->func->set_rtp_aei(aw_haptic, false);
				break;
			}
		}
	}
	enable_irq(gpio_to_irq(aw_haptic->irq_gpio));
	if (aw_haptic->play_mode == AW_RTP_MODE &&
		!atomic_read(&aw_haptic->exit_in_rtp_loop)) {
		atomic_set(&aw_haptic->richtap_rtp_mode, false);
		aw_haptic->func->set_rtp_aei(aw_haptic, true);
	}
	aw_info("rtp_play: exit\n");
	return ret;
}

static int wait_enter_rtp_mode(struct aw_haptic *aw_haptic)
{
	bool rtp_work_flag = false;
	uint8_t ret = 0;
	int cnt = 200;

	while (cnt) {
		ret = aw_haptic->func->judge_rtp_going(aw_haptic);
		if (ret) {
			rtp_work_flag = true;
			aw_info("RTP_GO!");
			break;
		}
		cnt--;
		aw_info("wait for RTP_GO, glb_state=0x%02X", ret);
		usleep_range(2000, 2500);
	}
	if (!rtp_work_flag) {
		aw_haptic->func->play_stop(aw_haptic);
		aw_err("failed to enter RTP_GO status!");
		return -ERANGE;
	}

	return 0;
}

#ifdef AW8693XS_RTP_F0_CALI
/************ RTP F0 Cali Start ****************/
static uint8_t aw_rtp_vib_data_des(uint8_t *data, int lens, int cnt_set)
{
	uint8_t ret = 0;
	int i = 0;
	int cnt = 0;

	for (i = 0; i < cnt_set; i++) {
		if ((i + 1) >= lens)
			return ret;
		if (abs(data[i]) <= abs(data[i + 1]))
			cnt++;
	}
	if (cnt == cnt_set)
		ret = 1;

	return ret;
}

static uint8_t aw_rtp_zero_data_des(uint8_t *data, int lens, int cnt_set)
{
	uint8_t ret = 0;
	int i = 0;
	int cnt = 0;

	for (i = 0; i < cnt_set; i++) {
		if ((i + 1) >= lens)
			return ret;
		if (data[i] == 0 || data[i] == 0x80)
			cnt++;
	}
	if (cnt == cnt_set)
		ret = 1;

	return ret;
}

static int aw_rtp_f0_cali(struct aw_haptic *aw_haptic, uint8_t *src_data,
						 int file_lens, int rtp_cali_len)
{
#ifdef AW_OVERRIDE_EN
	int zero_cnt = 0;
	int max_val = -1000;
	int two_cycle_stop_pos = 0;
	int last_pos = 0;
	int div = 1000;
	int l = 0;
#endif
	int temp_cnt = 0;
	int ret = 0;
	int i = 0;
	int j = 0;
	int k = 0;
	int index = 0;
	int *start_pos = NULL;
	int *stop_pos = NULL;
	int vib_judg_cnt = 0;
	int zero_judg_cnt = AW_CONTINUOUS_ZERO_NUM;
	int next_start_pos = 0;
	int temp_len = 0;
	int zero_len = 0;
	int brk_wave_len = 0;
	int8_t data_value = 0;
	int8_t *seg_src_data = NULL;
	int cnt = 0;
	uint32_t left = 0;
	uint32_t right = 0;
	uint32_t seg_src_len = 0;
	uint32_t seg_dst_len = 0;
	uint32_t src_f0 = aw_haptic->info.f0_pre;
	uint32_t dst_f0 = aw_haptic->f0;
#ifdef AW_ADD_BRAKE_WAVE
	bool en_replace = true;
#endif

	if (dst_f0 == src_f0) {
		aw_info("No calibration required!");
		return -EINVAL;
	}
	if (dst_f0 == 0) {
		aw_err("dst_f0 invalid %d", dst_f0);
		return -EINVAL;
	}
	if (src_f0 == 0) {
		aw_err("src_f0 invalid %d", src_f0);
		return -EINVAL;
	}
	start_pos = vmalloc(1000 * sizeof(int));
	stop_pos = vmalloc(1000 * sizeof(int));

	for (i = 0; i < file_lens; i++) {
		if ((i + 1) >= file_lens)
			break;

		if (src_data[i + 1] == 0 || src_data[i + 1] == 0x80)
			continue;

		ret = aw_rtp_vib_data_des(&src_data[i + 1], file_lens, vib_judg_cnt);
		if (!ret)
			continue;

		if (index >= 1000) {
			aw_err("index overflow : %d ", index);
			vfree(start_pos);
			vfree(stop_pos);
			return -EFBIG;
		}
		start_pos[index] = i;
		brk_wave_len = 0;
		for (j = i + 1; j < file_lens; j++) {
			if ((j + 1) >= file_lens) {
				stop_pos[index] = file_lens - 1;
				break;
			}
			if (src_data[j] == 0 || src_data[j] == 0x80) {
				if (j >= file_lens - zero_judg_cnt)
					continue;

				ret = aw_rtp_zero_data_des(&src_data[j + 1],
						file_lens, zero_judg_cnt);
#ifdef AW_ADD_BRAKE_WAVE
				if (ret == 1 && en_replace) {
					aw_info("index : %d ", index);
					en_replace = false;
					memset(&src_data[j], 0x80, sizeof(uint8_t) * zero_judg_cnt);
				}
#endif
				if (!ret)
					continue;

				ret = 0;
				for (k = j + 1; k < file_lens; k++) {
					if (src_data[k] == 0)
						continue;
					if (src_data[k] == 0x80) {
						brk_wave_len++;
						continue;
					}

					ret = aw_rtp_vib_data_des(&src_data[k],
						file_lens, vib_judg_cnt);
					if (ret) {
						next_start_pos = k;
						break;
					}

				}
				if (!ret) {
					next_start_pos = j;
					stop_pos[index] = next_start_pos;
					break;
				}

				seg_src_len = j - start_pos[index] + 1 + brk_wave_len;
				seg_dst_len = seg_src_len * src_f0 / dst_f0;
				if (seg_dst_len >= (next_start_pos - start_pos[index] + 1)) {
					j = next_start_pos;
				} else {
					stop_pos[index] = j;
					break;
				}
			}
		}

		if (index >= 1)
			zero_len = start_pos[index] - (start_pos[index - 1] + temp_len);
		else
			zero_len = start_pos[index];

		for (j = 0; j < zero_len; j++)
			aw_haptic->aw_rtp->data[cnt++] = 0;
		if (stop_pos[index] == file_lens - 1)
			brk_wave_len = 0;
		seg_src_len = stop_pos[index] - start_pos[index] + 1 + brk_wave_len;
		seg_src_data = &src_data[i];
		aw_info("seg_src_len = %d, src_f0 = %d, dst_f0 = %d", seg_src_len, src_f0, dst_f0);
		seg_dst_len = (seg_src_len * src_f0) / dst_f0;
		temp_cnt = cnt;
		temp_len = (dst_f0 / src_f0) >= 1 ? seg_src_len : seg_dst_len;
		aw_info("seg_src_len = %d, seg_dst_len = %d, temp_len = %d", seg_src_len, seg_dst_len, temp_len);
		aw_info("brk_wave_len = %d", brk_wave_len);
		for (j = 0; j < seg_dst_len; j++) {
			left = (uint32_t)(j * dst_f0 / src_f0);
			right = (uint32_t)(j * dst_f0 / src_f0 + 1.0);
			data_value = ((seg_src_data[right] - seg_src_data[left]) *
			((j * dst_f0 / src_f0) - left)) + seg_src_data[left];
			aw_haptic->aw_rtp->data[cnt++] = data_value;
		}
		for (j = 0; j < temp_len - seg_dst_len; j++)
			aw_haptic->aw_rtp->data[cnt++] = 0;
#ifdef AW_OVERRIDE_EN
		zero_cnt = 0;
		max_val = -1000;
		last_pos = 0;
		aw_info("temp_len - brk_wave_len = %d", temp_len - brk_wave_len);
		if ((temp_len - brk_wave_len) >= AW_LONG_VIB_POINTS) {
			for (l = 5; l < temp_len; l++) {
				if (abs((int8_t)aw_haptic->aw_rtp->data[l + temp_cnt]) >= max_val)
					max_val = abs((int8_t)aw_haptic->aw_rtp->data[l + temp_cnt]);

				if ((int8_t)aw_haptic->aw_rtp->data[l + temp_cnt] *
					(int8_t)aw_haptic->aw_rtp->data[l + temp_cnt + 1] <= 0) {
					if ((l - last_pos) >= 5) {
						zero_cnt++;
						last_pos = l;
					}
					if (zero_cnt == 3) {
						two_cycle_stop_pos = l;
						break;
					}
				}
			}
			div = 127000 / max_val;
			div = div > 2000 ? 2000 : div;
			for (l = 0; l < two_cycle_stop_pos; l++) {
				aw_haptic->aw_rtp->data[l + temp_cnt] =
					(int8_t)aw_haptic->aw_rtp->data[l + temp_cnt] * div / 1000;
			}
		}
#endif
		index++;
		i += temp_len;
#ifdef AW_ADD_BRAKE_WAVE
		en_replace = true;
#endif
	}

	if (index == 0) {
		vfree(start_pos);
		vfree(stop_pos);
		aw_err("No vibration data identified!");
		return -EINVAL;
	}

	zero_len = (file_lens - stop_pos[index - 1] - brk_wave_len - 1) - (temp_len - seg_src_len);
	for (j = 0; j < zero_len; j++) {
		if (cnt >= rtp_cali_len) {
			cnt = rtp_cali_len;
			break;
		}
		aw_haptic->aw_rtp->data[cnt++] = 0;
	}

	aw_haptic->aw_rtp->len = cnt;
	vfree(start_pos);
	vfree(stop_pos);

	aw_info("RTP F0 cali succeed. cnt = %d", cnt);

	return 0;
}
/************ RTP F0 Cali End ****************/
#endif

static void rtp_work_routine(struct work_struct *work)
{
	int ret = -1;
#ifdef AW8693XS_RTP_F0_CALI
	int rtp_cali_len = 0;
#endif
	const struct firmware *rtp_file;
	struct aw_haptic *aw_haptic = container_of(work, struct aw_haptic, rtp_work);

	mutex_lock(&aw_haptic->lock);

	if ((aw_haptic->index != aw_haptic->info.effect_max)){
		aw_info("index out of range!index = %d state=%d activate_mode = %d\n",
				aw_haptic->index, aw_haptic->state, aw_haptic->activate_mode);
		rb_force_exit();
		atomic_set(&aw_haptic->exit_in_rtp_loop, 0);
		wake_up_interruptible(&aw_haptic->stop_wait_q);
		mutex_unlock(&aw_haptic->lock);
		return;
	}

	aw_info("%s: index = %d state=%d activate_mode = %d\n", __func__,
		aw_haptic->index, aw_haptic->state, aw_haptic->activate_mode);

	if (!aw_haptic->ram.base_addr) {
		aw_err("base addr is 0, not allow rtp play");
		return;
	}
	aw_haptic->func->upload_lra(aw_haptic, AW_OSC_CALI_LRA);
	aw_haptic->func->set_rtp_aei(aw_haptic, false);
	aw_haptic->func->irq_clear(aw_haptic);
	/* wait for irq to exit */
	atomic_set(&aw_haptic->exit_in_rtp_loop, 1);
	while (atomic_read(&aw_haptic->is_in_rtp_loop)) {
		aw_info("goint to waiting irq exit\n");
		mutex_unlock(&aw_haptic->lock);
		ret = wait_event_interruptible(aw_haptic->wait_q,
				atomic_read(&aw_haptic->is_in_rtp_loop) == 0);
		aw_info("wakeup\n");
		mutex_lock(&aw_haptic->lock);
		if (ret == -ERESTARTSYS) {
			atomic_set(&aw_haptic->exit_in_rtp_loop, 0);
			wake_up_interruptible(&aw_haptic->stop_wait_q);
			mutex_unlock(&aw_haptic->lock);
			aw_err("wake up by signal return erro\n");
			return;
		}
	}

	atomic_set(&aw_haptic->exit_in_rtp_loop, 0);
	wake_up_interruptible(&aw_haptic->stop_wait_q);

	/* how to force exit this call */
	if (aw_haptic->is_custom_wave == 1 && aw_haptic->state) {
		aw_err("buffer size %d, availbe size %d\n",
				aw_haptic->ram.base_addr >> 2, get_rb_avalible_size());
		while (get_rb_avalible_size() < aw_haptic->ram.base_addr &&
		       !rb_shoule_exit()) {
			mutex_unlock(&aw_haptic->lock);
			ret = wait_event_interruptible(aw_haptic->stop_wait_q,
							(get_rb_avalible_size() >= aw_haptic->ram.base_addr) ||
							rb_shoule_exit());
			aw_info("wakeup\n");
			aw_err("after wakeup sbuffer size %d, availbe size %d\n",
					aw_haptic->ram.base_addr >> 2, get_rb_avalible_size());
			if (ret == -ERESTARTSYS) {
				aw_err("wake up by signal return erro\n");
				return;
			}
			mutex_lock(&aw_haptic->lock);
		}
	}

	aw_haptic->func->play_stop(aw_haptic);
	if (aw_haptic->state && aw_haptic->activate_mode == AW_RTP_MODE) {
		pm_stay_awake(aw_haptic->dev);
		/* gain */
		aw_haptic_effect_strength(aw_haptic);
		ram_vbat_comp(aw_haptic, false, aw_haptic->gain);
		aw_haptic->rtp_init = false;
		if (aw_haptic->is_custom_wave == 0) {
			aw_haptic->rtp_file_num = aw_haptic->index -
					aw_haptic->ram.ram_num;
			aw_info("aw_haptic->rtp_file_num =%d\n",aw_haptic->rtp_file_num);
			if (aw_haptic->rtp_file_num < 0)
				aw_haptic->rtp_file_num = 0;
			if (aw_haptic->rtp_file_num > (awinic_rtp_name_len - 1))
				aw_haptic->rtp_file_num = awinic_rtp_name_len - 1;
			aw_haptic->rtp_routine_on = 1;
			/* fw loaded */
			ret = request_firmware(&rtp_file,
					aw_rtp_name[aw_haptic->rtp_file_num],
					aw_haptic->dev);
			if (ret < 0) {
				aw_err("failed to read %s\n",aw_rtp_name[aw_haptic->rtp_file_num]);
				aw_haptic->rtp_routine_on = 0;
				pm_relax(aw_haptic->dev);
				mutex_unlock(&aw_haptic->lock);
				return;
			}
			vfree(aw_haptic->aw_rtp);
#if defined AW8693XS_RTP_F0_CALI
			if (aw_haptic->f0 == 0 || aw_haptic->info.f0_pre == 0) {
				aw_err("f0 or f0_pre invalid!");
				release_firmware(rtp_file);
				pm_relax(aw_haptic->dev);
				mutex_unlock(&aw_haptic->lock);
				return;
			}
			rtp_cali_len = rtp_file->size;
			if (aw_haptic->f0 < aw_haptic->info.f0_pre)
				rtp_cali_len = rtp_file->size * aw_haptic->info.f0_pre / aw_haptic->f0;
			aw_haptic->aw_rtp = vmalloc(rtp_cali_len + sizeof(int));
			if (!aw_haptic->aw_rtp) {
				release_firmware(rtp_file);
				aw_err("error allocating memory, f0:%d f0_cali=%d", aw_haptic->f0, aw_haptic->f0_cali_data);
				aw_haptic->rtp_routine_on = 0;
				pm_relax(aw_haptic->dev);
				mutex_unlock(&aw_haptic->lock);
				return;
			}
			aw_haptic->aw_rtp->len = rtp_cali_len;
			aw_info("rtp file:[%s] size = %zu bytes, rtp_cali_len = %d",
				aw_rtp_name[aw_haptic->rtp_file_num],
				rtp_file->size, rtp_cali_len);
			memcpy(aw_haptic->aw_rtp->data, rtp_file->data,
			       rtp_file->size);
			ret = aw_rtp_f0_cali(aw_haptic, (uint8_t *)rtp_file->data,
								rtp_file->size, rtp_cali_len);
			if (ret) {
				aw_err("Play uncalibrated data! f0:%d f0_cali=%d", aw_haptic->f0, aw_haptic->f0_cali_data);
				aw_haptic->aw_rtp->len = rtp_file->size;
			}
#else
			aw_haptic->aw_rtp = vmalloc(rtp_file->size + sizeof(int));
			if (!aw_haptic->aw_rtp) {
				release_firmware(rtp_file);
				aw_err("error allocating memory\n");
				aw_haptic->rtp_routine_on = 0;
				pm_relax(aw_haptic->dev);
				mutex_unlock(&aw_haptic->lock);
				return;
			}
			aw_haptic->aw_rtp->len = rtp_file->size;
			aw_info("rtp file:[%s] size = %dbytes\n",
				aw_rtp_name[aw_haptic->rtp_file_num],aw_haptic->aw_rtp->len);
			memcpy(aw_haptic->aw_rtp->data, rtp_file->data,
			       rtp_file->size);
#endif
			release_firmware(rtp_file);
		} else {
			vfree(aw_haptic->aw_rtp);
			aw_haptic->aw_rtp = NULL;
			if(aw_haptic->ram.base_addr != 0) {
				aw_haptic->aw_rtp = vmalloc((aw_haptic->ram.base_addr >> 2) + sizeof(int));
			} else {
				aw_err("ram update not done yet, return !");
			}
			if (!aw_haptic->aw_rtp) {
				aw_err("%s: error allocating memory\n",
				       __func__);
				pm_relax(aw_haptic->dev);
				mutex_unlock(&aw_haptic->lock);
				return;
			}
		}
		aw_haptic->rtp_init = true;
		/* rtp mode config */
		aw_haptic->func->play_mode(aw_haptic, AW_RTP_MODE);
		/* haptic go */
		aw_haptic->func->play_go(aw_haptic, true);
		usleep_range(2000, 2500);
		ret = wait_enter_rtp_mode(aw_haptic);
		rtp_play(aw_haptic);
		aw_haptic->rtp_routine_on = 0;
		pm_relax(aw_haptic->dev);
	} else {
		aw_haptic->rtp_cnt = 0;
		aw_haptic->rtp_init = false;
		pm_relax(aw_haptic->dev);
	}
	mutex_unlock(&aw_haptic->lock);
}

#ifdef AAC_RICHTAP_SUPPORT
static void tiktap_enable_mix_play(struct aw_haptic *aw_haptic, bool en_flag)
{
	if(en_flag){
		right->i2c->addr = right->record_i2c_addr;
		aw_haptic->func->set_pwm(right, AW_PWM_12K);
		aw_haptic->func->set_pwm(left, AW_PWM_12K);
	}
	aw8693x_broadcast_config(aw_haptic, en_flag, AW_RTP_MODE);
}

static void richtap_clean_buf(struct aw_haptic *aw_haptic, int status)
{
   struct mmap_buf_format *opbuf = aw_haptic->start_buf;
    int i = 0;

    for(i = 0; i < RICHTAP_MMAP_BUF_SUM; i++)
    {
	memset(opbuf->data, 0, RICHTAP_MMAP_BUF_SIZE);
	opbuf->status = status;
        opbuf = opbuf->kernel_next;
    }
}

static void richtap_update_fifo_data(struct aw_haptic *aw_haptic, uint32_t fifo_len)
{
	int32_t samples_left = 0, pos = 0, retry = 3;

	do {
		if (aw_haptic->curr_buf->status == MMAP_BUF_DATA_VALID) {
			samples_left = aw_haptic->curr_buf->length - aw_haptic->pos;
			//aw_info("length : %d, aw_haptic->pos: %d, fifo_len: %d, pos : %d", aw_haptic->curr_buf->length, aw_haptic->pos, fifo_len, pos);
			if (samples_left < 0) {
				aw_err("Invalid samples_left %d", samples_left);
				break;
			}
			if (samples_left < fifo_len) {
				memcpy(&aw_haptic->rtp_ptr[pos], &aw_haptic->curr_buf->data[aw_haptic->pos], samples_left);
				pos += samples_left;
				fifo_len -= samples_left;
				aw_haptic->curr_buf->status = MMAP_BUF_DATA_INVALID;
				aw_haptic->curr_buf->length = 0;
				aw_haptic->curr_buf = aw_haptic->curr_buf->kernel_next;
				aw_haptic->pos = 0;
			} else {
				memcpy(&aw_haptic->rtp_ptr[pos], &aw_haptic->curr_buf->data[aw_haptic->pos], fifo_len);
				aw_haptic->pos += fifo_len;
				pos += fifo_len;
				fifo_len = 0;
			}
		} else if (aw_haptic->curr_buf->status == MMAP_BUF_DATA_FINISHED) {
			break;
		} else {
			if (retry-- <= 0) {
				pr_info("invalid data\n");
				break;
			}
			usleep_range(1000, 1000);
		}
	} while (fifo_len > 0 && atomic_read(&aw_haptic->richtap_rtp_mode));

	pr_debug("update fifo len %d\n", pos);
	aw_haptic->func->set_rtp_data(aw_haptic, aw_haptic->rtp_ptr, pos);
}

static bool richtap_rtp_start(struct aw_haptic *aw_haptic)
{
    int cnt = 200;
    bool rtp_work_flag = false;
    uint8_t reg_val = 0;

    mutex_lock(&aw_haptic->lock);
//     if (aw_haptic->tiktap_mix_flag) {
// 	tiktap_enable_mix_play(aw_haptic, true);
// 	disable_irq(gpio_to_irq(left->irq_gpio));
//     } else {
// 	enable_irq(gpio_to_irq(left->irq_gpio));
//     }

    aw_haptic->func->play_mode(aw_haptic, AW_RTP_MODE);
    aw_haptic->func->play_go(aw_haptic, true);
    usleep_range(2000, 2000);

   while (cnt) {
        reg_val = aw_haptic->func->get_glb_state(aw_haptic);
       if ((reg_val & AW_GLBRD_STATE_MASK) == AW_STATE_RTP)
      {
            cnt = 0;
           rtp_work_flag = true;
            aw_info("RTP_GO! glb_state=0x08\n");
        }
        else
        {
            cnt--;
            pr_debug("%s: wait for RTP_GO, glb_state=0x%02X\n", __func__, reg_val);
        }
        usleep_range(2000, 2500);
    }

    if(rtp_work_flag == false)
    {
	aw_err("no RTP_GO! glb_state=0x08\n");
        aw_haptic->func->play_stop(aw_haptic);
    }

    mutex_unlock(&aw_haptic->lock);

    return rtp_work_flag;
}

static void richtap_rtp_work(struct work_struct *work)
{
	struct aw_haptic *aw_haptic = container_of(work, struct aw_haptic, richtap_rtp_work);
	uint32_t retry = 0, tmp_len = 0;
	uint8_t glb_state_val = 0;

	atomic_set(&aw_haptic->tiktap_dual_rtp, false);
   	aw_haptic->curr_buf = aw_haptic->start_buf;

	do {
		if(aw_haptic->curr_buf->status == MMAP_BUF_DATA_VALID) {
			if((tmp_len + aw_haptic->curr_buf->length) > aw_haptic->ram.base_addr) {
				memcpy(&aw_haptic->rtp_ptr[tmp_len], aw_haptic->curr_buf->data, (aw_haptic->ram.base_addr - tmp_len));
				aw_haptic->pos = aw_haptic->ram.base_addr - tmp_len;
				tmp_len = aw_haptic->ram.base_addr;
			} else {
				memcpy(&aw_haptic->rtp_ptr[tmp_len], aw_haptic->curr_buf->data, aw_haptic->curr_buf->length);
				tmp_len += aw_haptic->curr_buf->length;
				aw_haptic->curr_buf->status = MMAP_BUF_DATA_INVALID;
				aw_haptic->curr_buf->length = 0;
				aw_haptic->pos = 0;
				aw_haptic->curr_buf = aw_haptic->curr_buf->kernel_next;
			}
		} else if(aw_haptic->curr_buf->status == MMAP_BUF_DATA_FINISHED) {
			break;
		} else {
			msleep(1);
		}
	} while(tmp_len < aw_haptic->ram.base_addr && retry++ < 30);

	//aw_info("rtp tm_len = %d, retry = %d, aw_haptic->ram.base_addr = %d\n", tmp_len, retry, aw_haptic->ram.base_addr);
	rb_force_exit();
	wake_up_interruptible(&aw_haptic->stop_wait_q);

	if(richtap_rtp_start(aw_haptic)) {
		aw_haptic->func->set_rtp_data(aw_haptic, aw_haptic->rtp_ptr, tmp_len);
		if (aw_haptic->tiktap_mix_flag) {
			atomic_set(&aw_haptic->richtap_rtp_mode, true);
			while ((!aw_haptic->func->rtp_get_fifo_afs(aw_haptic))
			&& (aw_haptic->play_mode == AW_RTP_MODE) && (aw_haptic->curr_buf->status == MMAP_BUF_DATA_VALID)) {
				richtap_update_fifo_data(aw_haptic, (aw_haptic->ram.base_addr >> 2));
				glb_state_val = aw_haptic->func->get_glb_state(aw_haptic);
				if ((glb_state_val & AW_GLBRD_STATE_MASK) == AW_STATE_STANDBY)
					break;
			}
			aw_haptic->func->irq_clear(aw_haptic);
			aw_haptic->func->set_rtp_aei(aw_haptic, true);
		} else {
			aw_haptic->func->set_rtp_aei(aw_haptic, true);
			aw_haptic->func->irq_clear(aw_haptic);
			atomic_set(&aw_haptic->richtap_rtp_mode, true);
		}
    }
}

#ifdef AW_DOUBLE
static int haptic_left_flag(int unuse)
{
	(void)unuse;
	return LEFT_FOPS;
}
static int haptic_right_flag(int unuse)
{
	(void)unuse;
	return RIGHT_FOPS;
}
#endif

static int richtap_file_open(struct inode *inode, struct file *file)
{
	if (!try_module_get(THIS_MODULE))
		return -ENODEV;

#ifdef AW_DOUBLE
	file->private_data = (void *)left;
#else
	file->private_data = (void *)g_aw_haptic;
#endif
	return 0;
}

#ifdef AW_DOUBLE
static int richtap_file_open_r(struct inode *inode, struct file *file)
{
	if (!try_module_get(THIS_MODULE))
		return -ENODEV;

	file->private_data = (void *)right;

	return 0;
}
#endif

static int richtap_file_release(struct inode *inode, struct file *file)
{
    module_put(THIS_MODULE);

	return 0;
}

static long richtap_file_unlocked_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct aw_haptic *aw_haptic = (struct aw_haptic *)filp->private_data;
	int ret = 0, tmp;

	pr_info("%s: cmd=0x%x, arg=0x%lx\n", __func__, cmd, arg);

	switch(cmd)
	{
	    case RICHTAP_GET_HWINFO:
            tmp = RICHTAP_AW_8697;
            if(copy_to_user((void __user *)arg, &tmp, sizeof(int)))
                ret = -EFAULT;
            break;
	    case RICHTAP_RTP_MODE:
            mutex_lock(&aw_haptic->lock);
            aw_haptic->func->play_stop(aw_haptic);
            mutex_unlock(&aw_haptic->lock);
            if(copy_from_user(aw_haptic->rtp_ptr, (void __user *)arg, RICHTAP_MMAP_BUF_SIZE * RICHTAP_MMAP_BUF_SUM))
           {
                ret = -EFAULT;
                break;
            }
            tmp = *((int*)aw_haptic->rtp_ptr);
            if(tmp > (RICHTAP_MMAP_BUF_SIZE * RICHTAP_MMAP_BUF_SUM - 4))
            {
                dev_err(aw_haptic->dev, "rtp mode date len error %d\n", tmp);
                ret = -EINVAL;
                break;
            }
            aw_haptic->func->upload_lra(aw_haptic, AW_OSC_CALI_LRA);
            aw_haptic->func->set_bst_vol(aw_haptic, aw_haptic->vmax);
            if(richtap_rtp_start(aw_haptic))
            {
                aw_haptic->func->set_rtp_data(aw_haptic, &aw_haptic->rtp_ptr[4], tmp);
            }
            break;
	    case RICHTAP_OFF_MODE:
		    break;
	    case RICHTAP_GET_F0:
		    tmp = aw_haptic->f0;
		    if(copy_to_user((void __user *)arg, &tmp, sizeof(uint32_t)))
			    ret = -EFAULT;
		    break;
	    case RICHTAP_SETTING_GAIN:
		    if(arg > 0x80)
			    arg = 0x80;
          	aw_haptic->func->set_gain(aw_haptic, (uint8_t)arg);
		    break;
		case RICHTAP_STREAM_MODE:
			aw_info("RICHTAP_STREAM_MODE");
			richtap_clean_buf(aw_haptic, MMAP_BUF_DATA_INVALID);
			mutex_lock(&aw_haptic->lock);
			aw_haptic->func->irq_clear(aw_haptic);
			aw_haptic->func->play_stop(aw_haptic);
			tiktap_enable_mix_play(aw_haptic, false);
			right->tiktap_mix_flag = false;
			mutex_unlock(&aw_haptic->lock);
			aw_haptic->func->set_rtp_aei(aw_haptic, false);
			atomic_set(&aw_haptic->richtap_rtp_mode, false);
			aw_haptic->func->upload_lra(aw_haptic, AW_OSC_CALI_LRA);
			aw_haptic->func->set_bst_vol(aw_haptic, aw_haptic->vmax);
			queue_work(aw_haptic->work_queue, &aw_haptic->richtap_rtp_work);
			break;
		case TIKTAP_RTP_MIX_MODE:
			aw_info("TIKTAP_RTP_MIX_MODE");
			atomic_set(&aw_haptic->tiktap_dual_rtp, true);
			richtap_clean_buf(aw_haptic, MMAP_BUF_DATA_INVALID);
			mutex_lock(&aw_haptic->lock);
			aw_haptic->func->irq_clear(aw_haptic);
			aw_haptic->func->play_stop(aw_haptic);
			aw_haptic->tiktap_mix_flag = true;
			tiktap_enable_mix_play(aw_haptic, false);
			tiktap_enable_mix_play(aw_haptic, true);
			mutex_unlock(&aw_haptic->lock);
			aw_haptic->func->set_rtp_aei(aw_haptic, false);
			atomic_set(&aw_haptic->richtap_rtp_mode, false);
			aw_haptic->func->upload_lra(aw_haptic, AW_OSC_CALI_LRA);
			aw_haptic->func->set_bst_vol(aw_haptic, aw_haptic->vmax);
			queue_work(aw_haptic->work_queue, &aw_haptic->richtap_rtp_work);
			break;
		case RICHTAP_STOP_MODE:
			aw_info("RICHTAP_STOP_MODE");
			mutex_lock(&aw_haptic->lock);
			aw_haptic->func->irq_clear(aw_haptic);
			aw_haptic->func->play_stop(aw_haptic);
			if (aw_haptic->tiktap_mix_flag && (atomic_read(&aw_haptic->tiktap_dual_rtp) == false)) {
				tiktap_enable_mix_play(aw_haptic, false);
			}
			mutex_unlock(&aw_haptic->lock);
			aw_haptic->func->set_rtp_aei(aw_haptic, false);
			atomic_set(&aw_haptic->richtap_rtp_mode, false);
			break;
	    default:
		    dev_err(aw_haptic->dev, "%s, unknown cmd\n", __func__);
		    break;
	}

	return ret;
}

static int richtap_file_mmap(struct file *filp, struct vm_area_struct *vma)
{
	unsigned long phys;
	struct aw_haptic *aw_haptic = (struct aw_haptic *)filp->private_data;
	int ret = 0;

#if LINUX_VERSION_CODE > KERNEL_VERSION(4,7,0)
	//only accept PROT_READ, PROT_WRITE and MAP_SHARED from the API of mmap
	vm_flags_t vm_flags = calc_vm_prot_bits(PROT_READ|PROT_WRITE, 0) | calc_vm_flag_bits(MAP_SHARED);
	vm_flags |= current->mm->def_flags | VM_MAYREAD | VM_MAYWRITE | VM_MAYEXEC| VM_SHARED | VM_MAYSHARE;
	if(vma && (pgprot_val(vma->vm_page_prot) != pgprot_val(vm_get_page_prot(vm_flags))))
	    return -EPERM;

	if(vma && ((vma->vm_end - vma->vm_start) != (PAGE_SIZE << RICHTAP_MMAP_PAGE_ORDER)))
	    return -ENOMEM;
#endif
	phys = virt_to_phys(aw_haptic->start_buf);

	ret = remap_pfn_range(vma, vma->vm_start, (phys >> PAGE_SHIFT), (vma->vm_end - vma->vm_start), vma->vm_page_prot);
	if(ret)
	{
	    dev_err(aw_haptic->dev, "Error mmap failed\n");
	    return ret;
	}

	return ret;
}

static ssize_t richtap_read(struct file *file,
		char __user *user_buf, size_t count, loff_t *ppos)
{
	return 0;
}

static ssize_t richtap_write(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	return count;
}
static struct file_operations left_fops = {
	.owner = THIS_MODULE,
	.read = richtap_read,
	.write = richtap_write,
	.mmap = richtap_file_mmap,
	.unlocked_ioctl = richtap_file_unlocked_ioctl,
	.open = richtap_file_open,
	.release = richtap_file_release,
#ifdef AW_DOUBLE
	.check_flags = haptic_left_flag,
#endif
};

#ifdef AW_DOUBLE
static struct file_operations right_fops = {
	.owner = THIS_MODULE,
	.read = richtap_read,
	.write = richtap_write,
	.mmap = richtap_file_mmap,
	.unlocked_ioctl = richtap_file_unlocked_ioctl,
	.open = richtap_file_open_r,
	.release = richtap_file_release,
	.check_flags = haptic_right_flag,
};
#endif

static struct miscdevice richtap_misc = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "awinic_haptic",
    .fops = &left_fops,
};
#ifdef AW_DOUBLE
static struct miscdevice richtap_misc_x = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "awinic_x_haptic",
    .fops = &right_fops,
};
#endif
#endif
static irqreturn_t irq_handle(int irq, void *data)
{
	int irq_state = 0;
	struct aw_haptic *aw_haptic = data;
	unsigned char glb_state_val = 0;
	unsigned int buf_len = 0;
	unsigned int period_size = aw_haptic->ram.base_addr >> 2;
	struct aw_haptic_container *aw_rtp = aw_haptic->aw_rtp;
	aw_info("enter");

#ifdef AAC_RICHTAP_SUPPORT
	aw_info("enter");
	if(atomic_read(&aw_haptic->richtap_rtp_mode) == true) {
		irq_state = aw_haptic->func->get_irq_state(aw_haptic);
		if(irq_state == AW_IRQ_ALMOST_EMPTY) {
			aw_info("%s: aw_haptic rtp fifo almost empty\n", __func__);
			//richtap_update_fifo_data(aw_haptic, (aw_haptic->ram.base_addr - (aw_haptic->ram.base_addr >> 2)));
			richtap_update_fifo_data(aw_haptic, (aw_haptic->ram.base_addr >> 1));
			while(!aw_haptic->func->rtp_get_fifo_afi(aw_haptic) && atomic_read(&aw_haptic->richtap_rtp_mode) && (aw_haptic->curr_buf->status == MMAP_BUF_DATA_VALID)) {
				richtap_update_fifo_data(aw_haptic, (aw_haptic->ram.base_addr >> 2));
			}
		}

		if(aw_haptic->curr_buf->status == MMAP_BUF_DATA_INVALID) {
			aw_info("entry MMAP_BUF_DATA_INVALID!");
			aw_haptic->func->set_rtp_aei(aw_haptic, false);
			atomic_set(&aw_haptic->richtap_rtp_mode, false);
		}

		return IRQ_HANDLED;
	}
#endif

	atomic_set(&aw_haptic->is_in_rtp_loop, 1);
	irq_state = aw_haptic->func->get_irq_state(aw_haptic);
	if (!(irq_state & AW_IRQ_ALMOST_FULL) || !(irq_state & AW_IRQ_ALMOST_EMPTY))
		aw_haptic->rtp_routine_on = 0;
	if ((irq_state == AW_IRQ_ALMOST_EMPTY) && aw_haptic->rtp_init) {
		aw_info("irq_handle: aw86927 rtp fifo almost empty\n");
		while ((!aw_haptic->func->rtp_get_fifo_afs(aw_haptic)) &&
			(aw_haptic->play_mode == AW_RTP_MODE) &&
			!atomic_read(&aw_haptic->exit_in_rtp_loop)) {
			mutex_lock(&aw_haptic->rtp_lock);
			if (!aw_rtp) {
				aw_info("irq_handle:aw_rtp is null, break!\n");
				mutex_unlock(&aw_haptic->rtp_lock);
				break;
			}
			if (aw_haptic->is_custom_wave == 1) {
				buf_len = read_rb(aw_rtp->data,
						  period_size);
				aw_haptic->func->set_rtp_data(
					aw_haptic, aw_rtp->data, buf_len);
#ifdef A2HAPTIC_SUPPORT
				atomic_set(&aw_haptic->is_consume_data, 1);
				wake_up_interruptible(&aw_haptic->wait_q);
#endif
				if (buf_len < period_size) {
					aw_info("%s: rtp update complete\n",
						__func__);
					aw_haptic->func->set_rtp_aei(aw_haptic, false);
					aw_haptic->rtp_cnt = 0;
					aw_haptic->rtp_init = 0;
					pm_relax(aw_haptic->dev);
					mutex_unlock(&aw_haptic->rtp_lock);
					break;
				}
			} else {
				if ((aw_rtp->len - aw_haptic->rtp_cnt) <
				    period_size) {
					buf_len = aw_rtp->len -
							aw_haptic->rtp_cnt;
				} else {
					buf_len = period_size;
				}
				aw_haptic->func->set_rtp_data(aw_haptic,
					&aw_rtp->data[aw_haptic->rtp_cnt], buf_len);
				aw_haptic->rtp_cnt += buf_len;
				glb_state_val = aw_haptic->func->get_glb_state(aw_haptic);
				if ((aw_haptic->rtp_cnt == aw_rtp->len) || ((glb_state_val & AW_GLBRD_STATE_MASK) == AW_STATE_STANDBY)) {
					if (aw_haptic->rtp_cnt !=
					    aw_rtp->len)
						aw_err("irq_handle: rtp play suspend!\n");
					else
						aw_info("irq_handle: rtp update complete!\n");
					aw_haptic->rtp_routine_on = 0;
					aw_haptic->func->set_rtp_aei(aw_haptic, false);
					aw_haptic->rtp_cnt = 0;
					aw_haptic->rtp_init = 0;
					pm_relax(aw_haptic->dev);
					mutex_unlock(&aw_haptic->rtp_lock);
					break;
				}
			}
			pm_relax(aw_haptic->dev);
			mutex_unlock(&aw_haptic->rtp_lock);
		}
	}

	if (irq_state == AW_IRQ_ALMOST_FULL)
		aw_info("%s: aw_haptic rtp mode fifo almost full!\n", __func__);

	if (aw_haptic->play_mode != AW_RTP_MODE ||
	    atomic_read(&aw_haptic->exit_in_rtp_loop))
		aw_haptic->func->set_rtp_aei(aw_haptic, false);
	atomic_set(&aw_haptic->is_in_rtp_loop, 0);
	wake_up_interruptible(&aw_haptic->wait_q);
	aw_info("%s exit\n", __func__);

	return IRQ_HANDLED;
}

#ifndef AW_VIM3
static int irq_config(struct device *dev, struct aw_haptic *aw_haptic)
{
	int ret = -1;
	int irq_flags = 0;

	if (gpio_is_valid(aw_haptic->irq_gpio) &&
	    !(aw_haptic->flags & AW_FLAG_SKIP_INTERRUPTS)) {
		/* register irq handler */
		aw_haptic->func->interrupt_setup(aw_haptic);
		irq_flags = IRQF_TRIGGER_FALLING | IRQF_ONESHOT;
		ret = devm_request_threaded_irq(dev, gpio_to_irq(aw_haptic->irq_gpio), NULL,
						irq_handle, irq_flags, "aw_haptic", aw_haptic);
		if (ret != 0) {
			aw_err("failed to request IRQ %d: %d",
			       gpio_to_irq(aw_haptic->irq_gpio), ret);
			return ret;
		}
	} else {
		dev_info(dev, "skipping IRQ registration");
		/* disable feature support if gpio was invalid */
		aw_haptic->flags |= AW_FLAG_SKIP_INTERRUPTS;
	}

	return 0;
}
#endif

static void ram_load(const struct firmware *cont, void *context)
{
	uint16_t check_sum = 0;
	int i = 0;
	int ret = 0;
	struct aw_haptic *aw_haptic = context;
	struct aw_haptic_container *aw_fw;

#ifdef AW_READ_BIN_FLEXBALLY
	static uint8_t load_cont;
	int ram_timer_val = 1000;

	load_cont++;
#endif
	if (!cont) {
		aw_err("failed to read %s", aw_ram_name);
		release_firmware(cont);
#ifdef AW_READ_BIN_FLEXBALLY
		if (load_cont <= 20) {
			schedule_delayed_work(&aw_haptic->ram_work,
					      msecs_to_jiffies(ram_timer_val));
			aw_info("start hrtimer:load_cont%d", load_cont);
		}
#endif
		return;
	}
	aw_info("loaded %s - size: %zu", aw_ram_name, cont ? cont->size : 0);
	/* check sum */
	for (i = 2; i < cont->size; i++)
		check_sum += cont->data[i];
	if (check_sum != (uint16_t)((cont->data[0] << 8) | (cont->data[1]))) {
		aw_err("check sum err: check_sum=0x%04x", check_sum);
		release_firmware(cont);
		return;
	}
	aw_info("check sum pass : 0x%04x", check_sum);
	aw_haptic->ram.check_sum = check_sum;

	/* aw ram update */
	aw_fw = kzalloc(cont->size + sizeof(int), GFP_KERNEL);
	if (!aw_fw) {
		release_firmware(cont);
		aw_err("Error allocating memory");
		return;
	}
	aw_fw->len = cont->size;
	memcpy(aw_fw->data, cont->data, cont->size);
	release_firmware(cont);
	ret = aw_haptic->func->container_update(aw_haptic, aw_fw);
	if (ret) {
		aw_err("ram firmware update failed!");
	} else {
		aw_haptic->ram_init = true;
		aw_haptic->ram.len = aw_fw->len - aw_haptic->ram.ram_shift;
		mutex_lock(&aw_haptic->lock);
		aw_haptic->func->trig_init(aw_haptic);
		mutex_unlock(&aw_haptic->lock);
		aw_info("ram firmware update complete!");
		get_ram_num(aw_haptic);
	}
	kfree(aw_fw);

#ifdef AW_BOOT_OSC_CALI
	rtp_trim_lra_cali(aw_haptic);
#endif
}

static int ram_update(struct aw_haptic *aw_haptic)
{
	aw_haptic->ram_init = false;
	aw_haptic->rtp_init = false;
	return request_firmware_nowait(THIS_MODULE, 1, aw_ram_name, aw_haptic->dev, GFP_KERNEL,
				       aw_haptic, ram_load);
}

static void ram_work_routine(struct work_struct *work)
{
	struct aw_haptic *aw_haptic = container_of(work, struct aw_haptic, ram_work.work);

	ram_update(aw_haptic);
}

static void ram_work_init(struct aw_haptic *aw_haptic)
{
	int ram_timer_val = AW_RAM_WORK_DELAY_INTERVAL;

	INIT_DELAYED_WORK(&aw_haptic->ram_work, ram_work_routine);
	schedule_delayed_work(&aw_haptic->ram_work, msecs_to_jiffies(ram_timer_val));
}

/*****************************************************
 *
 * haptic audio
 *
 *****************************************************/
static int audio_ctrl_list_ins(struct aw_haptic *aw_haptic, struct aw_haptic_ctr *haptic_ctr)
{
	struct aw_haptic_ctr *p_new = NULL;
	struct aw_haptic_audio *haptic_audio = &aw_haptic->haptic_audio;

	p_new = kzalloc(sizeof(struct aw_haptic_ctr), GFP_KERNEL);
	if (p_new == NULL)
		return -ENOMEM;

	/* update new list info */
	p_new->cnt = haptic_ctr->cnt;
	p_new->cmd = haptic_ctr->cmd;
	p_new->play = haptic_ctr->play;
	p_new->wavseq = haptic_ctr->wavseq;
	p_new->loop = haptic_ctr->loop;
	p_new->gain = haptic_ctr->gain;
	INIT_LIST_HEAD(&(p_new->list));
	list_add(&(p_new->list), &(haptic_audio->ctr_list));

	return 0;
}

static void audio_ctrl_list_clr(struct aw_haptic_audio *haptic_audio)
{
	struct aw_haptic_ctr *p_ctr = NULL;
	struct aw_haptic_ctr *p_ctr_bak = NULL;

	list_for_each_entry_safe_reverse(p_ctr, p_ctr_bak, &(haptic_audio->ctr_list), list) {
		list_del(&p_ctr->list);
		kfree(p_ctr);
	}
}

static void audio_init(struct aw_haptic *aw_haptic)
{
	aw_dbg("enter!");
	aw_haptic->func->set_wav_seq(aw_haptic, 0x01, 0x00);
}

static void audio_off(struct aw_haptic *aw_haptic)
{
	aw_info("enter");
	mutex_lock(&aw_haptic->lock);
#ifdef AW_DOUBLE
	left->func->play_stop(left);
	right->func->play_stop(right);
	left->func->set_gain(left, 0x80);
	right->func->set_gain(right, 0x80);
#else
	aw_haptic->func->play_stop(aw_haptic);
	aw_haptic->func->set_gain(aw_haptic, 0x80);
#endif
	audio_ctrl_list_clr(&aw_haptic->haptic_audio);
	mutex_unlock(&aw_haptic->lock);
}

static enum hrtimer_restart audio_timer_func(struct hrtimer *timer)
{
	struct aw_haptic *aw_haptic =
	    container_of(timer, struct aw_haptic, haptic_audio.timer);

	aw_dbg("enter");
	queue_work(aw_haptic->work_queue, &aw_haptic->haptic_audio.work);

	hrtimer_start(&aw_haptic->haptic_audio.timer,
	ktime_set(aw_haptic->haptic_audio.timer_val / 1000000,
				(aw_haptic->haptic_audio.timer_val % 1000000) * 1000),
				HRTIMER_MODE_REL);

	return HRTIMER_NORESTART;
}

static void audio_work_routine(struct work_struct *work)
{
	struct aw_haptic *aw_haptic = container_of(work, struct aw_haptic, haptic_audio.work);
	struct aw_haptic_audio *haptic_audio = NULL;
	struct aw_haptic_ctr *ctr = &aw_haptic->haptic_audio.ctr;
	struct aw_haptic_ctr *p_ctr = NULL;
	struct aw_haptic_ctr *p_ctr_bak = NULL;
	uint32_t ctr_list_flag = 0;
	uint32_t ctr_list_input_cnt = 0;
	uint32_t ctr_list_output_cnt = 0;
	uint32_t ctr_list_diff_cnt = 0;
	uint32_t ctr_list_del_cnt = 0;
	int rtp_is_going_on = 0;

	aw_dbg("enter");
	haptic_audio = &(aw_haptic->haptic_audio);
	mutex_lock(&aw_haptic->haptic_audio.lock);
	memset(ctr, 0, sizeof(struct aw_haptic_ctr));
	ctr_list_flag = 0;
	list_for_each_entry_safe_reverse(p_ctr, p_ctr_bak, &(haptic_audio->ctr_list), list) {
		ctr_list_flag = 1;
		break;
	}
	if (ctr_list_flag == 0)
		aw_dbg("ctr list empty");
	if (ctr_list_flag == 1) {
		list_for_each_entry_safe(p_ctr, p_ctr_bak, &(haptic_audio->ctr_list), list) {
			ctr_list_input_cnt = p_ctr->cnt;
			break;
		}
		list_for_each_entry_safe_reverse(p_ctr, p_ctr_bak,
						 &(haptic_audio->ctr_list), list) {
			ctr_list_output_cnt = p_ctr->cnt;
			break;
		}
		if (ctr_list_input_cnt > ctr_list_output_cnt)
			ctr_list_diff_cnt = ctr_list_input_cnt - ctr_list_output_cnt;

		if (ctr_list_input_cnt < ctr_list_output_cnt)
			ctr_list_diff_cnt = 32 + ctr_list_input_cnt - ctr_list_output_cnt;

		if (ctr_list_diff_cnt > 2) {
			list_for_each_entry_safe_reverse(p_ctr, p_ctr_bak,
							 &(haptic_audio->ctr_list), list) {
				if ((p_ctr->play == 0) &&
				    (AW_CMD_ENABLE == (AW_CMD_HAPTIC & p_ctr->cmd))) {
					list_del(&p_ctr->list);
					kfree(p_ctr);
					ctr_list_del_cnt++;
				}
				if (ctr_list_del_cnt == ctr_list_diff_cnt)
					break;
			}
		}
	}
	/* get the last data from list */
	list_for_each_entry_safe_reverse(p_ctr, p_ctr_bak, &(haptic_audio->ctr_list), list) {
		ctr->cnt = p_ctr->cnt;
		ctr->cmd = p_ctr->cmd;
		ctr->play = p_ctr->play;
		ctr->wavseq = p_ctr->wavseq;
		ctr->loop = p_ctr->loop;
		ctr->gain = p_ctr->gain;
		list_del(&p_ctr->list);
		kfree(p_ctr);
		break;
	}
	if (ctr->play) {
		aw_info("cnt=%d, cmd=%d, play=%d, wavseq=%d, loop=%d, gain=%d",
			ctr->cnt,  ctr->cmd, ctr->play, ctr->wavseq, ctr->loop, ctr->gain);
	}
	if (ctr->wavseq > aw_haptic->ram.ram_num) {
		aw_err("wavseq out of range!");
		mutex_unlock(&aw_haptic->haptic_audio.lock);
		return;
	}
	rtp_is_going_on = aw_haptic->func->judge_rtp_going(aw_haptic);
	if (rtp_is_going_on) {
		mutex_unlock(&aw_haptic->haptic_audio.lock);
		return;
	}
	mutex_unlock(&aw_haptic->haptic_audio.lock);

#ifdef AW_DOUBLE
	if (ctr->play == AW_PLAY_ENABLE) {
		mutex_lock(&aw_haptic->lock);
		/* haptic config */
		if (ctr->cmd & AW_CMD_L_EN) {
			left->func->play_stop(left);
			left->func->play_mode(left, AW_RAM_MODE);
			left->func->set_wav_seq(left, 0x00, ctr->wavseq);
			left->func->set_wav_seq(left, 0x01, 0x00);
			left->func->set_wav_loop(left, 0x00, ctr->loop);
			left->func->set_gain(left, ctr->gain);
		}

		if (ctr->cmd & AW_CMD_R_EN) {
			right->func->play_stop(right);
			right->func->play_mode(right, AW_RAM_MODE);
			right->func->set_wav_seq(right, 0x00, ctr->wavseq);
			right->func->set_wav_seq(right, 0x01, 0x00);
			right->func->set_wav_loop(right, 0x00, ctr->loop);
			right->func->set_gain(right, ctr->gain);
		}
		/* play go */
		if (ctr->cmd & AW_CMD_L_EN)
			left->func->play_go(left, true);
		if (ctr->cmd & AW_CMD_R_EN)
			right->func->play_go(right, true);
		mutex_unlock(&aw_haptic->lock);
	} else if (ctr->play == AW_PLAY_STOP) {
		mutex_lock(&aw_haptic->lock);
		left->func->play_stop(left);
		right->func->play_stop(right);
		mutex_unlock(&aw_haptic->lock);
	} else if (ctr->play == AW_PLAY_GAIN) {
		mutex_lock(&aw_haptic->lock);
		left->func->set_gain(left, ctr->gain);
		right->func->set_gain(right, ctr->gain);
		mutex_unlock(&aw_haptic->lock);
	}
#else
	if (ctr->cmd == AW_CMD_ENABLE) {
		if (ctr->play == AW_PLAY_ENABLE) {
			aw_info("haptic_audio_play_start");
			mutex_lock(&aw_haptic->lock);
			aw_haptic->func->play_stop(aw_haptic);
			aw_haptic->func->play_mode(aw_haptic, AW_RAM_MODE);
			aw_haptic->func->set_wav_seq(aw_haptic, 0x00, ctr->wavseq);
			aw_haptic->func->set_wav_seq(aw_haptic, 0x01, 0x00);
			aw_haptic->func->set_wav_loop(aw_haptic, 0x00, ctr->loop);
			aw_haptic->func->set_gain(aw_haptic, ctr->gain);
			aw_haptic->func->play_go(aw_haptic, true);
			mutex_unlock(&aw_haptic->lock);
		} else if (ctr->play == AW_PLAY_STOP) {
			mutex_lock(&aw_haptic->lock);
			aw_haptic->func->play_stop(aw_haptic);
			mutex_unlock(&aw_haptic->lock);
		} else if (ctr->play == AW_PLAY_GAIN) {
			mutex_lock(&aw_haptic->lock);
			aw_haptic->func->set_gain(aw_haptic, ctr->gain);
			mutex_unlock(&aw_haptic->lock);
		}
	}
#endif

}

/*****************************************************
 *
 * node
 *
 *****************************************************/
#ifdef TIMED_OUTPUT
static int vibrator_get_time(struct timed_output_dev *dev)
{
	struct aw_haptic *aw_haptic = container_of(dev, struct aw_haptic, vib_dev);

	if (hrtimer_active(&aw_haptic->timer)) {
		ktime_t r = hrtimer_get_remaining(&aw_haptic->timer);

		return ktime_to_ms(r);
	}

	return 0;
}

static void vibrator_enable(struct timed_output_dev *dev, int value)
{
	struct aw_haptic *aw_haptic = container_of(dev, struct aw_haptic, vib_dev);

	aw_info("enter");
	if (!aw_haptic->ram_init) {
		aw_err("ram init failed, not allow to play!");
		return;
	}
	if (value < 0) {
		aw_err("unsupported param");
		return;
	}
	mutex_lock(&aw_haptic->lock);
	aw_haptic->state = value;
	aw_haptic->activate_mode = AW_RAM_MODE;
	mutex_unlock(&aw_haptic->lock);
	queue_work(aw_haptic->work_queue, &aw_haptic->vibrator_work);
}
#else
static enum led_brightness brightness_get(struct led_classdev *cdev)
{
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic, vib_dev);

	return aw_haptic->amplitude;
}

static void brightness_set(struct led_classdev *cdev, enum led_brightness level)
{
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic, vib_dev);

	aw_info("enter");
	if (!aw_haptic->ram_init) {
		aw_err("ram init failed, not allow to play!");
		return;
	}
	mutex_lock(&aw_haptic->lock);
	right->i2c->addr = right->record_i2c_addr;
	aw_haptic->func->set_pwm(right, AW_PWM_24K);
	aw_haptic->func->set_pwm(left, AW_PWM_24K);
	aw8693x_broadcast_config(aw_haptic, false, AW_RAM_MODE);
	aw_haptic->amplitude = level;
	aw_haptic->state = level;
	aw_haptic->activate_mode = AW_RAM_MODE;
	mutex_unlock(&aw_haptic->lock);
	queue_work(aw_haptic->work_queue, &aw_haptic->vibrator_work);
}
#endif

static ssize_t sample_param_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic, vib_dev);

	return snprintf(buf, PAGE_SIZE, "0x%04X,0x%04X,0x%04X,0x%04X,0x%04X,0x%04X\n",
			aw_haptic->sample_param[0], aw_haptic->sample_param[1],
			aw_haptic->sample_param[2], aw_haptic->sample_param[3],
			aw_haptic->sample_param[4], aw_haptic->sample_param[5]);
}

static ssize_t sample_param_store(struct device *dev, struct device_attribute *attr,
				  const char *buf, size_t count)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic, vib_dev);

	aw_get_sample_params(aw_haptic);
	return count;
}

static ssize_t algo_d2s_gain_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic, vib_dev);
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "algo_d2s_gain = %u\n",
						aw_haptic->info.algo_d2s_gain);

	return len;
}

static ssize_t algo_d2s_gain_store(struct device *dev, struct device_attribute *attr,
				  const char *buf, size_t count)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic, vib_dev);
	uint32_t val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0) {
		aw_err("kstrtouint fail");
		return rc;
	}

	mutex_lock(&aw_haptic->lock);
	aw_haptic->info.algo_d2s_gain = val;
	mutex_unlock(&aw_haptic->lock);

	return count;
}


static ssize_t state_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic, vib_dev);

	return snprintf(buf, PAGE_SIZE, "state = %d\n", aw_haptic->state);
}

static ssize_t state_store(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t count)
{
	return count;
}

static ssize_t duration_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic, vib_dev);
	ktime_t time_rem;
	s64 time_ms = 0;

	if (hrtimer_active(&aw_haptic->timer)) {
		time_rem = hrtimer_get_remaining(&aw_haptic->timer);
		time_ms = ktime_to_ms(time_rem);
	}

	return snprintf(buf, PAGE_SIZE, "duration = %lldms\n", time_ms);
}

static ssize_t duration_store(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t count)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic, vib_dev);
	uint32_t val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	/* setting 0 on duration is NOP for now */
	if (val == 0)
		return count;
	aw_info("duration=%d", val);
	aw_haptic->duration = val;

	return count;
}

static ssize_t activate_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic, vib_dev);

	return snprintf(buf, PAGE_SIZE, "activate = %d\n", aw_haptic->state);
}

static ssize_t activate_store(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t count)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic, vib_dev);
	uint32_t val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	aw_info("value=%d", val);
	if (!aw_haptic->ram_init) {
		aw_err("ram init failed, not allow to play!");
		return count;
	}
	mutex_lock(&aw_haptic->lock);
	right->i2c->addr = right->record_i2c_addr;
	aw_haptic->func->set_pwm(right, AW_PWM_24K);
	aw_haptic->func->set_pwm(left, AW_PWM_24K);
	aw8693x_broadcast_config(aw_haptic, false, AW_RAM_MODE);
	aw_haptic->state = val;
	aw_haptic->activate_mode = aw_haptic->info.mode;
	mutex_unlock(&aw_haptic->lock);
	queue_work(aw_haptic->work_queue, &aw_haptic->vibrator_work);

	return count;
}

static ssize_t activate_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic, vib_dev);

	return snprintf(buf, PAGE_SIZE, "activate_mode = %d\n", aw_haptic->activate_mode);
}

static ssize_t activate_mode_store(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t count)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic, vib_dev);
	uint32_t val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	mutex_lock(&aw_haptic->lock);
	aw_haptic->activate_mode = val;
	mutex_unlock(&aw_haptic->lock);

 	return count;
 }

static ssize_t index_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic, vib_dev);

	mutex_lock(&aw_haptic->lock);
	aw_haptic->func->get_wav_seq(aw_haptic, 1);
	aw_haptic->index = aw_haptic->seq[0];
	mutex_unlock(&aw_haptic->lock);

	return snprintf(buf, PAGE_SIZE, "index = %d\n", aw_haptic->index);
}

static ssize_t index_store(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t count)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic, vib_dev);
	uint32_t val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	if (val > aw_haptic->ram.ram_num) {
		aw_err("input value out of range!");
		return count;
	}
	aw_info("value=%d", val);
	mutex_lock(&aw_haptic->lock);
	aw_haptic->index = val;
	aw_haptic->func->set_repeat_seq(aw_haptic, aw_haptic->index);
	mutex_unlock(&aw_haptic->lock);

	return count;
}

static ssize_t vmax_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic, vib_dev);

	return snprintf(buf, PAGE_SIZE, "vmax = %dmV\n", aw_haptic->vmax);
}

static ssize_t vmax_store(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic, vib_dev);
	uint32_t val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	aw_info("value=%d", val);
	mutex_lock(&aw_haptic->lock);
	aw_haptic->vmax = val;
	aw_haptic->func->set_bst_vol(aw_haptic, aw_haptic->vmax);
	mutex_unlock(&aw_haptic->lock);

	return count;
}

static ssize_t gain_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic, vib_dev);

	return snprintf(buf, PAGE_SIZE, "gain = 0x%02X\n", aw_haptic->gain);
}

static ssize_t gain_store(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic, vib_dev);
	uint32_t val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	aw_info("value=0x%02x", val);
	mutex_lock(&aw_haptic->lock);
	aw_haptic->gain = val;
	aw_haptic->func->set_gain(aw_haptic, aw_haptic->gain);
	mutex_unlock(&aw_haptic->lock);

	return count;
}


static ssize_t vmax_mv_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic, vib_dev);

	return snprintf(buf, PAGE_SIZE, "vmax_mv = 0x%02X\n", aw_haptic->play.vmax_mv);
}

static ssize_t vmax_mv_store(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic, vib_dev);
	uint32_t val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	aw_info("vmax_mv=0x%02x", val);
	mutex_lock(&aw_haptic->lock);
	aw_haptic->play.vmax_mv = val;
	mutex_unlock(&aw_haptic->lock);

	return count;
}

static ssize_t seq_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	uint8_t i = 0;
	size_t count = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic, vib_dev);

	mutex_lock(&aw_haptic->lock);
	aw_haptic->func->get_wav_seq(aw_haptic, AW_SEQUENCER_SIZE);
	mutex_unlock(&aw_haptic->lock);
	for (i = 0; i < AW_SEQUENCER_SIZE; i++)
		count += snprintf(buf + count, PAGE_SIZE - count,
				  "seq%d = %d\n", i + 1, aw_haptic->seq[i]);

	return count;
}

static ssize_t seq_store(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t count)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic, vib_dev);
	uint32_t databuf[2] = { 0, 0 };

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) {
		if (databuf[0] >= AW_SEQUENCER_SIZE || databuf[1] > aw_haptic->ram.ram_num) {
			aw_err("input value out of range!");
			return count;
		}
		aw_info("seq%d=0x%02X", databuf[0], databuf[1]);
		mutex_lock(&aw_haptic->lock);
		aw_haptic->seq[databuf[0]] = (uint8_t)databuf[1];
		aw_haptic->func->set_wav_seq(aw_haptic, (uint8_t)databuf[0],
					     aw_haptic->seq[databuf[0]]);
		mutex_unlock(&aw_haptic->lock);
	}

	return count;
}

static ssize_t loop_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	size_t count = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic, vib_dev);

	mutex_lock(&aw_haptic->lock);
	count = aw_haptic->func->get_wav_loop(aw_haptic, buf);
	mutex_unlock(&aw_haptic->lock);

	return count;
}

static ssize_t loop_store(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic, vib_dev);
	uint32_t databuf[2] = { 0, 0 };

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) {
		aw_info("seq%d loop=0x%02X", databuf[0], databuf[1]);
		mutex_lock(&aw_haptic->lock);
		aw_haptic->loop[databuf[0]] = (uint8_t)databuf[1];
		aw_haptic->func->set_wav_loop(aw_haptic, (uint8_t)databuf[0],
					      aw_haptic->loop[databuf[0]]);
		mutex_unlock(&aw_haptic->lock);
	}

	return count;
}

static ssize_t reg_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic, vib_dev);
	ssize_t len = 0;

	mutex_lock(&aw_haptic->lock);
	len = aw_haptic->func->get_reg(aw_haptic, len, buf);
	mutex_unlock(&aw_haptic->lock);

	return len;
}

static ssize_t reg_store(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t count)
{
	uint8_t val = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic, vib_dev);
	uint32_t databuf[2] = { 0, 0 };

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) {
		val = (uint8_t)databuf[1];
		mutex_lock(&aw_haptic->lock);
		haptic_hv_i2c_writes(aw_haptic, (uint8_t)databuf[0], &val, AW_I2C_BYTE_ONE);
		mutex_unlock(&aw_haptic->lock);
	}

	return count;
}

static ssize_t rtp_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic, vib_dev);
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "rtp_cnt = %u\n", aw_haptic->rtp_cnt);

	return len;
}

static ssize_t rtp_store(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t count)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic, vib_dev);
	uint32_t val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0) {
		aw_err("kstrtouint fail");
		return rc;
	}
	mutex_lock(&aw_haptic->lock);
	if ((val > 0) && (val < aw_haptic->rtp_num)) {
		aw_haptic->state = 1;
		aw_haptic->rtp_file_num = val;
		aw_info("aw_rtp_name[%d]: %s", val, aw_rtp_name[val]);
	} else if (val == 0) {
		aw_haptic->state = 0;
	} else {
		aw_haptic->state = 0;
		aw_err("input number error:%d", val);
	}
	aw8693x_broadcast_config(aw_haptic, false, AW_RTP_MODE);
	mutex_unlock(&aw_haptic->lock);
	queue_work(aw_haptic->work_queue, &aw_haptic->rtp_work);

	return count;
}

static ssize_t ram_update_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic, vib_dev);
	ssize_t len = 0;
	int i = 0;
	uint8_t *ram_buf = NULL;

	aw_info("ram len = %d", aw_haptic->ram.len);
	ram_buf = kzalloc(aw_haptic->ram.len, GFP_KERNEL);
	if (!ram_buf) {
		aw_err("Error allocating memory");
		return len;
	}
	mutex_lock(&aw_haptic->lock);
	aw_haptic->func->play_stop(aw_haptic);
	/* RAMINIT Enable */
	aw_haptic->func->ram_init(aw_haptic, true);
	aw_haptic->func->set_ram_addr(aw_haptic);
	aw_haptic->func->get_ram_data(aw_haptic, ram_buf);
	for (i = 1; i <= aw_haptic->ram.len; i++) {
		len += snprintf(buf + len, PAGE_SIZE, "0x%02x,", *(ram_buf + i - 1));
		if (i % 16 == 0 || i == aw_haptic->ram.len) {
			len = 0;
			aw_info("%s", buf);
		}
	}
	kfree(ram_buf);
	/* RAMINIT Disable */
	aw_haptic->func->ram_init(aw_haptic, false);
	len = snprintf(buf, PAGE_SIZE, "Please check log\n");
	mutex_unlock(&aw_haptic->lock);

	return len;
}

static ssize_t ram_update_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic, vib_dev);
	uint32_t val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	if (val)
		ram_update(aw_haptic);

	return count;
}

static ssize_t ram_num_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic, vib_dev);
	ssize_t len = 0;

	get_ram_num(aw_haptic);
	len += snprintf(buf + len, PAGE_SIZE - len, "ram_num = %d\n", aw_haptic->ram.ram_num);

	return len;
}

static ssize_t f0_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic, vib_dev);
	ssize_t len = 0;

	mutex_lock(&aw_haptic->lock);
	aw_haptic->func->upload_lra(aw_haptic, AW_WRITE_ZERO);
	aw_haptic->func->get_f0(aw_haptic);
	mutex_unlock(&aw_haptic->lock);
	len += snprintf(buf + len, PAGE_SIZE - len, "%u\n", aw_haptic->f0);

	return len;
}

static ssize_t ram_f0_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic, vib_dev);
	ssize_t len = 0;

	mutex_lock(&aw_haptic->lock);
	aw_haptic->func->upload_lra(aw_haptic, AW_WRITE_ZERO);
	aw_haptic->func->ram_get_f0(aw_haptic);
	mutex_unlock(&aw_haptic->lock);
	len += snprintf(buf + len, PAGE_SIZE - len, "f0 = %u\n", aw_haptic->f0);

	return len;
}

static ssize_t cali_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic, vib_dev);
	ssize_t len = 0;

	mutex_lock(&aw_haptic->lock);
	aw_haptic->func->upload_lra(aw_haptic, AW_F0_CALI_LRA);
	aw_haptic->func->get_f0(aw_haptic);
	mutex_unlock(&aw_haptic->lock);
	len += snprintf(buf + len, PAGE_SIZE - len, "f0 = %u\n", aw_haptic->f0);

	return len;
}

static ssize_t cali_store(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic, vib_dev);
	uint32_t val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	if (val) {
		mutex_lock(&aw_haptic->lock);
		f0_cali(aw_haptic);
		mutex_unlock(&aw_haptic->lock);
	}

	return count;
}

static ssize_t ram_cali_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic, vib_dev);
	ssize_t len = 0;

	mutex_lock(&aw_haptic->lock);
	aw_haptic->func->upload_lra(aw_haptic, AW_F0_CALI_LRA);
	aw_haptic->func->ram_get_f0(aw_haptic);
	mutex_unlock(&aw_haptic->lock);
	len += snprintf(buf + len, PAGE_SIZE - len, "f0 = %u\n", aw_haptic->f0);

	return len;
}

static ssize_t ram_cali_store(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t count)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic, vib_dev);
	uint32_t val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	if (val) {
		mutex_lock(&aw_haptic->lock);
		ram_f0_cali(aw_haptic);
		mutex_unlock(&aw_haptic->lock);
	}

	return count;
}


static ssize_t cont_store(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic, vib_dev);
	uint32_t val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	mutex_lock(&aw_haptic->lock);
	aw_haptic->func->play_stop(aw_haptic);
	if (val) {
		aw_haptic->func->upload_lra(aw_haptic, AW_F0_CALI_LRA);
		aw_haptic->func->cont_config(aw_haptic);
	}
	mutex_unlock(&aw_haptic->lock);

	return count;
}

static ssize_t vbat_monitor_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic, vib_dev);
	ssize_t len = 0;

	mutex_lock(&aw_haptic->lock);
	aw_haptic->func->get_vbat(aw_haptic);
	len += snprintf(buf + len, PAGE_SIZE - len, "vbat_monitor = %u\n", aw_haptic->vbat);
	mutex_unlock(&aw_haptic->lock);

	return len;
}

static ssize_t lra_resistance_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic, vib_dev);
	ssize_t len = 0;

	mutex_lock(&aw_haptic->lock);
	aw_haptic->func->get_lra_resistance(aw_haptic);
	mutex_unlock(&aw_haptic->lock);
	len += snprintf(buf + len, PAGE_SIZE - len, "%u\n", aw_haptic->lra);

	return len;
}

static ssize_t auto_boost_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic, vib_dev);
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "auto_boost = %d\n", aw_haptic->auto_boost);

	return len;
}

static ssize_t auto_boost_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic, vib_dev);
	uint32_t val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	mutex_lock(&aw_haptic->lock);
	aw_haptic->func->play_stop(aw_haptic);
	aw_haptic->func->auto_bst_enable(aw_haptic, val);
	mutex_unlock(&aw_haptic->lock);

	return count;
}

static ssize_t prct_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic, vib_dev);
	ssize_t len = 0;
	uint8_t reg_val = 0;

	mutex_lock(&aw_haptic->lock);
	reg_val = aw_haptic->func->get_prctmode(aw_haptic);
	mutex_unlock(&aw_haptic->lock);
	len += snprintf(buf + len, PAGE_SIZE - len, "prctmode = %d\n", reg_val);

	return len;
}

static ssize_t prct_mode_store(struct device *dev, struct device_attribute *attr,
			       const char *buf, size_t count)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic, vib_dev);
	uint32_t databuf[2] = { 0, 0 };
	uint32_t prtime = 0;
	uint32_t prlvl = 0;

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) {
		prtime = databuf[0];
		prlvl = databuf[1];
		mutex_lock(&aw_haptic->lock);
		aw_haptic->func->protect_config(aw_haptic, prtime, prlvl);
		mutex_unlock(&aw_haptic->lock);
	}

	return count;
}

static ssize_t ram_vbat_comp_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic, vib_dev);
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "ram_vbat_comp = %d\n",
			aw_haptic->ram_vbat_comp);

	return len;
}

static ssize_t ram_vbat_comp_store(struct device *dev, struct device_attribute *attr,
				   const char *buf, size_t count)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic, vib_dev);
	uint32_t val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	mutex_lock(&aw_haptic->lock);
	if (val)
		aw_haptic->ram_vbat_comp = AW_RAM_VBAT_COMP_ENABLE;
	else
		aw_haptic->ram_vbat_comp = AW_RAM_VBAT_COMP_DISABLE;
	mutex_unlock(&aw_haptic->lock);

	return count;
}

static ssize_t osc_cali_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic, vib_dev);
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "%u\n",
			aw_haptic->osc_cali_data);

	return len;
}

static ssize_t osc_cali_store(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t count)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic, vib_dev);
	uint32_t val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	mutex_lock(&aw_haptic->lock);
	if (val == 1) {
		rtp_trim_lra_cali(aw_haptic);
	} else if (val == 2) {
		aw_haptic->func->upload_lra(aw_haptic, AW_OSC_CALI_LRA);
		rtp_osc_cali(aw_haptic);
	}
	mutex_unlock(&aw_haptic->lock);

	return count;
}

static ssize_t haptic_audio_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic, vib_dev);

	ssize_t len = 0;

	len += snprintf(buf+len, PAGE_SIZE-len, "%d\n", aw_haptic->haptic_audio.ctr.cnt);

	return len;
}

static ssize_t haptic_audio_store(struct device *dev, struct device_attribute *attr,
				  const char *buf, size_t count)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic, vib_dev);

	uint32_t databuf[6] = { 0 };
	struct aw_haptic_ctr *hap_ctr = NULL;

	if (!aw_haptic->ram_init) {
		aw_err("ram init failed, not allow to play!");
		return count;
	}
	if (sscanf(buf, "%u %u %u %u %u %u", &databuf[0], &databuf[1],
		   &databuf[2], &databuf[3], &databuf[4], &databuf[5]) == 6) {
		if (databuf[2]) {
			aw_info("cnt=%d, cmd=%d, play=%d, wavseq=%d, loop=%d, gain=%d",
				databuf[0], databuf[1], databuf[2], databuf[3],
				databuf[4], databuf[5]);
		}

		hap_ctr = kzalloc(sizeof(struct aw_haptic_ctr), GFP_KERNEL);
		if (hap_ctr == NULL)
			return count;

		mutex_lock(&aw_haptic->haptic_audio.lock);
		hap_ctr->cnt = (uint8_t)databuf[0];
		hap_ctr->cmd = (uint8_t)databuf[1];
		hap_ctr->play = (uint8_t)databuf[2];
		hap_ctr->wavseq = (uint8_t)databuf[3];
		hap_ctr->loop = (uint8_t)databuf[4];
		hap_ctr->gain = (uint8_t)databuf[5];
		audio_ctrl_list_ins(aw_haptic, hap_ctr);
		if (hap_ctr->cmd == AW_CMD_STOP) {
			aw_info("haptic_audio stop");
			if (hrtimer_active(&aw_haptic->haptic_audio.timer)) {
				aw_info("cancel haptic_audio_timer");
				hrtimer_cancel(&aw_haptic->haptic_audio.timer);
				aw_haptic->haptic_audio.ctr.cnt = 0;
				audio_off(aw_haptic);
			}
		} else {
			if (hrtimer_active(&aw_haptic->haptic_audio.timer)) {
				/* */
			} else {
				aw_info("start haptic_audio_timer");
				audio_init(aw_haptic);
				hrtimer_start(&aw_haptic->haptic_audio.timer,
					      ktime_set(aw_haptic->haptic_audio.delay_val / 1000000,
							(aw_haptic->haptic_audio.delay_val %
							 1000000) * 1000), HRTIMER_MODE_REL);
			}
		}
		mutex_unlock(&aw_haptic->haptic_audio.lock);
		kfree(hap_ctr);
	}

	return count;
}

static ssize_t haptic_audio_time_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic, vib_dev);

	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "haptic_audio.delay_val=%dus\n",
			aw_haptic->haptic_audio.delay_val);
	len += snprintf(buf + len, PAGE_SIZE - len, "haptic_audio.timer_val=%dus\n",
			aw_haptic->haptic_audio.timer_val);

	return len;
}

static ssize_t haptic_audio_time_store(struct device *dev, struct device_attribute *attr,
				       const char *buf, size_t count)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic, vib_dev);

	uint32_t databuf[2] = { 0 };

	if (sscanf(buf, "%u %u", &databuf[0], &databuf[1]) == 2) {
		aw_haptic->haptic_audio.delay_val = databuf[0];
		aw_haptic->haptic_audio.timer_val = databuf[1];
	}

	return count;
}

static ssize_t gun_type_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic, vib_dev);

	return snprintf(buf, PAGE_SIZE, "0x%02x\n", aw_haptic->gun_type);
}

static ssize_t gun_type_store(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t count)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic, vib_dev);

	uint32_t val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	aw_dbg("value=%d", val);
	mutex_lock(&aw_haptic->lock);
	aw_haptic->gun_type = val;
	mutex_unlock(&aw_haptic->lock);

	return count;
}

static ssize_t bullet_nr_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic, vib_dev);

	return snprintf(buf, PAGE_SIZE, "0x%02x\n", aw_haptic->bullet_nr);
}

static ssize_t bullet_nr_store(struct device *dev, struct device_attribute *attr,
			       const char *buf, size_t count)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic, vib_dev);

	uint32_t val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	aw_dbg("value=%d", val);
	mutex_lock(&aw_haptic->lock);
	aw_haptic->bullet_nr = val;
	mutex_unlock(&aw_haptic->lock);

	return count;
}

static ssize_t awrw_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic, vib_dev);
	int i = 0;
	ssize_t len = 0;

	if (aw_haptic->i2c_info.flag != AW_SEQ_READ) {
		aw_err("no read mode");
		return -ERANGE;
	}
	if (aw_haptic->i2c_info.reg_data == NULL) {
		aw_err("awrw lack param");
		return -ERANGE;
	}
	for (i = 0; i < aw_haptic->i2c_info.reg_num; i++) {
		len += snprintf(buf + len, PAGE_SIZE - len,
				"0x%02x,", aw_haptic->i2c_info.reg_data[i]);
	}
	len += snprintf(buf + len - 1, PAGE_SIZE - len, "\n");

	return len;
}

static ssize_t awrw_store(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	uint8_t value = 0;
	char data_buf[5] = { 0 };
	uint32_t flag = 0;
	uint32_t reg_num = 0;
	uint32_t reg_addr = 0;
	int i = 0;
	int rc = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic, vib_dev);

	if (sscanf(buf, "%x %x %x", &flag, &reg_num, &reg_addr) == 3) {
		if (!reg_num) {
			aw_err("param error");
			return -ERANGE;
		}
		aw_haptic->i2c_info.flag = flag;
		aw_haptic->i2c_info.reg_num = reg_num;
		if (aw_haptic->i2c_info.reg_data != NULL)
			kfree(aw_haptic->i2c_info.reg_data);
		aw_haptic->i2c_info.reg_data = kmalloc(reg_num, GFP_KERNEL);
		if (flag == AW_SEQ_WRITE) {
			if ((reg_num * 5) != (strlen(buf) - 3 * 5)) {
				aw_err("param error");
				return -ERANGE;
			}
			for (i = 0; i < reg_num; i++) {
				memcpy(data_buf, &buf[15 + i * 5], 4);
				data_buf[4] = '\0';
				rc = kstrtou8(data_buf, 0, &value);
				if (rc < 0) {
					aw_err("param error");
					return -ERANGE;
				}
				aw_haptic->i2c_info.reg_data[i] = value;
			}
			mutex_lock(&aw_haptic->lock);
			haptic_hv_i2c_writes(aw_haptic, (uint8_t)reg_addr,
				    aw_haptic->i2c_info.reg_data, reg_num);
			mutex_unlock(&aw_haptic->lock);
		} else if (flag == AW_SEQ_READ) {
			mutex_lock(&aw_haptic->lock);
			haptic_hv_i2c_reads(aw_haptic, reg_addr,
				    aw_haptic->i2c_info.reg_data, reg_num);
			mutex_unlock(&aw_haptic->lock);
		}
	} else {
		aw_err("param error");
	}

	return count;
}

static ssize_t f0_save_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic, vib_dev);

	len += snprintf(buf + len, PAGE_SIZE - len, "%u\n",
			aw_haptic->f0_cali_data);

	return len;
}

static ssize_t f0_save_store(struct device *dev, struct device_attribute *attr,
			     const char *buf, size_t count)
{
	uint32_t val = 0;
	int rc = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic, vib_dev);

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	aw_haptic->f0_cali_data = val;

	return count;
}

static ssize_t osc_save_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic, vib_dev);

	len += snprintf(buf + len, PAGE_SIZE - len, "%u\n",
			aw_haptic->osc_cali_data);

	return len;
}

static ssize_t osc_save_store(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t count)
{
	uint32_t val = 0;
	int rc = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic, vib_dev);

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	aw_haptic->osc_cali_data = val;

	return count;
}

#ifdef AW_DOUBLE
static ssize_t dual_cont_store(struct device *dev, struct device_attribute *attr,
			       const char *buf, size_t count)
{
	uint32_t val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	right->func->play_stop(right);
	left->func->play_stop(left);
	if (val) {
		right->func->upload_lra(right, AW_F0_CALI_LRA);
		left->func->upload_lra(left, AW_F0_CALI_LRA);
		right->func->cont_config(right);
		left->func->cont_config(left);
	}

	return count;
}

static ssize_t dual_index_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "%d %d\n", left->index, right->index);

	return len;
}

static ssize_t dual_index_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	int index_l = 0;
	int index_r = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic, vib_dev);

	if (sscanf(buf, "%d %d", &index_l, &index_r) == 2) {
		aw_info("index_l=%d index_r=%d", index_l, index_r);
		if (index_l > aw_haptic->ram.ram_num || index_r > aw_haptic->ram.ram_num) {
			aw_err("input value out of range!");
			return count;
		}
		mutex_lock(&aw_haptic->lock);
		left->index = index_l;
		left->func->set_repeat_seq(left, left->index);
		right->index = index_r;
		right->func->set_repeat_seq(right, right->index);
		mutex_unlock(&aw_haptic->lock);
	}

	return count;
}

static ssize_t dual_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic, vib_dev);

	return snprintf(buf, PAGE_SIZE, "dual_mode = %d\n", aw_haptic->activate_mode);
}

static ssize_t dual_mode_store(struct device *dev, struct device_attribute *attr,
			       const char *buf, size_t count)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic, vib_dev);
	uint32_t val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	mutex_lock(&aw_haptic->lock);
	left->activate_mode = val;
	right->activate_mode = val;
	mutex_unlock(&aw_haptic->lock);

	return count;
}

static ssize_t dual_duration_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic, vib_dev);
	ktime_t time_rem;
	s64 time_ms = 0;

	if (hrtimer_active(&aw_haptic->timer)) {
		time_rem = hrtimer_get_remaining(&aw_haptic->timer);
		time_ms = ktime_to_ms(time_rem);
	}

	return snprintf(buf, PAGE_SIZE, "duration = %lldms\n", time_ms);
}

static ssize_t dual_duration_store(struct device *dev, struct device_attribute *attr,
				   const char *buf, size_t count)
{
	int duration_l = 0;
	int duration_r = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic, vib_dev);

	if (sscanf(buf, "%d %d", &duration_l, &duration_r) == 2) {
		mutex_lock(&aw_haptic->lock);
		left->duration = duration_l;
		right->duration = duration_r;
		mutex_unlock(&aw_haptic->lock);
	}

	return count;
}

static ssize_t dual_activate_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic, vib_dev);

	return snprintf(buf, PAGE_SIZE, "activate = %d\n", aw_haptic->state);
}

static ssize_t dual_activate_store(struct device *dev, struct device_attribute *attr,
				   const char *buf, size_t count)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic, vib_dev);
	uint32_t val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	aw_info("value=%d", val);
	if (!left->ram_init || !right->ram_init) {
		aw_err("ram init failed, not allow to play!");
		return count;
	}
	mutex_lock(&aw_haptic->lock);
	(void)down_trylock(&left->sema);
	up(&left->sema);
	left->state = val;
	right->state = val;
	left->dual_flag = true;
	right->dual_flag = true;
	mutex_unlock(&aw_haptic->lock);
	queue_work(left->work_queue, &left->vibrator_work);
	queue_work(right->work_queue, &right->vibrator_work);

	return count;
}


static ssize_t dual_rtp_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "left_rtp_cnt = %u\n", left->rtp_cnt);
	len += snprintf(buf + len, PAGE_SIZE - len, "right_rtp_cnt = %u\n", right->rtp_cnt);

	return len;
}

static ssize_t dual_rtp_store(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t count)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic, vib_dev);
	int rtp_l = 0;
	int rtp_r = 0;

	mutex_lock(&aw_haptic->lock);
	(void)down_trylock(&left->sema);
	up(&left->sema);
	if (sscanf(buf, "%d %d", &rtp_l, &rtp_r) == 2) {
		if (rtp_l > 0 && rtp_l < aw_haptic->rtp_num) {
			left->state = 1;
			left->rtp_file_num = rtp_l;
		} else if (rtp_l == 0) {
			left->state = 0;
		} else {
			left->state = 0;
			aw_err("input number error:%d", rtp_l);
		}
		if (rtp_r > 0 && rtp_r < aw_haptic->rtp_num) {
			right->state = 1;
			right->rtp_file_num = rtp_r;
		} else if (rtp_r == 0) {
			right->state = 0;
		} else {
			right->state = 0;
			aw_err("input number error:%d", rtp_r);
		}
	}
	left->dual_flag = true;
	right->dual_flag = true;
	mutex_unlock(&aw_haptic->lock);
	queue_work(left->work_queue, &left->rtp_work);
	queue_work(right->work_queue, &right->rtp_work);

	return count;
}

/* return buffer size and availbe size */
static ssize_t aw_haptic_custom_wave_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic, vib_dev);
	ssize_t len = 0;

	len +=
		snprintf(buf + len, PAGE_SIZE - len, "period_size=%d;",
		aw_haptic->ram.base_addr >> 2);
	len +=
		snprintf(buf + len, PAGE_SIZE - len,
		"max_size=%d;free_size=%d;",
		get_rb_max_size(), get_rb_free_size());
	len +=
		snprintf(buf + len, PAGE_SIZE - len,
		"custom_wave_id=%d;", CUSTOME_WAVE_ID);
	return len;
}

static ssize_t aw_haptic_custom_wave_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic, vib_dev);
	unsigned long  buf_len, period_size, offset;
	int ret;

	period_size = (aw_haptic->ram.base_addr >> 2);
	offset = 0;
	ret = 0;

	aw_info("aw_haptic_custom_wave_store:write szie %zd, period size %lu", count,
		 period_size);
	if(period_size == 0)
		return ret;
	if (count % period_size || count < period_size)
		rb_end();
	atomic_set(&aw_haptic->is_in_write_loop, 1);

	while (count > 0) {
		buf_len = MIN(count, period_size);
		ret = write_rb(buf + offset,  buf_len);
		if (ret < 0)
			goto exit;
		count -= buf_len;
		offset += buf_len;
	}
	ret = offset;
exit:
	atomic_set(&aw_haptic->is_in_write_loop, 0);
	wake_up_interruptible(&aw_haptic->stop_wait_q);
	aw_info(" return size %d", ret);
	return ret;
}

ssize_t aw_haptic_custom_wave_store_to_ext(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic, vib_dev);
	unsigned long  buf_len, period_size, offset;
	int ret;

	period_size = (aw_haptic->ram.base_addr >> 2);
	offset = 0;

	aw_info("aw_haptic_custom_wave_store_to_ext:write szie %zd, period size %lu, ram.base_addr %u", count, 
		 period_size, aw_haptic->ram.base_addr);
	if (count % period_size || count < period_size) {
		rb_end();
	}
	atomic_set(&aw_haptic->is_in_write_loop, 1);

	while (count > 0) {
		buf_len = MIN(count, period_size);
		ret = write_rb(buf + offset,  buf_len);
		if (ret < 0)
			goto exit;
		count -= buf_len;
		offset += buf_len;
	}
	ret = offset;
exit:
	atomic_set(&aw_haptic->is_in_write_loop, 0);
	wake_up_interruptible(&aw_haptic->stop_wait_q);
	aw_info(" return size %d", ret);
	return ret;
}
EXPORT_SYMBOL_GPL(aw_haptic_custom_wave_store_to_ext);

static ssize_t aw86938_f0_check_show(struct device *dev,
  					struct device_attribute *attr,
  					char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic, vib_dev);
	ssize_t len = 0;

  	if (aw_haptic->f0_cali_status == true)
  		len += snprintf(buf + len, PAGE_SIZE - len, "%d\n", 1);
  	if (aw_haptic->f0_cali_status == false)
  		len += snprintf(buf + len, PAGE_SIZE - len, "%d\n", 0);

  	return len;
}

#endif


static DEVICE_ATTR_RW(algo_d2s_gain);
static DEVICE_ATTR_RW(sample_param);
static DEVICE_ATTR_RO(f0);
static DEVICE_ATTR_RO(ram_f0);
static DEVICE_ATTR_RW(seq);
static DEVICE_ATTR_RW(reg);
static DEVICE_ATTR_RW(vmax);
static DEVICE_ATTR_RW(gain);
static DEVICE_ATTR_RW(loop);
static DEVICE_ATTR_RW(rtp);
static DEVICE_ATTR_RW(cali);
static DEVICE_ATTR_RW(ram_cali);
static DEVICE_ATTR_WO(cont);
static DEVICE_ATTR_RW(awrw);
static DEVICE_ATTR_RW(state);
static DEVICE_ATTR_RW(index);
static DEVICE_ATTR_RO(ram_num);
static DEVICE_ATTR_RW(duration);
static DEVICE_ATTR_RW(activate);
static DEVICE_ATTR_RW(osc_cali);
static DEVICE_ATTR_RW(gun_type);
static DEVICE_ATTR_RW(prct_mode);
static DEVICE_ATTR_RW(bullet_nr);
static DEVICE_ATTR_RW(auto_boost);
static DEVICE_ATTR_RW(ram_update);
static DEVICE_ATTR_RW(haptic_audio);
static DEVICE_ATTR_RO(vbat_monitor);
static DEVICE_ATTR_RW(activate_mode);
static DEVICE_ATTR_RW(ram_vbat_comp);
static DEVICE_ATTR_RO(lra_resistance);
static DEVICE_ATTR_RW(haptic_audio_time);
static DEVICE_ATTR_RW(osc_save);
static DEVICE_ATTR_RW(f0_save);
static DEVICE_ATTR(custom_wave, S_IWUSR | S_IRUGO, aw_haptic_custom_wave_show,
		   aw_haptic_custom_wave_store);
static DEVICE_ATTR(f0_check, S_IRUGO, aw86938_f0_check_show, NULL);
static DEVICE_ATTR(vmax_mv, S_IWUSR | S_IRUGO, vmax_mv_show,
		   vmax_mv_store);
#ifdef AW_DOUBLE
static DEVICE_ATTR_WO(dual_cont);
static DEVICE_ATTR_RW(dual_index);
static DEVICE_ATTR_RW(dual_mode);
static DEVICE_ATTR_RW(dual_duration);
static DEVICE_ATTR_RW(dual_activate);
static DEVICE_ATTR_RW(dual_rtp);
#endif

static struct attribute *vibrator_attributes[] = {
	&dev_attr_algo_d2s_gain.attr,
	&dev_attr_sample_param.attr,
	&dev_attr_state.attr,
	&dev_attr_duration.attr,
	&dev_attr_activate.attr,
	&dev_attr_activate_mode.attr,
	&dev_attr_index.attr,
	&dev_attr_vmax.attr,
	&dev_attr_gain.attr,
	&dev_attr_seq.attr,
	&dev_attr_loop.attr,
	&dev_attr_reg.attr,
	&dev_attr_rtp.attr,
	&dev_attr_ram_update.attr,
	&dev_attr_ram_num.attr,
	&dev_attr_f0.attr,
	&dev_attr_ram_f0.attr,
	&dev_attr_f0_save.attr,
	&dev_attr_cali.attr,
	&dev_attr_ram_cali.attr,
	&dev_attr_cont.attr,
	&dev_attr_vbat_monitor.attr,
	&dev_attr_lra_resistance.attr,
	&dev_attr_auto_boost.attr,
	&dev_attr_prct_mode.attr,
	&dev_attr_ram_vbat_comp.attr,
	&dev_attr_osc_cali.attr,
	&dev_attr_osc_save.attr,
	&dev_attr_haptic_audio.attr,
	&dev_attr_haptic_audio_time.attr,
	&dev_attr_gun_type.attr,
	&dev_attr_bullet_nr.attr,
	&dev_attr_awrw.attr,
	&dev_attr_custom_wave.attr,
	&dev_attr_f0_check.attr,
	&dev_attr_vmax_mv.attr,
#ifdef AW_DOUBLE
	&dev_attr_dual_cont.attr,
	&dev_attr_dual_index.attr,
	&dev_attr_dual_mode.attr,
	&dev_attr_dual_duration.attr,
	&dev_attr_dual_activate.attr,
	&dev_attr_dual_rtp.attr,
#endif

	NULL
};

static struct attribute_group vibrator_attribute_group = {
	.attrs = vibrator_attributes
};

#ifdef AW_TIKTAP
static inline unsigned int tiktap_get_sys_msecs(void)
{
#if (KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE)
	struct timespec64 ts64;

	ktime_get_coarse_real_ts64(&ts64);
#else
	struct timespec64 ts64 = current_kernel_time64();
#endif

	return jiffies_to_msecs(timespec64_to_jiffies(&ts64));
}

static void tiktap_work_routine(struct work_struct *work)
{
	struct aw_haptic *aw_haptic = container_of(work, struct aw_haptic, tiktap_work);
	struct mmap_buf_format *tiktap_buf = aw_haptic->start_buf;
	int count = 100;
	unsigned char reg_val = 0x10;
	unsigned char glb_state_val = 0;
	unsigned int write_start;
	unsigned int buf_cnt = 0;

	mutex_lock(&aw_haptic->lock);
	aw_haptic->tiktap_stop_flag = false;
	aw_haptic->func->play_mode(aw_haptic, AW_RTP_MODE);
	aw_haptic->func->upload_lra(aw_haptic, AW_OSC_CALI_LRA);
	while (true && count--) {
		if (tiktap_buf->status == MMAP_BUF_DATA_VALID) {
			aw_haptic->func->play_go(aw_haptic, true);
			mdelay(1);
			break;
		} else if (aw_haptic->tiktap_stop_flag == true) {
			mutex_unlock(&aw_haptic->lock);
			return;
		}
		mdelay(1);
	}
	if (count <= 0) {
		aw_err("wait 100 ms but start_buf->status != VALID! status = 0x%02x",
		       tiktap_buf->status);
		aw_haptic->tiktap_stop_flag = true;
		mutex_unlock(&aw_haptic->lock);
		return;
	}
	aw_haptic->tiktap_ready = true;
	mutex_unlock(&aw_haptic->lock);

	mutex_lock(&aw_haptic->rtp_lock);
	pm_qos_enable(aw_haptic, true);
	write_start = tiktap_get_sys_msecs();
	while (true) {
		if (tiktap_get_sys_msecs() > (write_start + 800)) {
			aw_err("Failed! tiktap endless loop");
			break;
		}
		reg_val = aw_haptic->func->rtp_get_fifo_aes(aw_haptic);
		glb_state_val = aw_haptic->func->get_glb_state(aw_haptic);
		if ((glb_state_val & AW_GLBRD_STATE_MASK) != AW_STATE_RTP) {
			aw_err("tiktap glb_state != RTP_GO!, glb_state = 0x%02x", glb_state_val);
			break;
		}
		if ((aw_haptic->tiktap_stop_flag == true) ||
		    (tiktap_buf->status == MMAP_BUF_DATA_FINISHED) ||
		    (tiktap_buf->status == MMAP_BUF_DATA_INVALID)) {
			aw_err("tiktap exit! tiktap_buf->status = 0x%02x", tiktap_buf->status);
			break;
		} else if ((tiktap_buf->status == MMAP_BUF_DATA_VALID) && (reg_val & 0x01)) {
			aw_info("buf_cnt = %d, bit = %d, length = %d!",
				buf_cnt, tiktap_buf->bit, tiktap_buf->length);

			aw_haptic->func->set_rtp_data(aw_haptic, tiktap_buf->data, tiktap_buf->length);
			tiktap_buf->status = MMAP_BUF_DATA_FINISHED;

			tiktap_buf = tiktap_buf->kernel_next;
			write_start = tiktap_get_sys_msecs();
			buf_cnt++;
		} else {
			mdelay(1);
		}
	}
	pm_qos_enable(aw_haptic, false);
	aw_haptic->tiktap_stop_flag = true;
	mutex_unlock(&aw_haptic->rtp_lock);
}

static void tiktap_clean_buf(struct aw_haptic *aw_haptic, int status)
{
	struct mmap_buf_format *tiktap_buf = aw_haptic->start_buf;
	int i = 0;

	for (i = 0; i < AW_TIKTAP_MMAP_BUF_SUM; i++) {
		tiktap_buf->status = status;
		tiktap_buf = tiktap_buf->kernel_next;
	}
}

static long tiktap_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	unsigned int tmp = 0;
	int ret = 0;
	struct aw_haptic *aw_haptic = (struct aw_haptic *)file->private_data;

	switch (cmd) {
	case TIKTAP_GET_HWINFO:
		aw_info("cmd = TIKTAP_GET_HWINFO!");
		tmp = aw_haptic->chipid;
		if (copy_to_user((void __user *)arg, &tmp, sizeof(unsigned int)))
			ret = -EFAULT;
		break;
	case TIKTAP_GET_F0:
		aw_info("cmd = TIKTAP_GET_F0!");
		tmp = aw_haptic->f0;
		if (copy_to_user((void __user *)arg, &tmp, sizeof(unsigned int)))
			ret = -EFAULT;
		break;
	case TIKTAP_STOP_MODE:
		aw_info("cmd = TIKTAP_STOP_MODE!");
		tiktap_clean_buf(aw_haptic, MMAP_BUF_DATA_INVALID);
		aw_haptic->tiktap_stop_flag = true;
		mutex_lock(&aw_haptic->lock);
		aw_haptic->func->play_stop(aw_haptic);
		mutex_unlock(&aw_haptic->lock);
		break;
	case TIKTAP_RTP_MODE:
		/* aw_info("cmd = TIKTAP_RTP_MODE!"); */
		tiktap_clean_buf(aw_haptic, MMAP_BUF_DATA_INVALID);
		aw_haptic->tiktap_stop_flag = true;
		mutex_lock(&aw_haptic->lock);
		aw_haptic->func->play_stop(aw_haptic);
		mutex_unlock(&aw_haptic->lock);
		queue_work(aw_haptic->work_queue, &aw_haptic->tiktap_work);
		break;
	case TIKTAP_SETTING_GAIN:
		aw_info("cmd = TIKTAP_SETTING_GAIN!");
		if (arg > 0x80)
			arg = 0x80;
		mutex_lock(&aw_haptic->lock);
		aw_haptic->func->set_gain(aw_haptic, (uint8_t)arg);
		mutex_unlock(&aw_haptic->lock);
		break;
	default:
		aw_info("unknown cmd = %d", cmd);
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long tiktap_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;

	ret = tiktap_unlocked_ioctl(file, cmd, arg);

	return ret;
}
#endif

static int tiktap_file_mmap(struct file *file, struct vm_area_struct *vma)
{
	unsigned long phys;
	struct aw_haptic *aw_haptic = (struct aw_haptic *)file->private_data;
	int ret = 0;

#if KERNEL_VERSION(4, 7, 0) < LINUX_VERSION_CODE
	vm_flags_t vm_flags = calc_vm_prot_bits(PROT_READ|PROT_WRITE, 0) |
			      calc_vm_flag_bits(MAP_SHARED);

	vm_flags |= current->mm->def_flags | VM_MAYREAD | VM_MAYWRITE |
		    VM_MAYEXEC | VM_SHARED | VM_MAYSHARE;

	if (!vma || (pgprot_val(vma->vm_page_prot) != pgprot_val(vm_get_page_prot(vm_flags)))) {
		aw_err("vm_page_prot error!");
		return -EPERM;
	}

	if (!vma || ((vma->vm_end - vma->vm_start) != (PAGE_SIZE << AW_TIKTAP_MMAP_PAGE_ORDER))) {
		aw_err("mmap size check err!");
		return -EPERM;
	}
#endif
	phys = virt_to_phys(aw_haptic->start_buf);

	ret = remap_pfn_range(vma, vma->vm_start, (phys >> PAGE_SHIFT),
			      (vma->vm_end - vma->vm_start), vma->vm_page_prot);
	if (ret) {
		aw_err("mmap failed!");
		return ret;
	}

	aw_info("success!");

	return ret;
}

static int tiktap_file_open(struct inode *inode, struct file *file)
{
#ifdef AW_DOUBLE
	file->private_data = (void *)left;
#else
	file->private_data = (void *)g_aw_haptic;
#endif
	return 0;
}

#ifdef AW_DOUBLE
static int tiktap_file_open_r(struct inode *inode, struct file *file)
{
	file->private_data = (void *)right;
	return 0;
}
#endif

#ifdef KERNEL_OVER_5_10
static const struct proc_ops tiktap_proc_ops = {
	.proc_mmap = tiktap_file_mmap,
	.proc_open = tiktap_file_open,
	.proc_ioctl = tiktap_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.proc_compat_ioctl = tiktap_compat_ioctl,
#endif
};
#ifdef AW_DOUBLE
static const struct proc_ops tiktap_proc_ops_r = {
	.proc_mmap = tiktap_file_mmap,
	.proc_open = tiktap_file_open_r,
	.proc_ioctl = tiktap_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.proc_compat_ioctl = tiktap_compat_ioctl,
#endif
};
#endif
#else
static const struct file_operations tiktap_proc_ops = {
	.owner = THIS_MODULE,
	.mmap = tiktap_file_mmap,
	.open = tiktap_file_open,
	.unlocked_ioctl = tiktap_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = tiktap_compat_ioctl,
#endif
};
#ifdef AW_DOUBLE
static const struct file_operations tiktap_proc_ops_r = {
	.owner = THIS_MODULE,
	.mmap = tiktap_file_mmap,
	.open = tiktap_file_open_r,
	.unlocked_ioctl = tiktap_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = tiktap_compat_ioctl,
#endif
};
#endif
#endif

static int tiktap_init(struct aw_haptic *aw_haptic)
{
	struct mmap_buf_format *tiktap_start_buf;
	struct proc_dir_entry *tiktap_config_proc = NULL;

	/* Create proc file node */
#ifdef AW_DOUBLE
	if (of_device_is_compatible(aw_haptic->i2c->dev.of_node, "awinic,haptic_hv_l"))
		tiktap_config_proc = proc_create(AW_TIKTAP_PROCNAME, 0664, NULL, &tiktap_proc_ops);

	if (of_device_is_compatible(aw_haptic->i2c->dev.of_node, "awinic,haptic_hv_r"))
		tiktap_config_proc = proc_create(AW_TIKTAP_PROCNAME_R, 0664, NULL, &tiktap_proc_ops_r);
#else
	tiktap_config_proc = proc_create(AW_TIKTAP_PROCNAME, 0664, NULL, &tiktap_proc_ops);
#endif
	if (tiktap_config_proc == NULL) {
		aw_err("create proc file failed!");
		return -EPERM;
	}
	aw_info("create proc file success!");

	/* Construct shared memory */
	tiktap_start_buf = (struct mmap_buf_format *)__get_free_pages(GFP_KERNEL, AW_TIKTAP_MMAP_PAGE_ORDER);
	if (tiktap_start_buf == NULL) {
		aw_err("Error __get_free_pages failed");
		return -ENOMEM;
	}
	SetPageReserved(virt_to_page(tiktap_start_buf));
	{
		struct mmap_buf_format *temp;
		unsigned int i = 0;

		temp = tiktap_start_buf;
		for (i = 1; i < AW_TIKTAP_MMAP_BUF_SUM; i++) {
			temp->kernel_next = (tiktap_start_buf + i);
			temp = temp->kernel_next;
		}
		temp->kernel_next = tiktap_start_buf;

		temp = tiktap_start_buf;
		for (i = 0; i < AW_TIKTAP_MMAP_BUF_SUM; i++) {
			temp->bit = i;
			temp = temp->kernel_next;
		}
	}

	aw_haptic->aw_config_proc = tiktap_config_proc;
	aw_haptic->start_buf = tiktap_start_buf;
	/* init flag and work */
	aw_haptic->tiktap_stop_flag = true;
	aw_haptic->tiktap_ready = false;
	INIT_WORK(&aw_haptic->tiktap_work, tiktap_work_routine);

	return 0;
}
#endif

static int vibrator_init(struct aw_haptic *aw_haptic)
{
	int ret = 0;

	aw_info("enter");

#ifdef TIMED_OUTPUT
	aw_info("TIMED_OUT FRAMEWORK!");
#ifdef AW_DOUBLE
	ret = memcmp(aw_haptic->name, "left", sizeof("left"));
	if (!ret)
		aw_haptic->vib_dev.name = "vibrator_l";
	ret = memcmp(aw_haptic->name, "right", sizeof("right"));
	if (!ret)
		aw_haptic->vib_dev.name = "vibrator_r";
#else
	aw_haptic->vib_dev.name = "vibrator";
#endif
	aw_haptic->vib_dev.get_time = vibrator_get_time;
	aw_haptic->vib_dev.enable = vibrator_enable;

	ret = timed_output_dev_register(&(aw_haptic->vib_dev));
	if (ret < 0) {
		aw_err("fail to create timed output dev");
		return ret;
	}
	ret = sysfs_create_group(&aw_haptic->vib_dev.dev->kobj, &vibrator_attribute_group);
	if (ret < 0) {
		aw_err("error creating sysfs attr files");
		return ret;
	}
#else
	aw_info("loaded in leds_cdev framework!");
#ifdef AW_DOUBLE
	ret = memcmp(aw_haptic->name, "left", sizeof("left"));
	if (!ret)
		aw_haptic->vib_dev.name = "vibrator_l";
	ret = memcmp(aw_haptic->name, "right", sizeof("right"));
	if (!ret)
		aw_haptic->vib_dev.name = "vibrator_r";
#else
#ifdef KERNEL_OVER_5_10
	aw_haptic->vib_dev.name = "aw_vibrator";
#else
	aw_haptic->vib_dev.name = "vibrator";
#endif
#endif
	aw_haptic->vib_dev.brightness_get = brightness_get;
	aw_haptic->vib_dev.brightness_set = brightness_set;
	ret = devm_led_classdev_register(&aw_haptic->i2c->dev, &aw_haptic->vib_dev);
	if (ret < 0) {
		aw_err("fail to create led dev");
		return ret;
	}
	ret = sysfs_create_group(&aw_haptic->vib_dev.dev->kobj, &vibrator_attribute_group);
	if (ret < 0) {
		aw_err("error creating sysfs attr files");
		return ret;
	}
#endif
	hrtimer_init(&aw_haptic->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	aw_haptic->timer.function = vibrator_timer_func;
	INIT_WORK(&aw_haptic->vibrator_work, vibrator_work_routine);
	INIT_WORK(&aw_haptic->rtp_work, rtp_work_routine);
	mutex_init(&aw_haptic->lock);
	mutex_init(&aw_haptic->rtp_lock);
	mutex_init(&aw_haptic->dual_lock);
	sema_init(&aw_haptic->sema, 1);
	atomic_set(&aw_haptic->is_in_write_loop, 0);
	atomic_set(&aw_haptic->is_in_rtp_loop, 0);
	atomic_set(&aw_haptic->exit_in_rtp_loop, 0);
#ifdef A2HAPTIC_SUPPORT
	atomic_set(&aw_haptic->is_consume_data, 0);
#endif
	init_waitqueue_head(&aw_haptic->wait_q);
	init_waitqueue_head(&aw_haptic->stop_wait_q);

	return 0;
}

static void haptic_init(struct aw_haptic *aw_haptic)
{
	aw_info("enter");
	/* haptic audio */
	aw_haptic->haptic_audio.delay_val = 1;
	aw_haptic->haptic_audio.timer_val = 21318;
	aw_haptic->rtp_num = sizeof(aw_rtp_name) / sizeof(*aw_rtp_name);
	INIT_LIST_HEAD(&(aw_haptic->haptic_audio.ctr_list));
	hrtimer_init(&aw_haptic->haptic_audio.timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	aw_haptic->haptic_audio.timer.function = audio_timer_func;
	INIT_WORK(&aw_haptic->haptic_audio.work, audio_work_routine);
	mutex_init(&aw_haptic->haptic_audio.lock);
	INIT_LIST_HEAD(&(aw_haptic->haptic_audio.list));

	/* haptic init */
	mutex_lock(&aw_haptic->lock);
	aw_haptic->bullet_nr = 0;
	aw_haptic->gun_type = 0xff;
	aw_haptic->activate_mode = aw_haptic->info.mode;
	aw_haptic->func->play_mode(aw_haptic, AW_STANDBY_MODE);
	aw_haptic->func->set_pwm(aw_haptic, AW_PWM_12K);
	/* misc value init */
	aw_haptic->func->misc_para_init(aw_haptic);
	aw_haptic->func->set_bst_peak_cur(aw_haptic);
	aw_haptic->func->set_bst_vol(aw_haptic, aw_haptic->vmax);
	aw_haptic->func->auto_bst_enable(aw_haptic, aw_haptic->info.is_enabled_auto_bst);
	aw_haptic->func->vbat_mode_config(aw_haptic, AW_CONT_VBAT_HW_COMP_MODE);
	/* f0 calibration */
	if (aw_haptic == right) {
		aw_get_sample_params(aw_haptic);
		if (left != NULL) {
			left->f0 = right->f0;
			left->f0_cali_data = right->f0_cali_data;
			left->func->upload_lra(left, AW_F0_CALI_LRA);
		}
	} else {
		if (right != NULL) {
			left->f0 = right->f0;
			left->f0_cali_data = right->f0_cali_data;
			aw_info("left haptic use right haptic f0 instead, f0 = %d.", left->f0);
			left->func->upload_lra(left, AW_F0_CALI_LRA);
		}
	}
	mutex_unlock(&aw_haptic->lock);
}

static int aw_i2c_probe(struct i2c_client *i2c)
{
	int ret = 0;
#ifdef AW_VIM3
	int irq_flags = 0;
#endif
	struct aw_haptic *aw_haptic;
	struct device_node *np = i2c->dev.of_node;

	pr_info("<%s>%s: enter\n", AW_I2C_NAME, __func__);
	if (!i2c_check_functionality(i2c->adapter, I2C_FUNC_I2C)) {
		pr_err("<%s>%s: check_functionality failed\n", AW_I2C_NAME, __func__);
		return -EIO;
	}

	xm_hap_driver_init(true);
	aw_haptic = devm_kzalloc(&i2c->dev, sizeof(struct aw_haptic), GFP_KERNEL);
	if (aw_haptic == NULL)
		return -ENOMEM;

	aw_haptic->dev = &i2c->dev;
	aw_haptic->i2c = i2c;

	i2c_set_clientdata(i2c, aw_haptic);
	dev_set_drvdata(&i2c->dev, aw_haptic);
#ifdef AW_INPUT_FRAMEWORK
	ret = input_framework_init(aw_haptic);
	if (ret < 0){
		XM_HAP_REGISTER_EXCEPTION("Device register", "input_register_device");
		return ret;
	}
#endif
	/* aw_haptic rst & int */
	if (np) {
		ret = parse_dt_gpio(&i2c->dev, aw_haptic, np);
		if (ret) {
			aw_err("failed to parse gpio");
			XM_HAP_REGISTER_EXCEPTION("DT", "haptics_parse_dt");
			return ret;
		}
	} else {
		aw_haptic->reset_gpio = -1;
		aw_haptic->irq_gpio = -1;
	}
    if (gpio_is_valid(aw_haptic->reset_gpio)) {
        ret = devm_gpio_request_one(&i2c->dev, aw_haptic->reset_gpio,
                        GPIOF_OUT_INIT_LOW, "aw_rst");
        if (ret) {
            aw_err("rst request failed");
            return ret;
        }
    }

#ifdef AW_ENABLE_PIN_CONTROL
	aw_haptic->pinctrl = devm_pinctrl_get(&i2c->dev);
	if (IS_ERR(aw_haptic->pinctrl)) {
		if (PTR_ERR(aw_haptic->pinctrl) == -EPROBE_DEFER) {
			aw_err("pinctrl not ready");
			ret = -EPROBE_DEFER;
			return ret;
		}
		aw_err("Target does not use pinctrl");
		aw_haptic->pinctrl = NULL;
		ret = -EINVAL;
		return ret;
	}
	aw_haptic->pinctrl_state = pinctrl_lookup_state(aw_haptic->pinctrl, "aw86938_irq_active");
	if (IS_ERR(aw_haptic->pinctrl_state)) {
		aw_err("cannot find pinctrl state");
		ret = -EINVAL;
		return ret;
	}
	pinctrl_select_state(aw_haptic->pinctrl, aw_haptic->pinctrl_state);
#endif
#ifndef AW_VIM3
    if (gpio_is_valid(aw_haptic->irq_gpio)) {
        ret = devm_gpio_request_one(&i2c->dev, aw_haptic->irq_gpio, GPIOF_DIR_IN, "aw_int");
        if (ret) {
            aw_err("int request failed");
            return ret;
        }
    }
#else
    irq_flags = IRQF_TRIGGER_FALLING | IRQF_ONESHOT;
    ret = devm_request_threaded_irq(&i2c->dev, i2c->irq,
                    NULL, irq_handle, irq_flags,
                    i2c->name, aw_haptic);
    if (ret) {
        dev_err(&i2c->dev, "Unable to request IRQ.\n");
        return ret;
    }
#endif

	/* aw func ptr init */
	ret = ctrl_init(aw_haptic);
	if (ret < 0) {
		aw_err("ctrl_init failed ret=%d", ret);
		return ret;
	}

	ret = aw_haptic->func->check_qualify(aw_haptic);
	if (ret < 0) {
		aw_err("qualify check failed ret=%d", ret);
		return ret;
	}

	/* aw_haptic chip id */
	ret = parse_chipid(aw_haptic);
	if (ret < 0) {
		aw_err("parse chipid failed ret=%d", ret);
		return ret;
	}

	aw_haptic->func->parse_dt(&i2c->dev, aw_haptic, np);

	sw_reset(aw_haptic);
	if (!aw_haptic->info.cont_smart_loop) {
		ret = aw_haptic->func->offset_cali(aw_haptic);
		if (ret < 0)
			sw_reset(aw_haptic);
	} else {
		aw_info("open samrt loop");
	}

#ifdef AW_SND_SOC_CODEC
	aw_haptic->func->snd_soc_init(&i2c->dev);
#endif

	/* aw_haptic irq */
#ifndef AW_VIM3
    ret = irq_config(&i2c->dev, aw_haptic);
    if (ret != 0) {
        aw_err("irq_config failed ret=%d", ret);
        return ret;
    }
#else
	aw_haptic->func->interrupt_setup(aw_haptic);
#endif

#ifdef AW_TIKTAP
	g_aw_haptic = aw_haptic;
	ret = tiktap_init(aw_haptic);
	if (ret) {
		aw_err("tiktap_init failed ret = %d", ret);
		return ret;
	}
#endif
	CUSTOME_WAVE_ID = aw_haptic->info.effect_max;
	vibrator_init(aw_haptic);
	haptic_init(aw_haptic);
	aw_haptic->work_queue = create_singlethread_workqueue("aw_haptic_vibrator_work_queue");
	if (!aw_haptic->work_queue) {
		aw_err("Error creating aw_haptic_vibrator_work_queue");
		return -ERANGE;
	}
	aw_haptic->func->creat_node(aw_haptic);
	ram_work_init(aw_haptic);
	ret =  create_rb();
	if (ret < 0) {
		aw_info("error creating ringbuffer, ret = %d.\n", ret);
		XM_HAP_REGISTER_EXCEPTION("Hardware", "haptics_hw_init");
		return ret;
	}
#ifdef A2HAPTIC_SUPPORT
	if(aw_haptic->i2c->addr == 0x5B) {
		vir_aw_haptic = aw_haptic;
		aw_info("vir_aw_haptic probe completed successfully!");
	}
#endif
	aw_info("probe completed successfully!");
#ifdef AAC_RICHTAP_SUPPORT
	aw_haptic->rtp_ptr = kmalloc(RICHTAP_MMAP_BUF_SIZE * RICHTAP_MMAP_BUF_SUM, GFP_KERNEL);
	if(aw_haptic->rtp_ptr == NULL) {
		dev_err(&i2c->dev, "malloc rtp memory failed\n");
		goto richtap_err1;
	}

	aw_haptic->start_buf = (struct mmap_buf_format *)__get_free_pages(GFP_KERNEL, RICHTAP_MMAP_PAGE_ORDER);
	if(aw_haptic->start_buf == NULL) {
		dev_err(&i2c->dev, "Error __get_free_pages failed\n");
		goto richtap_err2;
	}
	SetPageReserved(virt_to_page(aw_haptic->start_buf));
	{
		struct mmap_buf_format *temp;
		uint32_t i = 0;
		temp = aw_haptic->start_buf;
		for( i = 1; i < RICHTAP_MMAP_BUF_SUM; i++) {
			temp->kernel_next = (aw_haptic->start_buf + i);
			temp = temp->kernel_next;
		}
		temp->kernel_next = aw_haptic->start_buf;

		//add by tiktap
		temp = aw_haptic->start_buf;
		for (i = 0; i < RICHTAP_MMAP_BUF_SUM; i++) {
			temp->bit = i;
			temp = temp->kernel_next;
		}
	}
	aw_haptic->tiktap_stop_flag = true;
	aw_haptic->tiktap_ready = false;
	INIT_WORK(&aw_haptic->richtap_rtp_work, richtap_rtp_work);
	atomic_set(&aw_haptic->richtap_rtp_mode, false);

#ifdef AW_DOUBLE
	if(of_device_is_compatible(np, "awinic,haptic_hv_l")) {
		ret = misc_register(&richtap_misc);
		if (ret) {
			aw_err("misc dev L register failed ret=%d", ret);
		} else {
			aw_err("misc dev L register success!");
		}
	}

	if(of_device_is_compatible(np, "awinic,haptic_hv_r")) {
		misc_register(&richtap_misc_x);
		if (ret) {
			aw_err("misc dev R register failed ret=%d", ret);
		} else {
			aw_err("misc dev R register success!");
		}
	}
#else
   	ret = misc_register(&richtap_misc);
#endif

	//dev_set_drvdata(richtap_misc.this_device, aw_haptic);
	g_aw_haptic = aw_haptic;
#endif
	return ret;
#ifdef AAC_RICHTAP_SUPPORT
richtap_err2:
	kfree(aw_haptic->rtp_ptr);
richtap_err1:
    	devm_free_irq(&i2c->dev, gpio_to_irq(aw_haptic->irq_gpio), aw_haptic);
#endif
	return ret;
}

#ifdef KERNEL_OVER_6_1
static void aw_remove(struct i2c_client *i2c)
#else
static int aw_remove(struct i2c_client *i2c)
#endif
{
	struct aw_haptic *aw_haptic = i2c_get_clientdata(i2c);

#ifdef DUAL_RTP_TEST
	if (aw_haptic == right)
		right->i2c->addr = right->record_i2c_addr;
#endif
	aw_info("enter");
#ifdef TIMED_OUTPUT
	timed_output_dev_unregister(&aw_haptic->vib_dev);
#else
	devm_led_classdev_unregister(&aw_haptic->i2c->dev, &aw_haptic->vib_dev);
#endif
#ifdef AW_INPUT_FRAMEWORK
	cancel_work_sync(&aw_haptic->gain_work);
	cancel_work_sync(&aw_haptic->input_vib_work);
	input_unregister_device(aw_haptic->input_dev);
	input_ff_destroy(aw_haptic->input_dev);
#endif
	cancel_delayed_work_sync(&aw_haptic->ram_work);
	cancel_work_sync(&aw_haptic->haptic_audio.work);
	hrtimer_cancel(&aw_haptic->haptic_audio.timer);
	cancel_work_sync(&aw_haptic->rtp_work);
	cancel_work_sync(&aw_haptic->vibrator_work);
	hrtimer_cancel(&aw_haptic->timer);
	mutex_destroy(&aw_haptic->lock);
	mutex_destroy(&aw_haptic->rtp_lock);
	mutex_destroy(&aw_haptic->haptic_audio.lock);
	destroy_workqueue(aw_haptic->work_queue);
#ifdef AW_SND_SOC_CODEC
#ifdef KERNEL_OVER_4_19
	snd_soc_unregister_component(&i2c->dev);
#else
	snd_soc_unregister_codec(&i2c->dev);
#endif
#endif
#ifdef AAC_RICHTAP_SUPPORT
	cancel_work_sync(&aw_haptic->richtap_rtp_work);
	kfree(aw_haptic->rtp_ptr);
	ClearPageReserved(virt_to_page(aw_haptic->start_buf));
	free_pages((unsigned long)aw_haptic->start_buf, RICHTAP_MMAP_PAGE_ORDER);
#ifdef AW_DOUBLE
	if(of_device_is_compatible(i2c->dev.of_node, "awinic,haptic_hv_l")) {
		misc_deregister(&richtap_misc);
	}
	if(of_device_is_compatible(i2c->dev.of_node, "awinic,haptic_hv_r")) {
		misc_deregister(&richtap_misc_x);
	}
#else
   	 misc_deregister(&richtap_misc);

#endif
#endif

#ifdef AW_TIKTAP
	cancel_work_sync(&aw_haptic->tiktap_work);
	ClearPageReserved(virt_to_page(aw_haptic->start_buf));
	free_pages((unsigned long)aw_haptic->start_buf, AW_TIKTAP_MMAP_PAGE_ORDER);
	aw_haptic->start_buf = NULL;
	proc_remove(aw_haptic->aw_config_proc);
	aw_haptic->aw_config_proc = NULL;
#endif
	release_rb();

#ifndef KERNEL_OVER_6_1
	return 0;
#endif
}

static int aw_i2c_suspend(struct device *dev)
{
	int ret = 0;
	struct aw_haptic *aw_haptic = dev_get_drvdata(dev);

	mutex_lock(&aw_haptic->lock);
	aw_haptic->func->play_stop(aw_haptic);
	mutex_unlock(&aw_haptic->lock);

	return ret;
}

static int aw_i2c_resume(struct device *dev)
{
	int ret = 0;

	return ret;
}

static SIMPLE_DEV_PM_OPS(aw_pm_ops, aw_i2c_suspend, aw_i2c_resume);

static const struct i2c_device_id aw_i2c_id[] = {
	{AW_I2C_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, aw_i2c_id);

static const struct of_device_id aw_dt_match[] = {
#ifdef AW_DOUBLE
	{.compatible = "awinic,haptic_hv_r"},
	{.compatible = "awinic,haptic_hv_l"},
#else
	{.compatible = "awinic,haptic_hv"},
#endif
	{},
};

static struct i2c_driver aw_i2c_driver = {
	.driver = {
		   .name = AW_I2C_NAME,
		   .owner = THIS_MODULE,
		   .of_match_table = of_match_ptr(aw_dt_match),
#ifdef CONFIG_PM_SLEEP
		   .pm = &aw_pm_ops,
#endif
		   },
	.probe = aw_i2c_probe,
	.remove = aw_remove,
	.id_table = aw_i2c_id,
};

static int __init aw_i2c_init(void)
{
	int ret = 0;

	pr_info("<%s>%s: aw_haptic driver version %s\n", AW_I2C_NAME, __func__,
		HAPTIC_HV_DRIVER_VERSION);
	ret = i2c_add_driver(&aw_i2c_driver);
	if (ret) {
		pr_err("<%s>%s: fail to add aw_haptic device into i2c\n", AW_I2C_NAME, __func__);
		return ret;
	}

	return 0;
}
module_init(aw_i2c_init);

static void __exit aw_i2c_exit(void)
{
	i2c_del_driver(&aw_i2c_driver);
}
module_exit(aw_i2c_exit);

MODULE_DESCRIPTION("AWINIC Haptic Driver");
MODULE_LICENSE("GPL v2");
