# SPDX-License-Identifier: GPL-2.0
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat

ifneq ($(filter y m, $(CONFIG_MTK_MME_SUPPORT)),)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mme
endif

obj-$(CONFIG_DRM_PANEL_SAMSUNG_ANA6705_CMD) += panel-samsung-ana6705-cmd.o
obj-$(CONFIG_DRM_PANEL_SAMSUNG_ANA6705_CMD_FHDP) += panel-samsung-ana6705-cmd-fhdp.o
obj-$(CONFIG_DRM_PANEL_NT37801_MSYNC2_RTE) += panel-nt37801-msync2-rte.o
obj-$(CONFIG_DRM_PANEL_SAMSUNG_S68FC01_VDO_AOD) += panel-samsung-s68fc01-vdo-aod.o
obj-$(CONFIG_DRM_PANEL_ALPHA_SAMSUNG_S6E3HAE_CMD_120HZ) += panel-alpha-samsung-s6e3hae-fhd-dphy-cmd-120hz-r4831a.o
obj-$(CONFIG_DRM_PANEL_BOE_TV106C9MLL0) += ocp2138_i2c.o
obj-$(CONFIG_DRM_PANEL_MAXIAM_MAX96851) += panel-maxiam-max96851.o
mediatek-drm-gateic-y := mtk_drm_panel_i2c.o \
		  mtk_drm_gateic_rt4801h.o \
		  mtk_drm_gateic_rt4831a.o \
		  mtk_drm_gateic.o

mediatek-drm-panel-drv-y :=  \
		  mtk_round_corner/mtk_drm_rc.o \
		  mtk_drm_panel_helper.o \
		  mtk_drm_panel_helper_dbi.o \
		  mtk_drm_panel_helper_dpi.o \
		  mtk_drm_panel_helper_dsi.o \
		  mtk_drm_panel_drv_dbi.o \
		  mtk_drm_panel_drv_dpi.o \
		  mtk_drm_panel_drv_dsi.o \
		  mtk_drm_panel_drv.o

mediatek-cust-panel-sample-y :=  \
		  cust_panel/mtk_cust_panel_sample/mtk_drm_panel_cust.o \
		  cust_panel/cust_lcm_hal.o


obj-$(CONFIG_DRM_MEDIATEK_PANEL) += mediatek-drm-gateic.o
obj-$(CONFIG_DRM_MEDIATEK_PANEL) += mediatek-drm-panel-drv.o
obj-$(CONFIG_DRM_MEDIATEK_PANEL) += mediatek-cust-panel-sample.o

ccflags-$(CONFIG_DRM_MEDIATEK_PANEL) += -I$(DEVICE_MODULES_PATH)/drivers/leds/
