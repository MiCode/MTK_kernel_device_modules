# SIA PA object

EXTRA_CFLAGS += $(call cc-option,-Wno-date-time,)

snd-soc-sipa-objs := \
		sipa.o \
		sipa_regmap.o \
		sipa_aux_dev_if.o \
		sipa_91xx.o \
		sipa_parameter.o

snd-soc-sipa-tuning-objs := sipa_tuning_misc.o \
		sipa_tuning_if.o \

EXTRA_CFLAGS += -DSIPA_COMPILE_TO_MODULE

obj-$(CONFIG_SND_SOC_SIPA_81XX) += snd-soc-sipa.o
obj-$(CONFIG_SND_SOC_SIPA_81XX) += snd-soc-sipa-tuning.o


