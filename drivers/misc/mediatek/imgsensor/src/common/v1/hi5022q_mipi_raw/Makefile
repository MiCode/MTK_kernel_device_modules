
$(eval $(MTK_PLATFORM_KO)-objs += \
		$(subdrv-rpath)/hi5022q_mipiraw_Sensor.o \
	$(subdrv-rpath)/hi5022qmipi_Sensor_pdafotp.o)

ifeq ($(CONFIG_MTK_IMGSENSOR_ISP4_T), m)
$(eval $(MTK_PLATFORM_KO)-objs += $(IMGSENSOR_SUBDRV_OBJ))
else ifeq ($(CONFIG_MTK_IMGSENSOR_ISP4_T), y)
obj-$(CONFIG_MTK_IMGSENSOR_ISP4_T) += $(IMGSENSOR_SUBDRV_OBJ)
endif
