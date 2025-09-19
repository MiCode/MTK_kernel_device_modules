#include <linux/backlight.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>
#include <drm/drm_modes.h>
#include <linux/delay.h>
#include <drm/drm_connector.h>
#include <drm/drm_device.h>
#include "../mediatek/mediatek_v2/drm_notifier_odm.h"
#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>
#include <video/mipi_display.h>
#include <video/of_videomode.h>
#include <video/videomode.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/of_graph.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "../mediatek/mediatek_v2/mtk_panel_ext.h"
#include "../mediatek/mediatek_v2/mtk_drm_graphics_base.h"
#include "../mediatek/mediatek_v2/mtk_log.h"
#endif

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
#include "../mediatek/mediatek_v2/mtk_corner_pattern/mtk_data_hw_roundedpattern.h"
#endif
#include <linux/atomic.h>

#define REGFLAG_DELAY 0xFFFC
#define REGFLAG_UDELAY 0xFFFB
#define REGFLAG_END_OF_TABLE 0xFFFD
#define REGFLAG_RESET_LOW 0xFFFE
#define REGFLAG_RESET_HIGH 0xFFFF

#define DATA_RATE1		1242

#define MODE2_FPS		120
#define MODE2_HFP		40
#define MODE2_HSA		4
#define MODE2_HBP		20
#define MODE2_VFP		330
#define MODE2_VSA		4
#define MODE2_VBP		32
#define MODE1_FPS		90
#define MODE1_HFP		40
#define MODE1_HSA		4
#define MODE1_HBP		20
#define MODE1_VFP		986
#define MODE1_VSA		4
#define MODE1_VBP		32
#define MODE0_FPS		60
#define MODE0_HFP		40
#define MODE0_HSA		4
#define MODE0_HBP		20
#define MODE0_VFP		2296
#define MODE0_VSA		4
#define MODE0_VBP		32
#define FRAME_WIDTH (720)
#define	FRAME_HEIGHT (1600)

extern bool esd_panel_dead;
bool lcd_reset_keep_high_4th = false;
void set_lcd_reset_gpio_keep_high_4th(bool en)
{
	lcd_reset_keep_high_4th = en;
}
EXPORT_SYMBOL(set_lcd_reset_gpio_keep_high_4th);

bool prox_lcd_reset_keep_high4 = false;
void set_prox_lcd_reset_gpio_keep_high4(bool en)
{
	prox_lcd_reset_keep_high4 = en;
}
EXPORT_SYMBOL(set_prox_lcd_reset_gpio_keep_high4);

static int current_fps = 90;
static struct drm_notifier_data z_notify_data;
extern int lcd_bias_write_bytes(unsigned char addr, unsigned char value);
static int curr_bl1;
static int curr_bl2;
static bool panel_inited = false;


struct lcm {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *enp_gpio;
	struct gpio_desc *enn_gpio;

	bool prepared;
	bool enabled;

	unsigned int dynamic_fps;

	int error;
};

struct LCM_setting_table {
	unsigned int cmd;
	unsigned char count;
	unsigned char para_list[200];
};

static struct lcm *panel_ctx;

#define LCM_DRIVER_NAME "dsi_panel_p15a_36_0f_0d_dsc_vdo"
static const char *panel_info = "panel_name=dsi_panel_p15a_36_0f_0d_dsc_vdo";

#define lcm_dcs_write_seq(ctx, seq...) \
({\
	const u8 d[] = { seq };\
	BUILD_BUG_ON_MSG(ARRAY_SIZE(d) > 64, "DCS sequence too big for stack");\
	lcm_dcs_write(ctx, d, ARRAY_SIZE(d));\
})
#define lcm_dcs_write_seq_static(ctx, seq...) \
({\
	static const u8 d[] = { seq };\
	lcm_dcs_write(ctx, d, ARRAY_SIZE(d));\
})

static inline struct lcm *panel_to_lcm(struct drm_panel *panel)
{
	return container_of(panel, struct lcm, panel);
}

static void lcm_dcs_write(struct lcm *ctx, const void *data, size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	ssize_t ret;
	char *addr;
	if (ctx->error < 0)
		return;
	addr = (char *)data;
	if ((int)*addr < 0xB0)
		ret = mipi_dsi_dcs_write_buffer(dsi, data, len);
	else
		ret = mipi_dsi_generic_write(dsi, data, len);
	if (ret < 0) {
		dev_err(ctx->dev, "error %zd writing seq: %ph\n", ret, data);
		ctx->error = ret;
	}
}

#ifdef PANEL_SUPPORT_READBACK
static int lcm_dcs_read(struct lcm *ctx, u8 cmd, void *data, size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	ssize_t ret;
	if (ctx->error < 0)
		return 0;
	ret = mipi_dsi_dcs_read(dsi, cmd, data, len);
	if (ret < 0) {
		dev_err(ctx->dev, "error %d reading dcs seq:(%#x)\n", ret, cmd);
		ctx->error = ret;
	}
	return ret;
}

static void lcm_panel_get_data(struct lcm *ctx)
{
	u8 buffer[3] = {0};
	static int ret;
	if (ret == 0) {
		ret = lcm_dcs_read(ctx,  0x0A, buffer, 1);
		dev_info(ctx->dev, "return %d data(0x%08x) to dsi engine\n",
			 ret, buffer[0] | (buffer[1] << 8));
	}
}
#endif


static struct LCM_setting_table init_setting[] = {
	{0xF0,0x03,{0x99,0x16,0x18}},
	{0xBC,0x0E,{0xFF,0xF0,0x05,0x00,0x00,0x00,0x00,0x04,0x00,0x05,0x08,0xAB,0x08,0x5B}},
	{0xBD,0x04,{0xA1,0x2A,0x52,0xA6}},
	{0xC2,0x02,{0x70,0x45}},
	{0xE0,0x20,{0x3A,0xF0,0x3B,0x36,0x3B,0xB8,0x3C,0x29,0x3C,0x8C,0x3D,0x39,0x3D,0xB2,0x3E,0x35,0x3E,0xA8,0x3F,0x30,0x3F,0x70,0x3F,0x90,0x3F,0x90,0x3F,0x90,0x3F,0x90,0x3F,0x90}},
	{0xE1,0x20,{0x2B,0xBB,0x2D,0x3F,0x2E,0x3F,0x2F,0xFF,0x31,0x47,0x32,0x8F,0x33,0xD6,0x35,0x1E,0x37,0x66,0x38,0xEF,0x3A,0x78,0x3C,0x01,0x3D,0x8A,0x3E,0xB4,0x3F,0x28,0x3F,0x88}},
	{0xE2,0x20,{0x2C,0xDD,0x2E,0x3F,0x2F,0x4F,0x30,0x7F,0x31,0x77,0x33,0xEF,0x35,0xC6,0x37,0x2E,0x38,0x86,0x3A,0x0A,0x3B,0x20,0x3C,0x31,0x3D,0x9A,0x3E,0xB4,0x3F,0x28,0x3F,0x88}},
	{0xE3,0x20,{0xED,0xF5,0xF7,0xF8,0xF9,0xFA,0xFB,0xFD,0xFE,0xFE,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xEC,0xEC,0xEC,0xEC,0xEC,0xEC,0xED,0xEF,0xF3,0xF5,0xF7,0xF9,0xFC,0xFE,0xFF,0xFF}},
	{0xE4,0x20,{0xEF,0xF0,0xF0,0xF2,0xF3,0xF4,0xF4,0xF5,0xF5,0xF8,0xFA,0xFB,0xFD,0xFE,0xFF,0xFF,0x0F,0x1F,0x2F,0x3F,0x4F,0x5F,0x6F,0x7F,0x90,0xA0,0xB0,0xC0,0xD0,0xE0,0xF0,0x00}},
	{0xE5,0x20,{0x8C,0x00,0x10,0x00,0x00,0x00,0x00,0x00,0x00,0x16,0x26,0x9F,0x00,0xB0,0x00,0x04,0x00,0x00,0x04,0x84,0x06,0x12,0x23,0x11,0xC7,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
	{0x53,0x01,{0x2C}},
	{0x35,0x01,{0x00}},
	//{0x51,0x02,{0x07,0xff}},
	{0x5e,0x02,{0x00,0x0D}},
	{0x11,0x02,{0x00,0x00}},
	//delay,120
	{REGFLAG_DELAY,100,{}},
	{0x29,0x02,{0x00,0x00}},
	//delay,20
	{REGFLAG_DELAY,20,{}},
	{0x6D,0x01,{0x02}},
	{0xBD,0x04,{0xA1,0xA2,0x52,0xAE}},
	{0xF0,0x03,{0x00,0x00,0x00}},
};

static void push_table(struct lcm *ctx, struct LCM_setting_table *table, unsigned int count)
{
	unsigned int i, j;
	unsigned char temp[255] = {0};
	for (i = 0; i < count; i++) {
		unsigned int cmd;
		cmd = table[i].cmd;
		memset(temp, 0, sizeof(temp));
		switch (cmd) {
		case REGFLAG_DELAY:
			msleep(table[i].count);
			break;
		case REGFLAG_END_OF_TABLE:
			break;
		default:
			temp[0] = cmd;
			for (j = 0; j < table[i].count; j++)
				temp[j+1] = table[i].para_list[j];
			lcm_dcs_write(ctx, temp, table[i].count+1);
		}
	}
}

static int lcm_disable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	if (!ctx->enabled)
		return 0;
	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_POWERDOWN;
		backlight_update_status(ctx->backlight);
	}
	ctx->enabled = false;
	return 0;
}

static int lcm_unprepare(struct drm_panel *panel)
{
	int blank;
	struct lcm *ctx = panel_to_lcm(panel);
	if (!ctx->prepared)
		return 0;

	ctx->error = 0;
	ctx->prepared = false;

	pr_info("%s: +\n", __func__);

	blank = DRM_BLANK_POWERDOWN;
	z_notify_data.data = &blank;
	drm_notifier_call_chain(DRM_EVENT_BLANK, &z_notify_data);
	pr_err("[XMFP] : %s ++++ blank = DRM_BLANK_POWERDOWN ++++", __func__);

	lcm_dcs_write_seq_static(ctx, 0x6D, 0x25);
	lcm_dcs_write_seq_static(ctx, 0x28, 0x00, 0x00);
	msleep(20);
	if (prox_lcd_reset_keep_high4 && esd_panel_dead == false) {
		pr_info("[LCM]%s:prox_lcd_reset_keep_high4 = %d\n",
			__func__, prox_lcd_reset_keep_high4);
		return 0;
	}
	lcm_dcs_write_seq_static(ctx, 0x10, 0x00, 0x00);
	msleep(120);
	if (lcd_reset_keep_high_4th && esd_panel_dead == false) {
		pr_info("[LCM]%s:tp_gesture_en\n", __func__);
		return 0;
	}
	ctx->enn_gpio = devm_gpiod_get(ctx->dev, "enn", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->enn_gpio)) {
		dev_err(ctx->dev, "%s: cannot get enn_gpio %ld\n", __func__, PTR_ERR(ctx->enn_gpio));
		return PTR_ERR(ctx->enn_gpio);
	}
	gpiod_set_value(ctx->enn_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->enn_gpio);
	udelay(5000);
	ctx->enp_gpio = devm_gpiod_get(ctx->dev, "enp", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->enp_gpio)) {
		dev_err(ctx->dev, "%s: cannot get enp_gpio %ld\n", __func__, PTR_ERR(ctx->enp_gpio));
		return PTR_ERR(ctx->enp_gpio);
	}
	gpiod_set_value(ctx->enp_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->enp_gpio);
	udelay(5000);

	pr_info("%s: -\n", __func__);
	return 0;
}
static void lcm_panel_init(struct lcm *ctx)
{
	pr_info("%s: +\n", __func__);
	if (ctx->prepared) {
		pr_info("%s: panel has been prepared, nothing to do!\n", __func__);
		goto err;
	}
	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		goto err;
	}
	udelay(15 * 1000);
	gpiod_set_value(ctx->reset_gpio, 0);
	udelay(5 * 1000);
	gpiod_set_value(ctx->reset_gpio, 1);
	msleep(30);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	push_table(ctx, init_setting,
		sizeof(init_setting) / sizeof(struct LCM_setting_table));
	ctx->prepared = true;
err:
	pr_info("%s: -\n", __func__);
}

static int lcm_prepare(struct drm_panel *panel)
{
	int blank;
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;
	unsigned char cmd = 0x00;
	unsigned char data = 0xFF;
	esd_panel_dead = false ;
	pr_info("%s\n", __func__);
	if (ctx->prepared)
		return 0;

	ctx->enp_gpio = devm_gpiod_get(ctx->dev, "enp", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->enp_gpio)) {
		dev_err(ctx->dev, "%s: cannot get enp_gpio %ld\n", __func__, PTR_ERR(ctx->enp_gpio));
		return PTR_ERR(ctx->enp_gpio);
	}
	gpiod_set_value(ctx->enp_gpio, 1);
	devm_gpiod_put(ctx->dev, ctx->enp_gpio);

	/* set AVDD 6.0V*/
	cmd = 0x00;
	data = 0x14;
	ret = lcd_bias_write_bytes(cmd, data);
	if (ret < 0)
		pr_info("icnl9916--kernel--bias----cmd=%0x--i2c write error----\n", cmd);
	else
		pr_info("icnl9916--kernel--bias----cmd=%0x--i2c write success----\n", cmd);

	udelay(5 * 1000);

	ctx->enn_gpio = devm_gpiod_get(ctx->dev, "enn", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->enn_gpio)) {
		dev_err(ctx->dev, "%s: cannot get enn_gpio %ld\n", __func__, PTR_ERR(ctx->enn_gpio));
		return PTR_ERR(ctx->enn_gpio);
	}
	gpiod_set_value(ctx->enn_gpio, 1);
	devm_gpiod_put(ctx->dev, ctx->enn_gpio);
	/* set AVEE -6.0V*/
	cmd = 0x01;
	data = 0x14;
	ret = lcd_bias_write_bytes(cmd, data);
	if (ret < 0)
		pr_info("icnl9916--kernel--bias----cmd=%0x--i2c write error----\n", cmd);
	else
		pr_info("icnl9916--kernel--bias----cmd=%0x--i2c write success----\n", cmd);

	lcm_panel_init(ctx);
	ret = ctx->error;
	if (ret < 0){
		lcm_unprepare(panel);
		pr_info("%s error lcm_unprepare\n", __func__);
	}
	ctx->prepared = true;

	blank = DRM_BLANK_UNBLANK;
	z_notify_data.data = &blank;
	drm_notifier_call_chain(DRM_EVENT_BLANK, &z_notify_data);
	pr_err("[XMFP] : %s ++++ blank = DRM_BLANK_UNBLANK ++++", __func__);
#ifdef PANEL_SUPPORT_READBACK
	lcm_panel_get_data(ctx);
#endif

	return ret;
}

static int lcm_enable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
  	pr_info("%s: +\n", __func__);
	if (ctx->enabled)
		return 0;
	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_UNBLANK;
		backlight_update_status(ctx->backlight);
	}
	ctx->enabled = true;
  	pr_info("%s: -\n", __func__);
	return 0;
}

static const struct drm_display_mode mode_60hz = {
	.clock = (FRAME_WIDTH + MODE0_HFP + MODE0_HSA + MODE0_HBP)*(FRAME_HEIGHT + MODE0_VFP + MODE0_VSA + MODE0_VBP)*60/1000,//h_total * v_total * fps 60hz; unit:KHz
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + MODE0_HFP,
	.hsync_end = FRAME_WIDTH + MODE0_HFP + MODE0_HSA,
	.htotal = FRAME_WIDTH + MODE0_HFP + MODE0_HSA + MODE0_HBP,
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + MODE0_VFP,
	.vsync_end = FRAME_HEIGHT + MODE0_VFP + MODE0_VSA,
	.vtotal = FRAME_HEIGHT + MODE0_VFP + MODE0_VSA + MODE0_VBP,
};

static const struct drm_display_mode mode_90hz = {
	.clock = (FRAME_WIDTH + MODE1_HFP + MODE1_HSA + MODE1_HBP)*(FRAME_HEIGHT + MODE1_VFP + MODE1_VSA + MODE1_VBP)*90/1000,//h_total * v_total * fps 90hz
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + MODE1_HFP,
	.hsync_end = FRAME_WIDTH + MODE1_HFP + MODE1_HSA,
	.htotal = FRAME_WIDTH + MODE1_HFP + MODE1_HSA + MODE1_HBP,
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + MODE1_VFP,
	.vsync_end = FRAME_HEIGHT + MODE1_VFP + MODE1_VSA,
	.vtotal = FRAME_HEIGHT + MODE1_VFP + MODE1_VSA + MODE1_VBP,
};

static const struct drm_display_mode default_mode = {
	.clock = (FRAME_WIDTH + MODE2_HFP + MODE2_HSA + MODE2_HBP)*(FRAME_HEIGHT + MODE2_VFP + MODE2_VSA + MODE2_VBP)*120/1000,//h_total * v_total * fps 120hz
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + MODE2_HFP,
	.hsync_end = FRAME_WIDTH + MODE2_HFP + MODE2_HSA,
	.htotal = FRAME_WIDTH + MODE2_HFP + MODE2_HSA + MODE2_HBP,
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + MODE2_VFP,
	.vsync_end = FRAME_HEIGHT + MODE2_VFP + MODE2_VSA,
	.vtotal = FRAME_HEIGHT + MODE2_VFP + MODE2_VSA + MODE2_VBP,
};

#if defined(CONFIG_MTK_PANEL_EXT)
static struct mtk_panel_params ext_params = {
	.pll_clk = DATA_RATE1 / 2,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.mi_esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0,
		.count = 1,
		.para_list[0] = 0x9c,
	},
	//.is_support_od = true,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.physical_width_um = 71928,
	.physical_height_um = 159840,
	.data_rate = DATA_RATE1,

	.dyn = {
		.switch_en = 1,
		.pll_clk = 647,
		//.vfp_lp_dyn = 2550,
		.hfp = 48,
		.hbp = 46,
		//.vfp = 1314,
	},

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	.round_corner_en = 1,
	.corner_pattern_height = ROUND_CORNER_H_TOP,
	.corner_pattern_height_bot = ROUND_CORNER_H_BOT,
	.corner_pattern_tp_size=sizeof(top_rc_pattern),
	.corner_pattern_lt_addr=(void*)top_rc_pattern,
#endif
};
#endif
struct drm_display_mode *get_mode_by_id(struct drm_connector *connector,
	unsigned int mode)
{
	struct drm_display_mode *m;
	unsigned int i = 0;
	list_for_each_entry(m, &connector->modes, head) {
		if (i == mode)
			return m;
		i++;
	}
	return NULL;
}

static int mtk_panel_ext_param_set(struct drm_panel *panel,
			struct drm_connector *connector, unsigned int mode)
{
	struct mtk_panel_ext *ext = find_panel_ext(panel);
	int ret = 0;
	int dst_fps = 0;
	struct drm_display_mode *m_dst = get_mode_by_id(connector, mode);
	pr_info("dsi_panel,m_dst=%p\n",m_dst);
	pr_info("dsi_panel,mode=%d\n",mode);
	pr_info("dsi_panel,dst_fps in m_dst=%d\n",drm_mode_vrefresh(m_dst));
	dst_fps = m_dst ? drm_mode_vrefresh(m_dst) : -EINVAL;
	if (dst_fps == MODE0_FPS)
		ext->params = &ext_params;
	else if (dst_fps == MODE1_FPS)
			ext->params = &ext_params;
	else if (dst_fps == MODE2_FPS)
			ext->params = &ext_params;
		else {
			pr_err("%s, dst_fps %d\n", __func__, dst_fps);
			ret = -EINVAL;
		}
	if (!ret)
		current_fps = dst_fps;
	return ret;
}

#define MIN_BL			7	//最低背光值
#define NORMAL_MAX_BL		2046	//normal段最高背光值
#define	MAX_BL			4095	//最高背光值

#define	MIN_REG_VALUE	13	//最低寄存器亮度值
#define NORMAL_MAX_REG	2766	//noraml段最高寄存器亮度值
#define HBM_MAX_REG	3330	//hbm段最高寄存器亮度值

static const int bl_base=500;
static int lcm_setbacklight_cmdq(void *dsi, dcs_write_gce cb,
	void *handle, unsigned int level)
{
	char bl_tb[] = {0x51, 0x07, 0xff};
	char bl_dimming_off[] = {0x53, 0x24};
	int brightness = level;

	if (level == 0) {
		panel_inited = false;
		cb(dsi, handle, bl_dimming_off, ARRAY_SIZE(bl_dimming_off));
		pr_info("%s,suspend start,bl level %d\n", __func__, level);
	} else
		panel_inited = true;

	if (level>0 && level<=MIN_REG_VALUE){
		level = MIN_REG_VALUE;
	} else if (level>MIN_REG_VALUE && level<=NORMAL_MAX_BL) {
		level = level*(NORMAL_MAX_REG-MIN_REG_VALUE)*bl_base / (NORMAL_MAX_BL-MIN_BL) + MIN_BL * bl_base - (NORMAL_MAX_REG - MIN_REG_VALUE)*bl_base/(NORMAL_MAX_BL - MIN_BL) * MIN_BL;
		level = level/bl_base;
	} else if (level >NORMAL_MAX_BL && level<=MAX_BL) {
		level = level*(HBM_MAX_REG-NORMAL_MAX_REG)*bl_base/(MAX_BL-NORMAL_MAX_BL)+NORMAL_MAX_REG*bl_base-(HBM_MAX_REG-NORMAL_MAX_REG)*bl_base/(MAX_BL-NORMAL_MAX_BL)*NORMAL_MAX_BL;
		level = level/bl_base;
	}


	bl_tb[1] = (level >> 8) & 0xFF;
	bl_tb[2] = level & 0xFF;

	pr_info("TM %s,brightness 0x%02x level %d\n", __func__, level, brightness);

	curr_bl1 = bl_tb[1];
	curr_bl2 = bl_tb[2];
	if (!cb)
		return -1;

	cb(dsi, handle, bl_tb, ARRAY_SIZE(bl_tb));
	return 0;
}

static void mode_switch_to_120(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	struct lcm *ctx = panel_to_lcm(panel);
	pr_info("%s state = %d\n", __func__, stage);
	if (stage == BEFORE_DSI_POWERDOWN) {
		ctx->dynamic_fps = 120;
	}
}

static void mode_switch_to_90(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	struct lcm *ctx = panel_to_lcm(panel);
	pr_info("%s state = %d\n", __func__, stage);
	if (stage == BEFORE_DSI_POWERDOWN) {
		ctx->dynamic_fps = 90;
	}
}

static void mode_switch_to_60(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	struct lcm *ctx = panel_to_lcm(panel);
	pr_info("%s state = %d\n", __func__, stage);
	if (stage == BEFORE_DSI_POWERDOWN) {
		ctx->dynamic_fps = 60;
	}
}

static int mode_switch(struct drm_panel *panel,
		struct drm_connector *connector, unsigned int cur_mode,
		unsigned int dst_mode, enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	int ret = 0;
	int dst_fps = 0, cur_fps = 0;
	int dst_vdisplay = 0, dst_hdisplay = 0;
	int cur_vdisplay = 0, cur_hdisplay = 0;
	bool isFpsChange = false;
	struct drm_display_mode *m_dst = get_mode_by_id(connector, dst_mode);
	struct drm_display_mode *m_cur = get_mode_by_id(connector, cur_mode);
	if (cur_mode == dst_mode)
		return ret;
	dst_fps = m_dst ? drm_mode_vrefresh(m_dst) : -EINVAL;
	dst_vdisplay = m_dst ? m_dst->vdisplay : -EINVAL;
	dst_hdisplay = m_dst ? m_dst->hdisplay : -EINVAL;
	cur_fps = m_cur ? drm_mode_vrefresh(m_cur) : -EINVAL;
	cur_vdisplay = m_cur ? m_cur->vdisplay : -EINVAL;
	cur_hdisplay = m_cur ? m_cur->hdisplay : -EINVAL;
	isFpsChange = ((dst_fps == cur_fps) && (dst_fps != -EINVAL)
			&& (cur_fps != -EINVAL)) ? false : true;
	pr_info("%s isFpsChange = %d\n", __func__, isFpsChange);
	pr_info("%s dst_mode vrefresh = %d, vdisplay = %d, vdisplay = %d\n",
		__func__, dst_fps, dst_vdisplay, dst_hdisplay);
	pr_info("%s cur_mode vrefresh = %d, vdisplay = %d, vdisplay = %d\n",
		__func__, cur_fps, cur_vdisplay, cur_hdisplay);
    pr_info("dsi_panel,dst_fps=%d\n",dst_fps);
	if (isFpsChange) {
		if (dst_fps == MODE0_FPS)
			mode_switch_to_60(panel, stage);
		else if (dst_fps == MODE1_FPS)
			mode_switch_to_90(panel, stage);
		else if (dst_fps == MODE2_FPS)
			mode_switch_to_120(panel, stage);
		else {
            mode_switch_to_90(panel, stage);
            pr_info("dsi_panel 4th,force to 90Hz=%d\n",dst_fps);
        }
	}
	return ret;
}

static struct LCM_setting_table hbm_setting[] = {
	{0x51, 02, {0xDC,0x00}},
};

static struct LCM_setting_table hbm_setting_off[] = {
	{0x51, 02, {0xBF,0x0E}},
};

int lcm_hbm_control_lc(struct drm_panel *panel, bool en)
{

	struct lcm *ctx = panel_to_lcm(panel);

	if (!ctx->prepared)
	{
		pr_err("%s, panel unprepare, hbm exit \n", __func__);
		return 0;
	}

	if(en == true){
		push_table(ctx, hbm_setting,
			sizeof(hbm_setting) / sizeof(struct LCM_setting_table));
		pr_info("%s, lcm_hbm_control_lc enable\n", __func__);
	} else{
		push_table(ctx, hbm_setting_off,
			sizeof(hbm_setting_off) / sizeof(struct LCM_setting_table));
		pr_info("%s, lcm_hbm_control_lc disable\n", __func__);
	}

	return 0;
}

#define CABC_OFF	0
#define	CABC_UI		1
#define	CABC_MOVIE	2
#define	CABC_STILL	3

/*
static struct LCM_setting_table cabc_setting_off[] = {
	{0x55, 01, {0x00}},
};

static struct LCM_setting_table cabc_setting_ui[] = {
	{0x55, 01, {0x01}},
};

static struct LCM_setting_table cabc_setting_still[] = {
	{0x55, 01, {0x02}},
};

static struct LCM_setting_table cabc_setting_movie[] = {
	{0x55, 01, {0x03}},
};
*/

int lcm_cabc_control_lc(struct drm_panel *panel, int mode)
{

	struct lcm *ctx = panel_to_lcm(panel);

	if (!panel_inited || !ctx->prepared)
	{
		pr_err("%s, panel unprepare, cabc exit \n", __func__);
		return 0;
	}

	switch (mode)
	{
	case CABC_OFF:
		lcm_dcs_write_seq_static(ctx, 0x55,0x00);
		pr_info("%s, CABC_OFF enable\n", __func__);
		break;

	case CABC_UI:
		lcm_dcs_write_seq_static(ctx, 0x55,0x01);
		pr_info("%s, CABC_UI enable\n", __func__);
		break;

	case CABC_MOVIE:
		lcm_dcs_write_seq_static(ctx, 0x55,0x03);
		pr_info("%s, CABC_MOVIE enable\n", __func__);
		break;

	case CABC_STILL:
		lcm_dcs_write_seq_static(ctx, 0x55,0x02);
		pr_info("%s, CABC_STILL enable\n", __func__);
		break;

	default:
		pr_err("%s, noting to do\n", __func__);
		break;
	}

	pr_info("%s, end \n", __func__);

	return 0;
}

static int lcm_get_panel_info(struct drm_panel *panel, char *buf)
{
	int res = 0;
	res = snprintf(buf, 64, "%s\n", panel_info);
	pr_info("[%s] panel_info: %s\n",__func__, buf);
	return res;
}

static struct LCM_setting_table esd_restore_bl[] = {
	{0x51, 02, {0x03,0xFF}},
};
static void lcm_esd_restore_backlight(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	esd_restore_bl->para_list[0] = curr_bl1;
	esd_restore_bl->para_list[1] = curr_bl2;
	push_table(ctx, esd_restore_bl,
		sizeof(esd_restore_bl) / sizeof(struct LCM_setting_table));
        pr_info("%s, lcm_esd_restore_backlight restore curr_bl1:%x curr_bl2:%x \n", __func__, curr_bl1, curr_bl2);
	pr_info("lcm_esd_restore_backlight \n");
	return;
}

static struct mtk_panel_funcs ext_funcs = {
	.ext_param_set = mtk_panel_ext_param_set,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.mode_switch = mode_switch,
	.hbm_control_lc = lcm_hbm_control_lc,
	.cabc_control_lc = lcm_cabc_control_lc,
	.get_panel_info = lcm_get_panel_info,
	.LC_esd_restore_backlight = lcm_esd_restore_backlight,
};

struct panel_desc {
	const struct drm_display_mode *modes;
	unsigned int num_modes;
	unsigned int bpc;
	struct {
		unsigned int width;
		unsigned int height;
	} size;
	struct {
		unsigned int prepare;
		unsigned int enable;
		unsigned int disable;
		unsigned int unprepare;
	} delay;
};
static int lcm_get_modes(struct drm_panel *panel, struct drm_connector *connector)
{
	struct drm_display_mode *mode0, *mode1, *mode2;

	mode0 = drm_mode_duplicate(connector->dev, &mode_60hz);
	if (!mode0) {
		dev_err(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			mode_60hz.hdisplay, mode_60hz.vdisplay,
			drm_mode_vrefresh(&mode_60hz));
		return -ENOMEM;
	}
	drm_mode_set_name(mode0);
	mode0->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode0);

	mode1 = drm_mode_duplicate(connector->dev, &mode_90hz);
	if (!mode1) {
		dev_err(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			mode_90hz.hdisplay, mode_90hz.vdisplay,
			drm_mode_vrefresh(&mode_90hz));
		return -ENOMEM;
	}
	drm_mode_set_name(mode1);
	mode1->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode1);

	mode2 = drm_mode_duplicate(connector->dev, &default_mode);
	if (!mode2) {
		dev_err(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			default_mode.hdisplay, default_mode.vdisplay,
			drm_mode_vrefresh(&default_mode));
		return -ENOMEM;
	}
	drm_mode_set_name(mode2);
	mode2->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode2);

	connector->display_info.width_mm = 70;
	connector->display_info.height_mm = 160;
	return 1;
}

static const struct drm_panel_funcs lcm_drm_funcs = {
	.disable = lcm_disable,
	.unprepare = lcm_unprepare,
	.prepare = lcm_prepare,
	.enable = lcm_enable,
	.get_modes = lcm_get_modes,
};

static ssize_t proch_mod_show(struct device *device,
			    struct device_attribute *attr,
			   char *buf)
{
	int len = 0;
	pr_info("%s, dsi_panel proch modify\n", __func__);
	len += snprintf(buf+len, PAGE_SIZE-len,
		"pll_clk: (hfp)(hbp)(hsa)(vfp)(vbp)(vsa)\n");
	pr_info("%d\n", len);
	return len;
}
static ssize_t proch_mod_store(struct device *device,
			   struct device_attribute *attr,
			   const char *buf, size_t count)
{
	int val = 0;
	int vals[7];
	pr_info("[DEBUG] %s, proch modify start\n", __func__);
	if (!strncmp(buf, "pll_clk", 7) && (sscanf(buf+7, "%d",&val) == 1)) {
		ext_params.dyn.pll_clk = val;
		pr_err("[DEBUG] pll_clk = %d\n", ext_params.dyn.pll_clk);
	} else if (!strncmp(buf, "vfp", 3) && (sscanf(buf+3, "%d",&val) == 1)) {
		ext_params.dyn.vfp = val;
		pr_err("[DEBUG] vfp = %d\n", ext_params.dyn.vfp);
	} else if (!strncmp(buf, "vbp", 3) && (sscanf(buf+3, "%d",&val) == 1)) {
		ext_params.dyn.vbp = val;
		pr_err("[DEBUG] vbp = %d\n", ext_params.dyn.vbp);
	} else if (!strncmp(buf, "vsa", 3) && (sscanf(buf+3, "%d",&val) == 1)) {
		ext_params.dyn.vsa = val;
		pr_err("[DEBUG] vsa = %d\n", ext_params.dyn.vsa);
	} else if (!strncmp(buf, "hfp", 3) && (sscanf(buf+3, "%d",&val) == 1)) {
		ext_params.dyn.hfp = val;
		pr_err("[DEBUG] hfp = %d\n", ext_params.dyn.hfp);
	} else if (!strncmp(buf, "hbp", 3) && (sscanf(buf+3, "%d",&val) == 1)) {
		ext_params.dyn.hbp = val;
		pr_err("[DEBUG] hbp = %d\n", ext_params.dyn.hbp);
	} else if (!strncmp(buf, "hsa", 3) && (sscanf(buf+3, "%d",&val) == 1)) {
		ext_params.dyn.hsa = val;
		pr_err("[DEBUG] hsa = %d\n", ext_params.dyn.hsa);
	} else if (!strncmp(buf, "set", 3) && (sscanf(buf+3, "%d",&val) == 1)) {
		val = sscanf(buf+3, " %d %d %d %d %d %d %d", &vals[0],
			&vals[1], &vals[2], &vals[3],
			&vals[4], &vals[5], &vals[6]);
		if (val <= 0 || val > 8) {
			pr_info("err\n");
		}
		ext_params.dyn.pll_clk = vals[0];
		ext_params.dyn.hfp = vals[1];
		ext_params.dyn.hbp = vals[2];
		ext_params.dyn.hsa = vals[3];
		ext_params.dyn.vfp = vals[4];
		ext_params.dyn.vbp = vals[5];
		ext_params.dyn.vsa = vals[6];
		pr_err("[DEBUG] set = %d, %d, %d, %d, %d, %d, %d\n", vals[0], vals[1], vals[2],
                       vals[3], vals[4], vals[5], vals[6]);
	}
	pr_info("[DEBUG] proch modify end = %d, %d, %d, %d, %d, %d, %d\n", ext_params.dyn.pll_clk,
                ext_params.dyn.hfp, ext_params.dyn.hbp, ext_params.dyn.hsa,
                ext_params.dyn.vfp, ext_params.dyn.vbp, ext_params.dyn.vsa);
	return count;
}
static DEVICE_ATTR_RW(proch_mod);

extern int panel_event;
static ssize_t panel_event_show(struct device *device,
			struct device_attribute *attr,
			char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", panel_event);
}

static DEVICE_ATTR_RO(panel_event);
static struct attribute *icnl9916_attrs[] = {
	&dev_attr_panel_event.attr,
	&dev_attr_proch_mod.attr,
	NULL,
};
static const struct attribute_group icnl9916_attr_group = {
	.attrs = icnl9916_attrs,
};

static int lcm_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct device_node *dsi_node, *remote_node = NULL, *endpoint = NULL;
	struct lcm *ctx;
	struct device_node *backlight;

	struct device_node *chosen = NULL;
	const char *bootargs = NULL;
	int ret;
	pr_info("%s,dsi_panel 0d,enter\n", __func__);
	dsi_node = of_get_parent(dev->of_node);
	if (dsi_node) {
		endpoint = of_graph_get_next_endpoint(dsi_node, NULL);
		if (endpoint) {
			remote_node = of_graph_get_remote_port_parent(endpoint);
			if (!remote_node) {
				pr_info("No panel connected,skip probe lcm\n");
				return -ENODEV;
			}
			pr_info("device node name:%s\n", remote_node->name);
		}
	}
	if (remote_node != dev->of_node) {
		pr_info("%s+ skip probe due to not current lcm\n", __func__);
		return -ENODEV;
	}

	chosen = of_find_node_by_path("/chosen");
	if(NULL!=chosen) {
	ret=of_property_read_string(chosen,"bootargs",&bootargs);
	if(0==ret) {
		pr_info("dsi_panel,bootargs,%s\n",bootargs);
			if(NULL!=strstr(bootargs,LCM_DRIVER_NAME)) {
				pr_info("dsi_panel,lcm compatibed,%s\n",LCM_DRIVER_NAME);
			} else {
				pr_err("dsi_panel,lcm find next driver\n");
				return -ENODEV;
			}
		} else {
		pr_err("dsi_panel,bootargs,read error\n");
		}
	} else {
	pr_err("dsi_panel,chosen,read error\n");
	}

	ctx = devm_kzalloc(dev, sizeof(struct lcm), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;
	panel_ctx = ctx;
	mipi_dsi_set_drvdata(dsi, ctx);
	ctx->dev = dev;
	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE | MIPI_DSI_MODE_NO_EOT_PACKET;
	backlight = of_parse_phandle(dev->of_node, "backlight", 0);
	if (backlight) {
		ctx->backlight = of_find_backlight_by_node(backlight);
		of_node_put(backlight);
		if (!ctx->backlight)
			return -EPROBE_DEFER;
	}
	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(dev, "%s: cannot get reset-gpios %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	devm_gpiod_put(dev, ctx->reset_gpio);

	ctx->enp_gpio =
		devm_gpiod_get(ctx->dev, "enp", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->enp_gpio)) {
		dev_err(ctx->dev, "%s: cannot get enp_gpio %ld\n",
			__func__, PTR_ERR(ctx->enp_gpio));
		return PTR_ERR(ctx->enp_gpio);
	}
	devm_gpiod_put(ctx->dev, ctx->enp_gpio);
	ctx->enn_gpio =
		devm_gpiod_get(ctx->dev, "enn", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->enn_gpio)) {
		dev_err(ctx->dev, "%s: cannot get enn_gpio %ld\n",
			__func__, PTR_ERR(ctx->enn_gpio));
		return PTR_ERR(ctx->enn_gpio);
	}
	devm_gpiod_put(ctx->dev, ctx->enn_gpio);

#ifndef CONFIG_FACTORY_BUILD
	ext_params.err_flag_irq_gpio=of_get_named_gpio(dev->of_node, "mi,esd-err-irq-gpio",0);
#endif

	ctx->prepared = true;
	ctx->enabled = true;
	ctx->dynamic_fps = 120;

	drm_panel_init(&ctx->panel, dev, &lcm_drm_funcs, DRM_MODE_CONNECTOR_DSI);
	drm_panel_add(&ctx->panel);
	ret = mipi_dsi_attach(dsi);
	if (ret < 0)
		drm_panel_remove(&ctx->panel);
#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_handle_reg(&ctx->panel);
	ret = mtk_panel_ext_create(dev, &ext_params, &ext_funcs, &ctx->panel);
	if (ret < 0)
		return ret;
#endif
	ret = sysfs_create_group(&dev->kobj, &icnl9916_attr_group);
	if (ret)
		return ret;
	pr_info("%s,dsi_panel 0d,finish\n", __func__);
	return ret;
}

static void lcm_remove(struct mipi_dsi_device *dsi)
{
	struct lcm *ctx = mipi_dsi_get_drvdata(dsi);
#if defined(CONFIG_MTK_PANEL_EXT)
	struct mtk_panel_ctx *ext_ctx = find_panel_ctx(&ctx->panel);
#endif
	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);
#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_detach(ext_ctx);
	mtk_panel_remove(ext_ctx);
#endif
	//return 0;
}

static const struct of_device_id lcm_of_match[] = {
	{ .compatible = "dsi_panel_p15a,lcm", },
	{ }
};
MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = LCM_DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};
module_mipi_dsi_driver(lcm_driver);

MODULE_LICENSE("GPL v2");
