#ifneq ($(MTK_VCODEC_PLATFORM),)

mtk-vcodec-enc-$(CONFIG_VIDEO_MEDIATEK_VCU) += venc/venc_common_if.o \
		venc/venc_vcu_if.o

mtk-vcodec-enc-$(CONFIG_MTK_TINYSYS_VCP_SUPPORT) += venc/venc_vcp_if.o

mtk-vcodec-enc-$(CONFIG_DEVICE_MODULES_VIDEO_MEDIATEK_VCODEC) += mtk_vcodec_enc_drv.o \
		venc_drv_if.o \
		mtk_vcodec_enc.o \
		mtk_vcodec_enc_pm.o

ifeq ($(CONFIG_VIDEO_MEDIATEK_VCODEC_V1),y)
	obj-$(CONFIG_DEVICE_MODULES_VIDEO_MEDIATEK_VCODEC) += mtk-vcodec-enc-v1.o

	mtk-vcodec-enc-v1-$(CONFIG_DEVICE_MODULES_VIDEO_MEDIATEK_VCODEC) += \
			v1/mtk_vcodec_enc_pm_plat.o \
			v1/vcodec_dvfs.o

	mtk-vcodec-enc-v1-$(CONFIG_DEVICE_MODULES_VIDEO_MEDIATEK_VCODEC) += $(mtk-vcodec-enc-m) $(mtk-vcodec-enc-y)
endif

ifeq ($(CONFIG_VIDEO_MEDIATEK_VCODEC_V2),y)
	obj-$(CONFIG_DEVICE_MODULES_VIDEO_MEDIATEK_VCODEC) += mtk-vcodec-enc-v2.o

	mtk-vcodec-enc-v2-$(CONFIG_DEVICE_MODULES_VIDEO_MEDIATEK_VCODEC) += \
			v2/mtk_vcodec_enc_pm_plat.o \
			v2/vcodec_dvfs.o

	mtk-vcodec-enc-v2-$(CONFIG_DEVICE_MODULES_VIDEO_MEDIATEK_VCODEC) += $(mtk-vcodec-enc-m) $(mtk-vcodec-enc-y)
endif

subdir-ccflags-y += -Werror
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/media/platform/mtk-vcu
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/media/platform/mtk-vcodec
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/slbc
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/smi
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/iommu
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/iommu
ccflags-y += -I$(DEVICE_MODULES_PATH)/arch/arm
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/staging/android/ion
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mtk-interconnect
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/vcp/rv
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/vcp/include
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/dma-buf/heaps
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/iommu/arm/arm-smmu-v3
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/sched/eas
ccflags-y += -I$(DEVICE_MODULES_PATH)/include/soc/mediatek
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/trusted_mem/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/cpufreq_lite/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/sched
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/sched/util
ccflags-y += -I$(srctree)/kernel/
#endif
