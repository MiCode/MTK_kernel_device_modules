subdir-ccflags-y += -I$(srctree)/include/
subdir-ccflags-y += -I$(srctree)/kernel/

subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/sched/

ccflags-y += -I$(srctree)/$(src)

obj-$(CONFIG_MTK_CPUQOS_V3) += cpuqos_v3.o
cpuqos_v3-y += cpuqos_v3_proto.o cpuqos_sys_common.o
