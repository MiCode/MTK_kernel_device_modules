# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2020 MediaTek Inc.

ifeq ($(CONFIG_MTK_CAM_CAL), m)
obj-$(CONFIG_MTK_CAM_CAL) += src/
endif

ifeq ($(filter $(CONFIG_MTK_CAM_CAL_ISP4), m y), $(CONFIG_MTK_CAM_CAL_ISP4))
obj-$(CONFIG_MTK_CAM_CAL_ISP4) += src/
endif

ifeq ($(filter $(CONFIG_MTK_CAM_CAL_ISP4_C), m y), $(CONFIG_MTK_CAM_CAL_ISP4_C))
obj-$(CONFIG_MTK_CAM_CAL_ISP4_C) += src/
endif

ifeq ($(filter $(CONFIG_MTK_CAM_CAL_ISP6S_MON), m), $(CONFIG_MTK_CAM_CAL_ISP6S_MON))
obj-$(CONFIG_MTK_CAM_CAL_ISP6S_MON) += src/
endif