# SPDX-License-Identifier: GPL-2.0
#
# Makefile for the touchscreen drivers.
#
TOUCH_LISTS := $(subst ",,$(CONFIG_TOUCHSCREEN_MTK_TOUCH))
obj-y += $(foreach TOUCH,$(TOUCH_LISTS),$(TOUCH)/)

ifneq ($(wildcard $(src)/k6985v1_64_alpha),)
obj-$(CONFIG_TOUCHSCREEN_MT6985_ALPHA_SUPPORT) += k6985v1_64_alpha/
endif

obj-$(CONFIG_TOUCHSCREEN_ILITEK_TOUCH) += ILITEK/
obj-$(CONFIG_TOUCHSCREEN_SYNAPTICS_DSX_CORE_96789) += synaptics_dsx_96789/
obj-$(CONFIG_TOUCHSCREEN_SYNAPTICS_DSX_CORE_96851) += synaptics_dsx_96851/

