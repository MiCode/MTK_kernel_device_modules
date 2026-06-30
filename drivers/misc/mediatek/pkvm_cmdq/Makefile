# SPDX-License-Identifier: GPL-2.0
#
# Copyright (c) 2024 MediaTek Inc.
#

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include

obj-$(CONFIG_MTK_PKVM_CMDQ) += pkvm_cmdq.o

$(obj)/hyp/kvm_nvhe.o: FORCE
	$(Q)$(MAKE) $(build)=$(obj)/hyp $(obj)/hyp/kvm_nvhe.o

clean-files := hyp/hyp.lds hyp/hyp-reloc.S

pkvm_cmdq-objs += pkvm_cmdq_host.o hyp/kvm_nvhe.o
