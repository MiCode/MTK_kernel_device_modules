# debugging support (also enables trace_printk)
# ccflags-y += -Werror
# ccflags-y += $(call cc-option,-Wno-date-time,)

TYPEC_ACC_OBJS += typec_analog_acc.o

obj-$(CONFIG_SND_SOC_TYPEC_ANALOG_ACC) += typec_acc.o
typec_acc-y := $(TYPEC_ACC_OBJS)