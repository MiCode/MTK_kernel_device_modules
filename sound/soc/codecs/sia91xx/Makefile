# for SIA91XX Codec
SIA91XX_OBJS += sipa.o
SIA91XX_OBJS += sipa_regmap.o
SIA91XX_OBJS += sipa_aux_dev_if.o
SIA91XX_OBJS += sipa_91xx.o
SIA91XX_OBJS += sipa_parameter.o
SIA91XX_TUNING_OBJS += sipa_tuning_misc.o
SIA91XX_TUNING_OBJS += sipa_tuning_if.o

# Module information used by KBuild framework
obj-$(CONFIG_SND_SOC_SIA91XX) += sia91xx.o
sia91xx-y := $(SIA91XX_OBJS)

obj-$(CONFIG_SND_SOC_SIA91XX) += sia91xx_tuning.o
sia91xx_tuning-y := $(SIA91XX_TUNING_OBJS)
