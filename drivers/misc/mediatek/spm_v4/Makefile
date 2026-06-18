
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/spm_v4/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/spm_v4/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/vcorefs_v3/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/eem_v1/inc/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/cpuidle/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat/mt6739/include/mach/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/power_gs_v1/

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/irq/mt6739/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/dramc/mt6739/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/thermal/fakeHeader/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/pmic_wrap/mt6739/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/pmic/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/pmic/include/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/pmic/include/mach/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/pmic/include/mt6356/

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/video/mt6739/videox/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/video/mt6739/dispsys/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/video/mt6739/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/video/include/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/cmdq/v3/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/cmdq/v3/mt6739/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/lcm/inc/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/hps_v3/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mcdi/include
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/watchdog/mediatek/include/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/scsi/ufs/

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/power/mt6739/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mt6739/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mmdvfs/

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/gpu/drm/mediatek/mediatek_v2 \

ifneq ($(CONFIG_DEVICE_MODULES_USB_MTU3), )
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/usb/mtu3
endif

ifeq ($(CONFIG_MTK_SND_SOC_NEW_ARCH), y)
ccflags-y += -I$(DEVICE_MODULES_PATH)/sound/soc/mediatek/mt6739/
ccflags-y += -I$(DEVICE_MODULES_PATH)/sound/soc/mediatek/common_int/
endif

ifneq ($(strip $(CONFIG_MTK_PLAT_POWER_MT8788)),)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/sspm/
endif

ifneq ($(strip $(CONFIG_SND_SOC_MT6771)),)
subdir-ccflags-y += -Werror -I$(DEVICE_MODULES_PATH)/sound/soc/mediatek/mt6771
endif

ifneq ($(strip $(CONFIG_MTK_DVFSRC_V1)),)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/helio-dvfsrc/
endif

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/smi/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/devfreq/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/clkbuf_v1/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/eem_v2/mt6739/inc/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/cpuidle_v3/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/cpufreq_v2/src/mach/mt6739/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/mmc/host/

ifneq ($(CONFIG_DEVICE_MODULES_USB_MTU3), )
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/usb/mtu3
endif

obj-$(CONFIG_MTK_SPM_V4) += MTK_INTERNAL_SPM.o
MTK_INTERNAL_SPM-y += mtk_idle.o
MTK_INTERNAL_SPM-y += mtk_sleep.o
MTK_INTERNAL_SPM-y += mtk_spm_init.o
MTK_INTERNAL_SPM-y += mtk_spm.o
MTK_INTERNAL_SPM-y += mtk_spm_dpidle.o
MTK_INTERNAL_SPM-y += mtk_idle_profile.o
MTK_INTERNAL_SPM-y += mtk_spm_fs.o
MTK_INTERNAL_SPM-y += mtk_spm_internal.o
MTK_INTERNAL_SPM-y += mtk_spm_resource_req.o
MTK_INTERNAL_SPM-y += mtk_spm_sleep.o
MTK_INTERNAL_SPM-y += mtk_spm_sodi.o
MTK_INTERNAL_SPM-y += mtk_spm_sodi3p0.o
MTK_INTERNAL_SPM-y += mtk_lp_sysfs.o
MTK_INTERNAL_SPM-y += mtk_lp_kernfs.o
MTK_INTERNAL_SPM-y += mtk_idle_sysfs.o
MTK_INTERNAL_SPM-y += mtk_lp_sysfs_procfs.o
ifneq ($(strip $(CONFIG_MTK_PLAT_POWER_MT8788)),)
MTK_INTERNAL_SPM-y += mtk_idle_mt6771.o
MTK_INTERNAL_SPM-y += mtk_spm_dpidle_mt6771.o
MTK_INTERNAL_SPM-y += mtk_spm_sleep_mt6771.o
MTK_INTERNAL_SPM-y += mtk_spm_sodi_mt6771.o
MTK_INTERNAL_SPM-y += mtk_spm_vcorefs_ipi_mt6771.o
MTK_INTERNAL_SPM-y += mtk_spm_vcorefs_mt6771.o
MTK_INTERNAL_SPM-y += helio-dvfsrc-opp-mt6771.o
else
MTK_INTERNAL_SPM-y += mtk_spm_vcorefs.o
MTK_INTERNAL_SPM-y += mtk_idle_platform.o
MTK_INTERNAL_SPM-y += mtk_spm_dpidle_platform.o
MTK_INTERNAL_SPM-y += mtk_spm_sleep_platform.o
MTK_INTERNAL_SPM-y += mtk_spm_sodi_platform.o
endif
MTK_INTERNAL_SPM-y += mtk_vcorefs_manager.o
MTK_INTERNAL_SPM-y += mtk_vcorefs_governor.o
