#ifndef __ICS_HAPTIC_DRV_H__
#define __ICS_HAPTIC_DRV_H__

#include <linux/kernel.h>
#include <linux/atomic.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/regmap.h>
#include <linux/cdev.h>
#include <linux/input.h>

#include <linux/kfifo.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <linux/timer.h>
#include <linux/hrtimer.h>
#include <linux/device.h>
#include <linux/types.h>
#include <linux/leds.h>

#define DEBUG
#define AAC_RICHTAP_SUPPORT
//#define ENABLE_PIN_CONTROL

#define ics_err(format, ...) \
    pr_info("[ics_haptic]" format, ##__VA_ARGS__)

#define ics_info(format, ...) \
    pr_info("[ics_haptic]" format, ##__VA_ARGS__)

#define ics_dbg(format, ...) \
    pr_info("[ics_haptic]" format, ##__VA_ARGS__)

#define check_error_return(ret)     \
    if (ret < 0) {                  \
        pr_info("%s: check_error_return! ret = %d\n", __func__, ret);    \
        return ret;                 \
    }                               \

#ifdef TIMED_OUTPUT
#include "../staging/android/timed_output.h"
typedef struct timed_output_dev vib_dev_t;
#else
typedef struct led_classdev vib_dev_t;
#endif

#define ICS_HAPTIC_VERSION     "v0.9.8"
#define ICS_HAPTIC_NAME        "haptic_rt"
#define MAX_STREAM_FIFO_SIZE    4096
#define MAX_PRESET_NAME_LEN     64
#define DEFAULT_DEV_NAME        "vibrator"
#define GP_BUFFER_SIZE          1536
#define MAX_BEMF_DAQ_COUNT      40
#define MAX_BEMF_DAQ_UNIT       6
#define MAX_BEMF_BUF_SIZE       (MAX_BEMF_DAQ_COUNT * MAX_BEMF_DAQ_UNIT + 4)
#define RESAMPLE_THRESHOLD      1

enum ics_haptic_play_mode
{
    PLAY_MODE_RAM           = 0x01,
    PLAY_MODE_STREAM        = 0x02,
    PLAY_MODE_TRACK         = 0x03,
    PLAY_MODE_RAM_LOOP      = 0x04,
};

enum ics_haptic_boost_mode
{
    BOOST_MODE_NORMAL       = 0x00,
    BOOST_MODE_BYPASS       = 0x01,
};

enum ics_hatpic_sys_state
{
    SYS_STATE_STOP          = 0x00,
    SYS_STATE_RAM_PLAY      = 0X01,
    SYS_STATE_STREAM_PLAY   = 0X02,
    SYS_STATE_TRACK_PLAY    = 0X03,
    SYS_STATE_TRIG          = 0x04,
    SYS_STATE_DETECT        = 0x05,
    SYS_STATE_BRAKE         = 0X06,
};

struct ics_haptic_chip_config
{
    uint32_t chip_id;
    uint32_t reg_size;
    uint32_t ram_size;
    uint32_t sys_f0;
    uint32_t f0;
    uint32_t list_base_addr;
    uint32_t wave_base_addr;
    uint32_t fifo_ae;
    uint32_t fifo_af;
    int32_t fifo_size;
    int32_t list_section_size;
    int32_t wave_section_size;
    uint32_t boost_mode;
    uint32_t boost_vol;
    uint32_t boost_vol_ram;
    uint32_t boost_vol_rtp;
    uint32_t ram_loop_gain;

    uint32_t gain;
    uint32_t brake_en;
    uint32_t brake_wave_no;
    uint32_t brake_wave_mode;
    uint32_t brake_const;
    uint32_t brake_acq_point;
    int32_t vbat;
    int32_t resistance;
    uint32_t ram_effect_count;

    uint32_t play_time[175];

};

struct ics_haptic_data
{
    struct ics_haptic_chip_config chip_config;
    uint32_t waveform_size;
    uint8_t *waveform_data;
    //
    vib_dev_t vib_dev;
    char vib_name[64];
    //
    struct i2c_client *client;
    struct device *dev;
    struct regmap *regmap;
    struct mutex lock;
    struct mutex preset_lock;
    struct hrtimer timer;
    struct delayed_work chip_init_work;
    struct work_struct vibrator_work;
    struct work_struct preset_work;
    struct work_struct brake_guard_work;
    struct ics_haptic_func *func;
    //
    bool chip_initialized;
    bool stream_start;
    struct kfifo stream_fifo;
    uint8_t *ram_buf;
    uint8_t *gp_buf;
    //
    uint32_t play_mode;
    uint32_t play_status;
    int32_t amplitude;
    uint32_t preset_wave_index;
    uint32_t ram_wave_index;
    uint32_t duration;
    uint32_t activate_state;
    uint32_t sys_state;
    uint32_t irq_state;
    int32_t adc_offset;
    //
    int32_t gpio_en;
    int32_t gpio_irq;
    //
    uint32_t efs_data;
    uint8_t efs_ver;
    //
    uint32_t daq_en;
    int32_t daq_size;
    uint8_t daq_data[MAX_BEMF_BUF_SIZE];
    struct hrtimer reset_timer;
    // for input dev
    struct input_dev *input_dev;
    struct workqueue_struct *input_work_queue;
    struct hrtimer input_timer;
    struct mutex input_lock;
    atomic_t is_in_rtp_loop;
    atomic_t exit_in_rtp_loop;
    atomic_t exit_write_loop;
    atomic_t wave_data_done;
    atomic_t kfifo_available;
    wait_queue_head_t wait_q; /*wait queue for exit irq mode */
    wait_queue_head_t stop_wait_q; /* wait queue for stop rtp mode */
    wait_queue_head_t kfifo_wait_q; /* wait queue for kfifo available*/
    enum ics_haptic_play_mode activate_mode;
    enum ics_haptic_play_mode current_mode;
    int32_t effect_type;
    int32_t effect_id;
    uint32_t effect_max;
    uint32_t effects_count;
    int32_t is_custom_wave;

    ktime_t input_pre_time;
    ktime_t input_cur_time;
    unsigned long int input_interval_us;

    int32_t state;
    struct work_struct input_vibrator_work;
    struct work_struct input_rtp_work;
    struct work_struct input_set_gain_work;
    int32_t magnitude;
    uint8_t level;
    uint32_t new_gain;
    uint8_t rtp_file_num;

    uint32_t autotrack_f0;
    uint32_t bemf_f0;
    uint32_t f0_en;

#ifdef AAC_RICHTAP_SUPPORT
    char richtap_misc_name[64];
    void* richtap_data;
    struct work_struct richtap_stream_work;
#endif
#ifdef ENABLE_PIN_CONTROL
    struct pinctrl *ics_pinctrl;
    struct pinctrl_state *pinctrl_state[3];
#endif
    int enable_pin_control;
};

struct ics_haptic_func
{
    int32_t (*chip_init)(struct ics_haptic_data *, const uint8_t *, int32_t);
    int32_t (*get_chip_id)(struct ics_haptic_data *);
    int32_t (*get_reg)(struct ics_haptic_data *, uint32_t, uint32_t *);
    int32_t (*set_reg)(struct ics_haptic_data *, uint32_t, uint32_t);
    int32_t (*get_f0)(struct ics_haptic_data *);
    int32_t (*set_f0)(struct ics_haptic_data *, uint32_t);
    int32_t (*get_play_mode)(struct ics_haptic_data *);
    int32_t (*set_play_mode)(struct ics_haptic_data *, uint32_t);
    int32_t (*play_go)(struct ics_haptic_data *);
    int32_t (*play_stop)(struct ics_haptic_data *);
    int32_t (*get_play_status)(struct ics_haptic_data *);
    int32_t (*set_brake_en)(struct ics_haptic_data *, uint32_t);
    int32_t (*set_daq_en)(struct ics_haptic_data *, uint32_t);
    int32_t (*set_f0_en)(struct ics_haptic_data *, uint32_t);
    int32_t (*set_gain)(struct ics_haptic_data *, uint32_t);
    int32_t (*set_bst_vol)(struct ics_haptic_data *, uint32_t);
    int32_t (*set_bst_mode)(struct ics_haptic_data *, uint32_t);
    int32_t (*set_play_list)(struct ics_haptic_data *, const uint8_t *, int32_t);
    int32_t (*get_ram_data)(struct ics_haptic_data *, uint8_t *, int32_t *);
    int32_t (*set_waveform_data)(struct ics_haptic_data *, const uint8_t *, int32_t);
    int32_t (*set_sys_data)(struct ics_haptic_data *, const uint8_t *, int32_t);
    int32_t (*get_daq_data)(struct ics_haptic_data *, uint8_t *, int32_t *);
    int32_t (*clear_stream_fifo)(struct ics_haptic_data *);
    int32_t (*set_stream_data)(struct ics_haptic_data *, const uint8_t *, int32_t);
    int32_t (*get_sys_state)(struct ics_haptic_data *);
    int32_t (*get_vbat)(struct ics_haptic_data *);
    int32_t (*get_resistance)(struct ics_haptic_data *);
    int32_t (*get_irq_state)(struct ics_haptic_data *);
    int32_t (*get_adc_offset)(struct ics_haptic_data *);
    int32_t (*resample_ram_waveform)(struct ics_haptic_data *);
    bool (*is_irq_play_done)(struct ics_haptic_data *);
    bool (*is_irq_fifo_ae)(struct ics_haptic_data *);
    bool (*is_irq_fifo_af)(struct ics_haptic_data *);
    bool (*is_irq_protection)(struct ics_haptic_data *);
    int32_t (*clear_protection)(struct ics_haptic_data *);
};

extern struct ics_haptic_func rt6010_func_list;
int32_t send_stream_data(struct ics_haptic_data *haptic_data, uint32_t fifo_available_size);

#endif // __ICS_HAPTIC_DRV_H__
