# SPDX-License-Identifier: GPL-2.0
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat

ifneq ($(filter y m, $(CONFIG_MTK_MME_SUPPORT)),)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mme
endif

obj-$(CONFIG_DRM_PANEL_NT37801_MSYNC2_RTE) += panel-nt37801-msync2-rte.o
obj-$(CONFIG_DRM_PANEL_BOE_TV106C9MLL0) += ocp2138_i2c.o
obj-$(CONFIG_DRM_PANEL_MAXIAM_MAX96851) += panel-maxiam-max96851.o

obj-$(CONFIG_DRM_PANEL_O6_42_0D_0A_DSC_VDO) += panel-o6-42-0d-0a-dsc-vdo.o
obj-$(CONFIG_DRM_PANEL_O6_41_02_0B_DSC_VDO) += panel-o6-41-02-0b-dsc-vdo.o
obj-$(CONFIG_DRM_PANEL_O6_36_0F_0C_DSC_VDO) += panel-o6-36-0f-0c-dsc-vdo.o
obj-$(CONFIG_DRM_PANEL_O7_38_0C_0A_DSC_VDO) += panel-o7-38-0c-0a-dsc-vdo.o

#ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/scp/include/
#ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/dramc/$(MTK_PLATFORM)	\
#             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/smi/			\
#             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mminfra/			\
#             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mmp/			\
#             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/spi_slave_drv/          \
#            -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/cmdq/bridge/ 	\
#             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/clkbuf/src/	\
#             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/base/power/include/     \
#             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mmdvfs/			\
#             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/pmic/			\
#             -I$(DEVICE_MODULES_PATH)/drivers/iommu/				\
#             -I$(DEVICE_MODULES_PATH)/drivers/devfreq/				\
#             -I$(srctree)/drivers/gpu/drm/				\
#             -I$(srctree)/drivers/gpu/drm/display/				\
#             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/mach/				\
#             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat/ \
#             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/leds/ \
#             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/leds/$(MTK_PLATFORM) \
#             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/cmdq/mailbox/        \
#             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/iommu/		\
#             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mtk-interconnect/        \
#             -I$(DEVICE_MODULES_PATH)/sound/soc/mediatek/common/ \
#             -I$(srctree)/drivers/gpu/drm/mediatek \
#             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/slbc/ \
#             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/cmdq/mailbox/ \
#             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/tinysys_scmi/ \
#             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/vcp/rv_v2/ \
#             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/vcp/include \
#             -I$(DEVICE_MODULES_PATH)/drivers/dma-buf/heaps/ \
#             -I$(DEVICE_MODULES_PATH)/drivers/gpu/drm/mediatek/mml \
#             -I$(DEVICE_MODULES_PATH)/drivers/iommu/arm/arm-smmu-v3 \
#             -I$(DEVICE_MODULES_PATH)/drivers/gpu/drm/mediatek/dpc \
#             -I$(DEVICE_MODULES_PATH)/drivers/clk/mediatek \
#             -I$(DEVICE_MODULES_PATH)/drivers/gpu/drm/mediatek/mediatek_v2/mi_disp/ \
#             -I$(DEVICE_MODULES_PATH)/drivers/gpu/drm/mediatek/mediatek_v2/ \

ccflags-$(CONFIG_DRM_MEDIATEK_PANEL) += -I$(DEVICE_MODULES_PATH)/drivers/leds/
