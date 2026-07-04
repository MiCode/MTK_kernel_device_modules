# In case the platform does NOT support this type of sensors

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/sensor/1.0/hwmon/include

obj-$(CONFIG_MTK_SENSOR_SUPPORT)	+= situation.o
obj-y += situation_hub/
obj-$(CONFIG_MTK_SAR_ALGO_HUB) += sar_algo/
obj-$(CONFIG_MTK_SAR_ALGO_HUB) += sar_algo_top/