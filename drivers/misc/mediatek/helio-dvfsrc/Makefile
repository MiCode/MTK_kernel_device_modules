ifneq ($(strip $(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)),)
ifneq ($(strip $(CONFIG_MTK_TINYSYS_SSPM_V1)),)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/sspm/v1/
endif
endif

ccflags-y += \
	-I$(srctree)/drivers/devfreq/						\
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/			\
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/spm_v4/		\
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/spm_v4/		\
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/vcorefs_v3/	\

obj-$(CONFIG_MTK_PMQOS)		+= mtk-pm-qos.o

obj-$(CONFIG_MTK_DVFSRC_V1)	+= mtk-helio-dvfsrc.o
#mtk-helio-dvfsrc-objs		+= helio-dvfsrc.o helio-dvfsrc-sysfs.o helio-dvfsrc-mt6771.o helio-dvfsrc-opp-mt6771.o
mtk-helio-dvfsrc-objs		+= helio-dvfsrc.o helio-dvfsrc-sysfs.o helio-dvfsrc-mt6771.o

