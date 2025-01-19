# SPDX-License-Identifier: GPL-2.0
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat

ifneq ($(filter y m, $(CONFIG_MTK_MME_SUPPORT)),)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mme
endif

obj-$(CONFIG_DRM_PANEL_NOVATEK_NT35695B_CMD) += panel-nt35695b-auo-cmd.o
obj-$(CONFIG_DRM_PANEL_NOVATEK_NT36672A_VDO) += panel-nt36672a-rt4801-vdo.o
obj-$(CONFIG_DRM_PANEL_SAMSUNG_ANA6705_CMD) += panel-samsung-ana6705-cmd.o
obj-$(CONFIG_DRM_PANEL_SAMSUNG_ANA6705_CMD_FHDP) += panel-samsung-ana6705-cmd-fhdp.o
obj-$(CONFIG_DRM_PANEL_TRULY_TD4330_VDO) += panel-truly-td4330-vdo.o
obj-$(CONFIG_DRM_PANEL_TRULY_TD4330_CMD) += panel-truly-td4330-cmd.o
obj-$(CONFIG_DRM_PANEL_SAMSUNG_OP_CMD) += panel-samsung-op-cmd.o
obj-$(CONFIG_DRM_PANEL_SAMSUNG_OP_CMD_MSYNC2) += panel-samsung-op-cmd-msync2.o
obj-$(CONFIG_DRM_PANEL_TRULY_FT8756_VDO) += panel-truly-ft8756-vdo.o
obj-$(CONFIG_DRM_PANEL_TIANMA_R66451_CMD_120HZ) += panel-tianma-r66451-cmd-120hz.o
obj-$(CONFIG_DRM_PANEL_TIANMA_R66451_CMD_120HZ_WA) += panel-tianma-r66451-cmd-120hz-wa.o
obj-$(CONFIG_DRM_PANEL_TIANMA_NT36672E_VDO_120HZ_VFP) += panel-tianma-nt36672e-vdo-120hz-vfp.o
obj-$(CONFIG_DRM_PANEL_TIANMA_NT36672E_VDO_120HZ_VFP_6382) += panel-tianma-nt36672e-vdo-120hz-vfp-6382.o
obj-$(CONFIG_DRM_PANEL_ALPHA_JDI_NT36672E_CPHY_VDO) += panel-alpha-jdi-nt36672e-cphy-vdo.o
obj-$(CONFIG_DRM_PANEL_ALPHA_JDI_NT36672E_VDO_120HZ) += panel-alpha-jdi-nt36672e-vdo-120hz.o
obj-$(CONFIG_DRM_PANEL_ALPHA_JDI_NT36672E_VDO_120HZ_THRESHOLD) += panel-alpha-jdi-nt36672e-vdo-120hz-threshold.o
obj-$(CONFIG_DRM_PANEL_ALPHA_JDI_NT36672E_VDO_144HZ) += panel-alpha-jdi-nt36672e-vdo-144hz.o
obj-$(CONFIG_DRM_PANEL_ALPHA_JDI_NT36672E_VDO_60HZ) += panel-alpha-jdi-nt36672e-vdo-60hz.o
obj-$(CONFIG_DRM_PANEL_ALPHA_JDI_NT36672E_VDO_120HZ_HFP) += panel-alpha-jdi-nt36672e-vdo-120hz-hfp.o
obj-$(CONFIG_DRM_PANEL_ALPHA_JDI_NT36672E_VDO_144HZ_HFP) += panel-alpha-jdi-nt36672e-vdo-144hz-hfp.o
obj-$(CONFIG_DRM_PANEL_TRULY_NT35595_CMD) += panel-truly-nt35595-cmd.o
obj-$(CONFIG_DRM_PANEL_NT37801_MSYNC2_RTE) += panel-nt37801-msync2-rte.o
obj-$(CONFIG_DRM_PANEL_SAMSUNG_S68FC01_VDO_AOD) += panel-samsung-s68fc01-vdo-aod.o
obj-$(CONFIG_DRM_PANEL_ALPHA_SAMSUNG_S6E3HAE_CMD_120HZ) += panel-alpha-samsung-s6e3hae-fhd-dphy-cmd-120hz-r4831a.o
obj-$(CONFIG_DRM_PANEL_BOE_TV106C9MLL0) += ocp2138_i2c.o
obj-$(CONFIG_DRM_PANEL_NT37801_CMD_LTPO) += panel-nt37801-cmd-ltpo.o
obj-$(CONFIG_DRM_PANEL_NT37801_CMD_120HZ) += panel-nt37801-cmd-120hz.o
obj-$(CONFIG_DRM_PANEL_NT37801_CMD_FHD) += panel-nt37801-cmd-fhd.o
obj-$(CONFIG_DRM_PANEL_NT37801_CMD_SPR) += panel-nt37801-cmd-spr.o
obj-$(CONFIG_DRM_PANEL_NT37801_CMD_FHD_PLUS) += panel-nt37801-cmd-fhd-plus.o
obj-$(CONFIG_DRM_PANEL_BOE_TS127QFMLL1DKP0) += panel-boe-ts127qfmll1dkp0.o
obj-$(CONFIG_DRM_PANEL_RM692H5_CMD) += panel-rm692h5-cmd.o
obj-$(CONFIG_DRM_PANEL_RM692H5_ALPHA_CMD_SPR) += panel-rm692h5-alpha-cmd-spr.o
obj-$(CONFIG_DRM_PANEL_MAXIAM_MAX96851) += panel-maxiam-max96851.o
obj-$(CONFIG_DRM_PANEL_NT37707_C2V_ARP) += panel-nt37707-c2v-arp.o
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
