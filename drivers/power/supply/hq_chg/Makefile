# SPDX-License-Identifier: GPL-2.0
subdir-ccflags-y += -I$(srctree)/drivers/power/supply/hq_chg
subdir-ccflags-y += -I$(srctree)/drivers/power/supply/hq_chg/charger_policy
subdir-ccflags-y += -I$(srctree)/drivers/power/supply/hq_chg/charger_class
subdir-ccflags-y += -I$(srctree)/drivers/power/supply/hq_chg/fuelgauge
subdir-ccflags-y += -I$(srctree)/drivers/power/supply/hq_chg/include
subdir-ccflags-y += -I$(srctree)/drivers/power/supply/hq_chg/common/include
ifeq ($(CONFIG_HQ_SYSFS_SUPPORT), m)
subdir-ccflags-y += -I$(srctree)/drivers/misc/hqsysfs
endif

obj-$(CONFIG_HUAQIN_CHARGER_COMMON)     += common/
obj-$(CONFIG_HUAQIN_CHARGER_MANAGER)    += charger_policy/
obj-$(CONFIG_HUAQIN_CHARGER_CLASS)      += charger_class/
obj-$(CONFIG_SC_CHARGER)                += sc_charger/
obj-$(CONFIG_NU_CHARGER)                += nu_charger/
obj-$(CONFIG_SY_CHARGER)                += sy_charger/
obj-$(CONFIG_BQ_CHARGERPUMP_BQ25960)    += bq_charger/
obj-y                                   += fuelgauge/
#obj-y                                   += battery_secrete/
obj-y                                   +=  tiny_battery_secrete/


ifeq ($(KERNEL_FACTORY_BUILD),1)
KBUILD_CFLAGS += -DKERNEL_FACTORY_BUILD=1
endif
