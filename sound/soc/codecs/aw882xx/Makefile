# TOPLEVEL=$(PWD)/..
# EXTRA_CFLAGS += -I$(TOPLEVEL)

# EXTRA_CFLAGS += -DDEBUG -DFSM_DEBUG
# EXTRA_CFLAGS += -Wall -Werror
# MODFLAGS = -fno-pic
# CFLAGS_MODULE = $(MODFLAGS)
# AFLAGS_MODULE = $(MODFLAGS)

subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/sound/soc/mediatek/common

snd-soc-aw882xx-objs := aw882xx.o \
			aw882xx_monitor.o \
			aw882xx_init.o \
			aw882xx_dsp.o \
			aw882xx_device.o \
			aw882xx_calib.o \
			aw882xx_bin_parse.o \
			aw882xx_spin.o

obj-$(CONFIG_SND_SMARTPA_AW882XX) += snd-soc-aw882xx.o

