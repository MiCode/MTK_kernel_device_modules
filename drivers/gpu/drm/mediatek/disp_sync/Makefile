# SPDX-License-Identifier: GPL-2.0-only
$(warning build_test disp_sync)

subdir-ccflags-y += -Werror
subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/                \
                    -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat/        \
                    -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/mach/	\
                    -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat/        \
                    -I$(DEVICE_MODULES_PATH)/gpu/drm/mediatek/mediatek_v2/     \

#disp-sync-y := \
#	disp_sync.o

#obj-$(CONFIG_MTK_DISP_SYNC) += disp-sync.o