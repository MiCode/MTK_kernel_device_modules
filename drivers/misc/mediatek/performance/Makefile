MTK_PM_BUILD_FOLDER=$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/performance

subdir-ccflags-y += \
	-I$(MTK_PM_BUILD_FOLDER)/perf_ioctl/ \
	-I$(MTK_PM_BUILD_FOLDER)/include/\
	-I$(MTK_PM_BUILD_FOLDER)/powerhal_cpu_ctrl/\
	-I$(MTK_PM_BUILD_FOLDER)/pelt_hint/include/

# mtk_game
# obj-$(CONFIG_MTK_GAME) += game/

#Generic subdir build
# obj-$(CONFIG_MTK_FPSGO_V8) += fpsgo_v8/
#obj-$(CONFIG_MTK_FPSGO_V3) += fpsgo_v3/
obj-$(CONFIG_MTK_FRS) += frs/
obj-$(CONFIG_MTK_SBE) += sbe/
