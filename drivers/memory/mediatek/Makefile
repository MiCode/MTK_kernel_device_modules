# SPDX-License-Identifier: GPL-2.0
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/memory/mediatek/


ifneq (, $(filter m y, $(CONFIG_MTK_EMI_LEGACY)))
obj-$(CONFIG_MTK_EMI_LEGACY) += emi_legacy/
else
obj-$(CONFIG_MTK_EMI) += emi.o
emi-objs := emi-cen.o emi-isu.o emi-dbg.o
obj-$(CONFIG_MTK_EMI) += emi-fake-eng.o
obj-$(CONFIG_MTK_EMI) += smpu.o
obj-$(CONFIG_MTK_EMI) += smpu-hook-v1.o
obj-$(CONFIG_MTK_EMI_MPU_TEST) += emi-mpu-test.o
obj-$(CONFIG_MTK_EMI_MPU_TEST) += emi-mpu-test-v2.o
endif

