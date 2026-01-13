subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/sound/soc/mediatek/common

ccflags-y += -I$(DEVICE_MODULES_PATH)/sound/soc/codecs/tfa9865/inc
ccflags-y += -DDEBUG -DTFA_NON_DSP_SOLUTION
ccflags-y += -Werror
ccflags-y += $(call cc-option,-Wno-date-time,)
snd-soc-tfa9865-objs += src/tfa98xx.o
snd-soc-tfa9865-objs += src/tfa_container.o
snd-soc-tfa9865-objs += src/tfa_dsp.o
snd-soc-tfa9865-objs += src/tfa_init.o

obj-$(CONFIG_SND_SMARTPA_TFA9865) += snd-soc-tfa9865.o
