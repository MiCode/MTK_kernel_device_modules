ccflags-$(CONFIG_TOUCHSCREEN_MTK_TOUCH) += -I$(DEVICE_MODULES_PATH)/drivers/input/touchscreen/
ccflags-y += -I$(srctree)/drivers/input/touchscreen/xiaomi/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/input/touchscreen/xiaomi/
obj-m += gt9916r.o
gt9916r-y :=\
		goodix_brl_spi.o \
		goodix_ts_core.o \
		goodix_brl_hw.o \
		goodix_cfg_bin.o \
		goodix_ts_utils.o \
		goodix_brl_fwupdate.o \
		goodix_ts_gesture.o \
		goodix_ts_inspect.o \
		goodix_ts_tools.o 
#		mi_disp_notifier.o
