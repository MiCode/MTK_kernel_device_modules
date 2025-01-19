# Copyright (C) 2019 MediaTek Inc.
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
#
demo_mkp = true
ifdef demo_mkp
ccflags-y += -DDEMO_MKP
ccflags-y += -DDEBUG_MKP_ENABLED
endif

subdir-ccflags-y += -I$(srctree)/drivers/android/ # use debug_kinfo
subdir-ccflags-y += -Wno-unused-but-set-variable

ccflags-y += -I$(srctree)/$(src)              # needed for trace events
ccflags-y += -I$(srctree)/arch/arm/include/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/geniezone/public
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mkp
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/aee/mrdump
ccflags-y += -I$(srctree)/security/selinux/include/
