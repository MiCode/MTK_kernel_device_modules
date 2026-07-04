ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(srctree)/drivers/gpu/drm/mediatek/mediatek_v2/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/input/touchscreen/

obj-m += icnl9951r_spi.o

icnl9951r_spi-objs := cts_driver.o
icnl9951r_spi-objs += cts_core.o
icnl9951r_spi-objs += cts_sfctrlv2.o
icnl9951r_spi-objs += cts_spi_flash.o
icnl9951r_spi-objs += cts_firmware.o
icnl9951r_spi-objs += cts_test.o
icnl9951r_spi-objs += cts_charger_detect.o
icnl9951r_spi-objs += cts_earjack_detect.o
icnl9951r_spi-objs += cts_tcs.o
icnl9951r_spi-objs += cts_platform.o
icnl9951r_spi-objs += cts_tool.o
icnl9951r_spi-objs += cts_sysfs.o
icnl9951r_spi-objs += cts_strerror.o
icnl9951r_spi-objs += cts_oem.o

