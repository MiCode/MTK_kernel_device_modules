# SPDX-License-Identifier: GPL-2.0-only
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/clk/mediatek/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/cmdq/mailbox
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/vcp/include/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/vcp/rv/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mmdvfs/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mmdebug/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/iommu/arm/arm-smmu-v3/
ccflags-y += -I$(srctree)/drivers/soc/mediatek/
obj-$(CONFIG_MTK_CMDQ) += mtk-cmdq-helper.o
#obj-$(CONFIG_DEVICE_MODULES_MTK_PMIC_WRAP) += mtk-pmic-wrap.o
obj-$(CONFIG_DEVICE_MODULES_MTK_SCPSYS) += mtk-scpsys.o
ifneq ($(wildcard $(src)/mtk-scpsys-mt6768.c),)
obj-$(CONFIG_DEVICE_MODULES_MTK_SCPSYS) += mtk-scpsys-mt6768.o
endif
ifneq ($(wildcard $(src)/mtk-scpsys-mt6761.c),)
obj-$(CONFIG_DEVICE_MODULES_MTK_SCPSYS) += mtk-scpsys-mt6761.o
endif
ifneq ($(wildcard $(src)/mtk-scpsys-mt6765.c),)
obj-$(CONFIG_DEVICE_MODULES_MTK_SCPSYS) += mtk-scpsys-mt6765.o
endif
ifneq ($(wildcard $(src)/mtk-scpsys-mt6781.c),)
obj-$(CONFIG_DEVICE_MODULES_MTK_SCPSYS) += mtk-scpsys-mt6781.o
endif
ifneq ($(wildcard $(src)/mtk-scpsys-mt6789.c),)
obj-$(CONFIG_DEVICE_MODULES_MTK_SCPSYS) += mtk-scpsys-mt6789.o
endif
ifneq ($(wildcard $(src)/mtk-scpsys-mt6833.c),)
obj-$(CONFIG_DEVICE_MODULES_MTK_SCPSYS) += mtk-scpsys-mt6833.o
endif
ifneq ($(wildcard $(src)/mtk-scpsys-mt6853.c),)
obj-$(CONFIG_DEVICE_MODULES_MTK_SCPSYS) += mtk-scpsys-mt6853.o
endif
ifneq ($(wildcard $(src)/mtk-scpsys-mt6855.c),)
obj-$(CONFIG_DEVICE_MODULES_MTK_SCPSYS) += mtk-scpsys-mt6855.o
endif
ifneq ($(wildcard $(src)/mtk-scpsys-mt6877.c),)
obj-$(CONFIG_DEVICE_MODULES_MTK_SCPSYS) += mtk-scpsys-mt6877.o
endif
ifneq ($(wildcard $(src)/mtk-scpsys-mt6886.c),)
obj-$(CONFIG_DEVICE_MODULES_MTK_SCPSYS) += mtk-scpsys-mt6886.o
endif
ifneq ($(wildcard $(src)/mtk-scpsys-mt6893.c),)
obj-$(CONFIG_DEVICE_MODULES_MTK_SCPSYS) += mtk-scpsys-mt6893.o
endif
ifneq ($(wildcard $(src)/mtk-scpsys-mt6897.c),)
obj-$(CONFIG_DEVICE_MODULES_MTK_SCPSYS) += mtk-scpsys-mt6897.o
endif
ifneq ($(wildcard $(src)/mtk-scpsys-mt6899.c),)
obj-$(CONFIG_DEVICE_MODULES_MTK_SCPSYS) += mtk-scpsys-mt6899.o
endif
ifneq ($(wildcard $(src)/mtk-scpsys-mt6983.c),)
obj-$(CONFIG_DEVICE_MODULES_MTK_SCPSYS) += mtk-scpsys-mt6983.o
endif
ifneq ($(wildcard $(src)/mtk-scpsys-mt6985.c),)
obj-$(CONFIG_DEVICE_MODULES_MTK_SCPSYS) += mtk-scpsys-mt6985.o
endif
ifneq ($(wildcard $(src)/mtk-scpsys-mt6989.c),)
obj-$(CONFIG_DEVICE_MODULES_MTK_SCPSYS) += mtk-scpsys-mt6989.o
endif
ifneq ($(wildcard $(src)/mtk-scpsys-mt6991-spm.c),)
obj-$(CONFIG_DEVICE_MODULES_MTK_SCPSYS) += mtk-scpsys-mt6991-spm.o
endif
ifneq ($(wildcard $(src)/mtk-scpsys-mt6991-mmpc.c),)
obj-$(CONFIG_DEVICE_MODULES_MTK_SCPSYS) += mtk-scpsys-mt6991-mmpc.o
endif
ifneq ($(wildcard $(src)/mtk-scpsys-mt6991-ivi.c),)
obj-$(CONFIG_DEVICE_MODULES_MTK_SCPSYS_IVI) += mtk-scpsys-mt6991-ivi.o
endif
ifneq ($(wildcard $(src)/mt8188-pm-domains.h),)
obj-$(CONFIG_DEVICE_MODULES_MTK_SCPSYS_PM_DOMAINS) += mtk-pm-domains.o
endif
obj-$(CONFIG_DEVICE_MODULES_MTK_MMSYS_MMSYS) += mtk-mmsys.o
obj-$(CONFIG_DEVICE_MODULES_MTK_DEVAPC) += devapc/
obj-$(CONFIG_MTK_DEVMPU) += devmpu/
obj-$(CONFIG_MTK_MMDVFS) += mmdvfs/
obj-$(CONFIG_MTK_MMDVFS) += mtk-mmdvfs.o
ifneq ($(filter y m,$(CONFIG_MTK_MMDVFS_VCP)),)
obj-$(CONFIG_MTK_MMDVFS) += mtk-mmdvfs-v3.o
endif
obj-$(CONFIG_DEVICE_MODULES_MTK_MMSYS_MUTEX) += mtk-mutex.o
obj-$(CONFIG_MTK_SOCINFO) += mtk-socinfo.o
