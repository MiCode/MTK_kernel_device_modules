ccflags-y += -Wall
ccflags-y += -DILITEK_KERNEL_DRIVER
ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(DEVICE_MODULES_PATH)/drivers/gpu/drm/mediatek/mediatek_v2/

ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/
ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat/
ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mtk-interconnect/
ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/slbc/
ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(DEVICE_MODULES_PATH)/drivers/gpu/drm/mediatek/dpc/

#Un-mark below item to enable FW upgrade on boot
#ccflags-y += -DILITEK_BOOT_UPDATE

vir_touch := vhost_input

obj-m += $(vir_touch).o

$(vir_touch)-y += \
	tpd_control.o \
	tpd_hyp_virtio.o
