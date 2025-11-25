#####MBRAINK Common Include #####
MTK_MBRAINK_FOLDER=$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/auto_mbraink/
ccflags-y += -I$(MTK_MBRAINK_FOLDER)
subdir-ccflags-y += -I$(MTK_MBRAINK_FOLDER)

ifeq ($(CONFIG_MTK_MBRAINK_HOST_MODE),m)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/vhost/	\
			 -I$(KERNEL_SRC)/drivers/vhost/
endif

ifeq ($(CONFIG_MTK_SWPM_MODULE),m)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/lpm/modules/debug/sys_res
subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/lpm/modules/debug/sys_res

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/lpm/modules/debug/v2
subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/lpm/modules/debug/v2

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/lpm/modules/include
subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/lpm/modules/include

endif

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/gpu/drm/mediatek/mediatek_v2/
subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/gpu/drm/mediatek/mediatek_v2/

ccflags-y += -I$(srctree)/include/linux/mailbox
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat/
subdir-ccflags-y += -Wformat

BUILD_AUTO_MBRAINK_VHOST_NAME=mtk_mbraink_vhost_v8668
obj-m += ${BUILD_AUTO_MBRAINK_VHOST_NAME}.o

ifneq ($(CONFIG_MTK_AUTO_MBRAINK_HOST_MODE),m)
${BUILD_AUTO_MBRAINK_VHOST_NAME}-y +=mbraink_virtio_client.o
endif

