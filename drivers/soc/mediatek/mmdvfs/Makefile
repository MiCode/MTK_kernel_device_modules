# SPDX-License-Identifier: GPL-2.0-only
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/clk/mediatek/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/vcp/include/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/vcp/rv/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mmdvfs/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mmdebug/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/iommu/arm/arm-smmu-v3/

ifneq ($(filter y m,$(CONFIG_MTK_TINYSYS_VCP_SUPPORT)),)
obj-$(CONFIG_MTK_MMDVFS) += mtk-mmdvfs-v5.o
ifneq ($(wildcard $(src)/mmdvfs-mt6993.c),)
obj-$(CONFIG_MTK_MMDVFS) += mmdvfs-mt6993.o
endif
endif
