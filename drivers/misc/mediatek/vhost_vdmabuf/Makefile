# SPDX-License-Identifier: GPL-2.0-only

# ifeq ($(DEVICE_MODULES_PATH),)
# DEVICE_MODULES_PATH = $(srctree)
# else
# LINUXINCLUDE := $(DEVCIE_MODULES_INCLUDE) $(LINUXINCLUDE)
# endif

# VHOST_VDMABUF_PATH = $(if $(filter /%,$(src)),,$(srctree)/)$(src)
subdir-ccflags-y += \
	-I$(DEVICE_MODULES_PATH)/include/ \
	-I$(DEVICE_MODULES_PATH)/drivers/vhost/ \
	-I$(KERNEL_SRC)/drivers/vhost/ \
	-I$(DEVICE_MODULES_PATH)/../device_module/include/

# ccflags-y += -I$(VHOST_VDMABUF_PATH)/include

obj-$(CONFIG_DEVICE_MODULES_VHOST_VDMABUF) += vhost_vdmabuf.o
vhost_vdmabuf-y := vhost_vdmabuf_host.o vhost_vdmabuf_dmabuf.o
