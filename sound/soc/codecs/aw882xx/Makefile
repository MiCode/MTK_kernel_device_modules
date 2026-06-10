#for AWINIC AW882XX Smart PA
# MTK include path
# MTK include path
subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/sound/soc/mediatek/audio_dsp
subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/sound/soc/mediatek/common
subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/audio_ipi/include
subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include
subdir-ccflags-y += -I$(srctree)/drivers/base

# debugging support (also enables trace_printk)
ccflags-y += -Werror
ccflags-y += $(call cc-option,-Wno-date-time,)

#ccflags-y += -I$(srctree)/sound/soc/codecs/aw882xx

#snd-soc-aw882xx-objs += aw882xx.o
#snd-soc-aw882xx-objs += aw882xx_monitor.o
#snd-soc-aw882xx-objs += aw882xx_init.o
#snd-soc-aw882xx-objs += aw882xx_dsp.o
#snd-soc-aw882xx-objs += aw882xx_device.o
#snd-soc-aw882xx-objs += aw882xx_calib.o
#snd-soc-aw882xx-objs += aw882xx_bin_parse.o
#snd-soc-aw882xx-objs += aw882xx_spin.o

#obj-$(CONFIG_SND_SMARTPA_AW882XX) += snd-soc-aw882xx.o