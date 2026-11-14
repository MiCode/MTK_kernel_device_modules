$(warning "start build CONFIG_INPUT_FS3XXX_HAPTIC")
ifneq ($(KERNELRELEASE),)
    obj-$(CONFIG_INPUT_FS3XXX_HAPTIC):=fshaptic_o.o
        fshaptic_o-objs := fshaptic.o fshaptic3001.o
    $(info "2nd\n")
else
    KDIR := /lib/modules/$(shell uname -r)/build
    PWD:=$(shell pwd)
    $(info "$(KDIR)\n")
    $(info "$(PWD)\n")
all:
        $(info "1st\n")
        make -C $(KDIR) M=$(PWD) modules
clean:
        rm -f *.ko *.o *.symvers *.mod *.mod.c *.mode.o *.order
endif
