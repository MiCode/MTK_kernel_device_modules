#
# Makefile for trusty components
#

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/ise_trusty/include
ifneq ($(findstring $(CONFIG_TRUSTONIC_TEE_SUPPORT), y m),)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/tee/gud/$(CONFIG_TRUSTONIC_TEE_VERSION)/MobiCoreDriver/public/
endif

#obj-$(CONFIG_ISE_TRUSTY) += ise-trusty.o
#ise-trusty-$(CONFIG_ISE_TRUSTY) += ise-trusty-core.o ise-trusty-mem.o ise-trusty-shm.o
#ise-trusty-$(CONFIG_TRUSTONIC_TEE_SUPPORT) += ise-trusty-dci.o

#obj-$(CONFIG_ISE_TRUSTY_LOG) += ise-trusty-log.o
#obj-$(CONFIG_ISE_TRUSTY_VIRTIO_IPC) += ise-trusty-ipc.o
#obj-$(CONFIG_ISE_TRUSTY_VIRTIO) += ise-trusty-virtio.o
