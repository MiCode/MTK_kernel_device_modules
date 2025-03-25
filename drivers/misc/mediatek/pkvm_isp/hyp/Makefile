# SPDX-License-Identifier: GPL-2.0
#
# Copyright (c) 2025 MediaTek Inc.
#

subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include

subdir-ccflags-y += -DIS_MULTI_SEC_PORT_SUPPORT
subdir-ccflags-y += -DMULTI_SENSOR_INDIVIDUAL_SUPPORT

# MTK_CAM_SECURE_CSI_PORT_FRONT = 0
# MTK_CAM_SECURE_CSI_PORT_REAR = 3
subdir-ccflags-y += -DSingle_Secure_csi_port_front=0
subdir-ccflags-y += -DMulti_Secure_csi_port_front=0
subdir-ccflags-y += -DMulti_Secure_csi_port_rear=3

hyp-obj-y := pkvm_isp_hyp.o secio_api.o seninf_auth.o seninf_drv_csi_info.o seninf_sec_drv.o sensor_cfg_sec.o
include $(srctree)/arch/arm64/kvm/hyp/nvhe/Makefile.module
