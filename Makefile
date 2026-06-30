# SPDX-License-Identifier: GPL-2.0

include $(wildcard $(KERNEL_SRC)/$(DEVICE_MODULES_REL_DIR)/Makefile.include.ddk)

modules modules_install headers_install clean:
	$(MAKE) -C $(KERNEL_SRC) M=$(M) KBUILD_EXTRA_SYMBOLS="$(EXTRA_SYMBOLS)" $(@)

modules_install: headers_install
