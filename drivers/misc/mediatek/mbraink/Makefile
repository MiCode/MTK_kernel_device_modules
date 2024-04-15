#####MBRAINK Common Include #####
MTK_MBRAINK_FOLDER=$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mbraink/
ccflags-y += -I$(MTK_MBRAINK_FOLDER)
subdir-ccflags-y += -I$(MTK_MBRAINK_FOLDER)

ccflags-y += -I$(srctree)/drivers/android/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/qos/

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/swpm/inc/
subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/swpm/inc/

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mbraink/bridge
subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mbraink/bridge

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/power_throttling/
subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/power_throttling/

subdir-ccflags-y += -Wformat

obj-${CONFIG_MTK_MBRAINK} += mtk_mbraink.o
obj-${CONFIG_MTK_MBRAINK} += modules/
obj-${CONFIG_MTK_MBRAINK} += bridge/

mtk_mbraink-objs += mbraink_power.o mbraink_video.o mbraink_process.o mbraink.o mbraink_memory.o mbraink_gpu.o mbraink_audio.o mbraink_cpufreq.o mbraink_battery.o mbraink_pmu.o mbraink_gps.o mbraink_wifi.o
