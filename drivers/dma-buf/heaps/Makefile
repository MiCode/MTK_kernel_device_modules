# SPDX-License-Identifier: GPL-2.0

ccflags-y += -I$(srctree)/drivers/dma-buf/heaps

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/iommu/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/iommu/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/iommu/arm/arm-smmu-v3/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/trusted_mem/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/monitor_hang/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/
ifndef CONFIG_GRT_HYPERVISOR
mtk_sec_heap-objs := \
	mtk_sec_heap_main.o \
	mtk_tmem_interop.o
obj-$(CONFIG_MTK_TRUSTED_MEMORY_SUBSYSTEM) += mtk_sec_heap.o
endif
obj-$(CONFIG_DMABUF_HEAPS_DEBUG) += mtk_heap_debug.o

