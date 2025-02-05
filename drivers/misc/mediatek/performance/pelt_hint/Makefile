ccflags-y += \
        -I$(srctree)/include/ \
        -I$(srctree)/kernel/ \
        -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/performance/pelt_hint/include/ \

obj-$(CONFIG_MTK_PELT_HINT) += mtk_pelt_hint.o
mtk_pelt_hint-y += src/pelt_hint_main.o
mtk_pelt_hint-y += src/pelt_hint_sysfs.o
