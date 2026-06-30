ccflags-y += -Wall
ccflags-y += -DILITEK_KERNEL_DRIVER
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/vhost/	\
			 -I$(KERNEL_SRC)/drivers/vhost/
ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(DEVICE_MODULES_PATH)/drivers/gpu/drm/mediatek/mediatek_v2/
#Un-mark below item to enable FW upgrade on boot
#ccflags-y += -DILITEK_BOOT_UPDATE




#vir_common := vhost_common
#obj-m += $(vir_common).o
#$(vir_common)-y += \
#	touch_hypervisor.o \
#	touch_init.o

obj-$(CONFIG_TOUCHSCREEN_VHOST_COMMON)	+= vhost_input_yocto.o
vhost_input_yocto-y	+= touch_hypervisor.o

