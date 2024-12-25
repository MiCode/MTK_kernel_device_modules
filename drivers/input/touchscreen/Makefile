# SPDX-License-Identifier: GPL-2.0
#
# Makefile for the touchscreen drivers.
#
obj-m += tui-common.o
TOUCH_LISTS := $(subst ",,$(CONFIG_TOUCHSCREEN_MTK_TOUCH))
obj-y += $(foreach TOUCH,$(TOUCH_LISTS),$(TOUCH)/)


#obj-$(CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE) += xiaomi/
obj-$(CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE_COMMON) += xiaomi_touch/
obj-$(CONFIG_TOUCHSCREEN_GET_LCMNAME) += tp_get_lcm_name/
