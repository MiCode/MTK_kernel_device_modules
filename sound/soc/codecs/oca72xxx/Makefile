# MTK include path
subdir-ccflags-y += -I$(srctree)/sound/soc/mediatek/audio_dsp
subdir-ccflags-y += -I$(srctree)/sound/soc/mediatek/common
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/audio_ipi/include
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
subdir-ccflags-y += -I$(srctree)/drivers/base

# debugging support (also enables trace_printk)
ccflags-y += -Werror
ccflags-y += $(call cc-option,-Wno-date-time,)
snd-soc-oca72xxx-objs:= oca72xxx.o oca72xxx_device.o oca72xxx_monitor.o oca72xxx_bin_parse.o oca72xxx_dsp.o oca72xxx_acf_bin.o
obj-$(CONFIG_SND_SOC_OCA72XXX) += snd-soc-oca72xxx.o
