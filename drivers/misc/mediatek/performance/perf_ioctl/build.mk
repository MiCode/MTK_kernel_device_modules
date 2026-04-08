
# Add cflags as bellow
#mtk_perf_ioctl_cflags += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat/
subdir-ccflags-y += -I$(srctree)/kernel/
subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/performance/fpsgo_v3/base/include/
subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/sched/
subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/performance/pelt_hint/include/
mtk_perf_ioctl_objs += perf_ioctl.o
mtk_ioctl_touch_boost_objs += ioctl_touch_boost.o
mtk_perf_ioctl_magt_objs += perf_ioctl_magt.o
mtk_ioctl_powerhal_objs += ioctl_powerhal.o
