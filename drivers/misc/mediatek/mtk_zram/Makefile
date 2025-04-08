# Copyright (C) 2024 MediaTek Inc.
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
ccflags-y += -I$(src)              	# needed for trace events
ccflags-y += -I$(srctree)/mm
ccflags-y += -I$(DEVICE_MODULES_PATH)/include/linux
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mtk_zram
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mtk_zram/hw

##ccflags-y += -DFPGA_EMULATION		# For FPGA emulation
##ccflags-y += -DENGINE_BYPASS_SMMU	# Bypass SMMU
##ccflags-y += -DZRAM_ENGINE_DEBUG	# Dump debug log
#
#obj-$(CONFIG_MTK_ZRAM) += mtk_zram.o
#obj-$(CONFIG_MTK_ZRAM_ENGINE) += mtk_hwz.o
#no_dst_copy-$(CONFIG_HWCOMP_SUPPORT_NO_DST_COPY) := hw/engine_no_dst_copy.o
#mtk_zram-objs += zcomp.o mtk_zram_drv.o
#mtk_hwz-objs += hw/zram_engine.o $(no_dst_copy-y) hw/engine_dst_copy.o hw/helpers.o hw/engine_control.o hw/engine_gears_control.o $(sw_simulation-y)
