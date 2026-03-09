# SPDX-License-Identifier: GPL-2.0

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/memory
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/clk/mediatek/

ifneq ($(filter y m, $(CONFIG_MTK_MME_SUPPORT)),)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mme
endif

ifneq ($(filter mt6771, $(CONFIG_MTK_LEGACY_PLATFORM)),)
ccflags-y += -DCONFIG_MTK_SMI_MT6771
obj-$(CONFIG_DEVICE_MODULES_MTK_SMI)		+= mtk-smi-mt6771.o
else
obj-$(CONFIG_DEVICE_MODULES_MTK_SMI)		+= mtk-smi.o
endif

obj-$(CONFIG_HAVE_MTK_MC)	+= mediatek/
