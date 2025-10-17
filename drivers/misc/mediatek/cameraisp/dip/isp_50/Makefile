#
# Copyright (C) 2015 MediaTek Inc.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#

$(info $(DEVICE_MODULES_PATH))

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/m4u/$(MTK_PLATFORM)/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mmp/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/cmdq/v3/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/staging/android/mtk_ion \
             -I$(DEVICE_MODULES_PATH)/drivers/staging/android/mtk_ion/mtk
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/smi/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mdp/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/cmdq/v3/$(MTK_PLATFORM)/
ccflags-$(CONFIG_MTK_IOMMU_V2) += -I$(DEVICE_MODULES_PATH)/drivers/iommu
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/cmdq/mailbox/

obj-$(CONFIG_MTK_CAMERA_DIP_ISP50) += camera_dip_isp50.o
camera_dip_isp50-objs += camera_dip.o
