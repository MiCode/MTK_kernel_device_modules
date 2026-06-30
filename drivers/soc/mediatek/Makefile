# SPDX-License-Identifier: GPL-2.0-only
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/clk/mediatek/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/cmdq/mailbox
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/vcp/include/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/vcp/rv/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mmdvfs/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mmdebug/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mmp/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/smi/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/iommu/arm/arm-smmu-v3/
ccflags-y += -I$(srctree)/drivers/soc/mediatek/
obj-$(CONFIG_MTK_CMDQ) += mtk-cmdq-helper.o
#obj-$(CONFIG_DEVICE_MODULES_MTK_PMIC_WRAP) += mtk-pmic-wrap.o
ifneq ($(wildcard $(src)/mtk-scpsys-mt6991-ivi.c),)
obj-$(CONFIG_DEVICE_MODULES_MTK_SCPSYS_IVI) += mtk-scpsys-mt6991-ivi.o
endif
#ifneq ($(wildcard $(src)/mt8188-pm-domains.h),)
#obj-$(CONFIG_DEVICE_MODULES_MTK_SCPSYS_PM_DOMAINS) += mtk-pm-domains.o
#endif
obj-$(CONFIG_DEVICE_MODULES_MTK_MMSYS_MMSYS) += mtk-mmsys.o
obj-$(CONFIG_DEVICE_MODULES_MTK_DEVAPC) += devapc/
# obj-$(CONFIG_MTK_DEVMPU) += devmpu/
obj-$(CONFIG_MTK_MMDVFS) += mmdvfs/
obj-$(CONFIG_DEVICE_MODULES_MTK_MMSYS_MUTEX) += mtk-mutex.o
