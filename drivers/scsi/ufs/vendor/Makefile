# SPDX-License-Identifier: GPL-2.0
# Mediatek UFS extended features makefile
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/scsi/ufs/vendor/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/scsi/ufs/
ifeq ($(CONFIG_SCSI_UFS_XIAOMI),n)
# must after -I$(DEVICE_MODULES_PATH)/drivers/scsi/ufs/
ccflags-y += -I$(srctree)/drivers/scsi/ufs/
ccflags-y += -I$(srctree)/drivers/ufs/core/
ccflags-y += -I$(srctree)/drivers/ufs/host/

obj-$(CONFIG_DEVICE_MODULES_SCSI_UFS_MEDIATEK) += ufs-mediatek-mod.o
ufs-mediatek-mod-y 				+= ../ufs-mediatek.o
else
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/scsi/ufs/xiaomi/core

obj-$(CONFIG_DEVICE_MODULES_SCSI_UFS_MEDIATEK) += ufs-mediatek-mod.o
ufs-mediatek-mod-y 				+= ../ufs-mediatek.o ../xiaomi/core/ufshcd.o \
									../xiaomi/core/ufs-sysfs.o ../xiaomi/core/ufs-mcq.o
ufs-mediatek-mod-$(CONFIG_SCSI_UFSHCD_PLATFORM) += ../xiaomi/core/ufshcd-pltfrm.o
ufs-mediatek-mod-$(CONFIG_DEBUG_FS)          += ../xiaomi/core/ufs-debugfs.o
ufs-mediatek-mod-$(CONFIG_SCSI_UFS_BSG)      += ../xiaomi/core/ufs_bsg.o
ufs-mediatek-mod-$(CONFIG_SCSI_UFS_CRYPTO)   += ../xiaomi/core/ufshcd-crypto.o
ufs-mediatek-mod-$(CONFIG_SCSI_UFS_HPB)      += ../xiaomi/core/ufshpb.o
ufs-mediatek-mod-$(CONFIG_SCSI_UFS_FAULT_INJECTION) += ../xiaomi/core/ufs-fault-injection.o
ufs-mediatek-mod-$(CONFIG_SCSI_UFS_HWMON)    += ../xiaomi/core/ufs-hwmon.o
endif
ufs-mediatek-mod-$(CONFIG_UFSFEATURE) 		+= ufsfeature.o ufsfeature-mimic.o
ufs-mediatek-mod-$(CONFIG_UFSSHPB) 		+= ufsshpb.o
ufs-mediatek-mod-$(CONFIG_UFSTW) 		+= ufstw.o
ufs-mediatek-mod-$(CONFIG_UFSHID) 		+= ufshid.o
ufs-mediatek-mod-$(CONFIG_UFSRINGBUF)		+= ufsringbuf.o
