# SPDX-License-Identifier: GPL-2.0
ccflags-y += -I$(srctree)/drivers/power/supply/hq_chg/common/include
ccflags-y += -I$(srctree)/drivers/power/supply/hq_chg/include

obj-$(CONFIG_HUAQIN_VOTER)    += hq_voter.o

ifeq ($(CONFIG_MIEV),m)
obj-$(CONFIG_HQ_NOTIFY)       += hq_notify.o
endif