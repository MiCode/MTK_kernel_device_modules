subdir-ccflags-y += -I$(srctree)/arch/arm64/boot/dts/mediatek/wt_chg
subdir-ccflags-y += -I$(srctree)/drivers/power/supply/wt_chg/sc_charger
subdir-ccflags-y += -I$(srctree)/drivers/power/supply/wt_chg/sc_charger/protocol
subdir-ccflags-y += -DCONFIG_ENABLE_BOOT_DEBUG -DCONFIG_ENABLE_SYSFS_DEBUG

obj-$(CONFIG_SUBPMIC_DRIVER)    += subpmic_charger.o subpmic.o subpmic_cid.o
