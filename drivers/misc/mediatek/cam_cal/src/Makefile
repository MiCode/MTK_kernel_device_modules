# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2020 MediaTek Inc.
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#

ifeq ($(CONFIG_MTK_CAM_CAL), m)
obj-$(CONFIG_MTK_CAM_CAL) += custom/
endif

ifeq ($(filter $(CONFIG_MTK_CAM_CAL_ISP4_T), m y), $(CONFIG_MTK_CAM_CAL_ISP4_T))
obj-$(CONFIG_MTK_CAM_CAL_ISP4_T) += isp4_t/
endif

ifeq ($(filter $(CONFIG_MTK_CAM_CAL_ISP4_C), m y), $(CONFIG_MTK_CAM_CAL_ISP4_C))
obj-$(CONFIG_MTK_CAM_CAL_ISP4_C) += isp4_c/
endif

ifeq ($(filter $(CONFIG_MTK_CAM_CAL_ISP6S_MON), m), $(CONFIG_MTK_CAM_CAL_ISP6S_MON))
obj-$(CONFIG_MTK_CAM_CAL_ISP6S_MON) += isp6s_mon/
endif
