# SPDX-License-Identifier: GPL-2.0
#
# Copyright (c) 2024 MediaTek Inc.
#

subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include

#subdir-ccflags-y += -DCMDQ_DEBUG
subdir-ccflags-y += -DCMDQ_IOVA

hyp-obj-y := pkvm_cmdq_hyp.o pkvm_cmdq_platform.o secio_api.o isp_sec_platform.o
include $(srctree)/arch/arm64/kvm/hyp/nvhe/Makefile.module
