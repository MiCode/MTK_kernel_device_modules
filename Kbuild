# SPDX-License-Identifier: GPL-2.0

ifneq ($(CONFIG_DEVICE_MODULES_ALLOW_BUILTIN),y)

LINUXINCLUDE := $(DEVCIE_MODULES_INCLUDE) $(LINUXINCLUDE)

subdir-ccflags-y += -Wall -Werror \
		-I$(srctree)/$(src)/include \
		-I$(srctree)/$(src)/include/uapi \

obj-y += drivers/memory/

obj-y += drivers/edac/

obj-y += drivers/iio/adc/

obj-y += drivers/mfd/

obj-y += drivers/nvmem/

obj-y += drivers/dma/mediatek/

obj-y += drivers/ufs/

obj-y += drivers/char/

obj-y += drivers/clk/mediatek/

obj-y += drivers/soc/mediatek/

obj-y += drivers/regulator/

obj-y += drivers/leds/

obj-y += drivers/power/supply/

obj-y += drivers/rtc/

obj-y += drivers/remoteproc/

#obj-y += drivers/rpmsg/

obj-y += drivers/thermal/mediatek/

obj-y += drivers/spmi/

obj-y += drivers/tty/serial/8250/

obj-y += drivers/mailbox/

obj-y += drivers/interconnect/

obj-y += drivers/i2c/busses/

obj-y += drivers/pwm/

obj-y += drivers/mmc/host/

obj-y += drivers/tee/

obj-y += drivers/gpu/drm/mediatek/

obj-y += drivers/input/touchscreen/

obj-y += drivers/gpu/drm/panel/

obj-y += drivers/gpu/drm/bridge/

obj-y += drivers/gpu/mediatek/

obj-y += drivers/media/platform/

obj-y += drivers/net/ethernet/stmicro/

obj-y += drivers/net/phy/

obj-y += drivers/devfreq/

obj-y += drivers/misc/mediatek/

obj-y += sound/soc/codecs/

obj-y += sound/soc/mediatek/

obj-y += sound/virtio/

obj-y += drivers/pci/controller/

obj-y += drivers/media/virtio/

obj-y += drivers/video/backlight/

obj-y += drivers/hooks/geniezone_vendor_hooks/

endif
