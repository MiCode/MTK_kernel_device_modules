ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/gpu/drm/mediatek/mediatek_v2 \
             -I$(DEVICE_MODULES_PATH)/drivers/gpu/drm/mediatek/mediatek_v2/mtk_drm_auto

ifeq ($(CONFIG_DRM_MEDIATEK_AUTO), y)
ccflags-y += -DMTK_AUTO_PRODUCT
mediatek-drm-y += \
	mtk_drm_crtc_auto.o \
	mtk_drm_drv_auto.o \
	mtk_drm_ddp_auto.o \
	mtk_drm_ddp_comp_auto.o \
	mtk_drm_plane_auto.o \
	mtk_drm_se.o \
	mtk_drm_sysfs.o \
	mtk_virtio_disp.o

ifeq ($(CONFIG_DRM_MEDIATEK_AUTO_HOST), y)
mediatek-drm-y += \
	mtk_virtual_output_host.o \
	mtk_disp_vdisp_ao_host.o
endif
endif

