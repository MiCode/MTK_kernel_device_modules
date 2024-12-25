###
# Copyright (C) Fourier Semiconductor Inc. 2016-2020. All rights reserved.
#

# TOPLEVEL=$(PWD)/..
# EXTRA_CFLAGS += -I$(TOPLEVEL)

# EXTRA_CFLAGS += -DDEBUG -DFSM_DEBUG
# EXTRA_CFLAGS += -Wall -Werror
# MODFLAGS = -fno-pic
# CFLAGS_MODULE = $(MODFLAGS)
# AFLAGS_MODULE = $(MODFLAGS)

subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/sound/soc/mediatek/common

snd-soc-fs19xx-objs := \
				fs18yn.o \
				fs1958.o \
				fsm_core.o \
				fsm_i2c.o

obj-$(CONFIG_SND_SOC_FS1XXX) += snd-soc-fs19xx.o
