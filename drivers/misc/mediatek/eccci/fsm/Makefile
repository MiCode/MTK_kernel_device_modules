#
# Copyright (C) 2016 MediaTek Inc.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See http://www.gnu.org/licenses/gpl-2.0.html for more details.
#

ifeq ($(CONFIG_MTK_ECCCI_DRIVER), m)

ccflags-y += -DCCCI_KMODULE_ENABLE

#ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/eccci/inc
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/eccci/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/eccci/port
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/eccci/platform
#ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/scp/mt6873
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/scp/include/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/scp/rv/
#ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/eccci/hif/

endif

ifeq ($(CONFIG_MTK_ECCCI_DRIVER), y)

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/ccmni/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/eccci/inc
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/eccci/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/eccci/port
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/scp/mt6873
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/scp/include/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/scp/rv/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/eccci/hif/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/pbm/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mdpm/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/memory-amms/

# fsm objects


endif
