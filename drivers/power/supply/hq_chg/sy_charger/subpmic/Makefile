subdir-ccflags-y += -I$(srctree)/arch/arm64/boot/dts/mediatek/hq_chg
subdir-ccflags-y += -I$(srctree)/drivers/power/supply/hq_chg/charger_class
subdir-ccflags-y += -I$(srctree)/drivers/power/supply/hq_chg/sy_charger
subdir-ccflags-y += -I$(srctree)/drivers/power/supply/hq_chg/sy_charger/protocol
subdir-ccflags-y += -DCONFIG_ENABLE_SYSFS_DEBUG

obj-$(CONFIG_SY_SUBPMIC_DRIVER)    += subpmic_sy_charger.o subpmic_sy.o
