#include <linux/module.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/power_supply.h>
#include <linux/regmap.h>
#include <linux/types.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/usb/phy.h>
#include <linux/acpi.h>
#include <linux/of.h>
#include <linux/gpio.h>
#include <linux/kthread.h>
#include <linux/sched/prio.h>
#include <uapi/linux/sched/types.h>
#include <linux/platform_device.h>

#include "get_boot.h"

#define MAX_CMD_LENGTH      100

static uint8_t boot_mode = BOOT_MODE_NORMOL;
static bool get_boot = false;
static char boot_mode_str[MAX_CMD_LENGTH];

uint8_t get_boot_mode(void)
{
	char *substr = NULL;
    int i = 0;

    if (get_boot) {
        return boot_mode;
    }

	substr = strstr(boot_command_line, "androidboot.mode=");
	if (substr) {
		substr += strlen("androidboot.mode=");
		for (i = 0; substr[i] != ' ' && i < MAX_CMD_LENGTH && substr[i] != '\0'; i++) {
			boot_mode_str[i] = substr[i];
		}
		boot_mode_str[i] = '\0';
	}

    if (strcmp(boot_mode_str, "test") == 0) {
        boot_mode = BOOT_MODE_TEST;
    } else {
        boot_mode = BOOT_MODE_NORMOL;
    }
    get_boot = true;
    return boot_mode;
}
EXPORT_SYMBOL_GPL(get_boot_mode);
