# SPDX-License-Identifier: GPL-2.0

VERSION := legacy

# common include path
ccflags-y += -I$(DEVICE_MODULES_PATH)/sound/soc/mediatek/common
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/adsp
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/adsp/include
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/base/power/include
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include

# target object
obj-${CONFIG_MTK_ADSP_LEGACY} += adsp-$(VERSION).o

adsp-$(VERSION)-objs += adsp_driver_$(VERSION).o
adsp-$(VERSION)-objs += adsp_clk_tinysys.o
