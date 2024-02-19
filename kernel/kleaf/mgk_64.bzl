load("//kernel_device_modules-6.6:kernel/kleaf/mgk_modules.bzl",
     "mgk_module_outs",
     "mgk_module_eng_outs",
     "mgk_module_userdebug_outs",
     "mgk_module_user_outs"
)

load("@mgk_info//:dict.bzl",
    "DEFCONFIG_OVERLAYS",
)

mgk_64_defconfig = "mgk_64_k66_defconfig"

mgk_64_kleaf_modules = [
    # keep sorted
    "//vendor/mediatek/kernel_modules/connectivity/bt/linux_v2:btmtk_uart_unify",
    "//vendor/mediatek/kernel_modules/connectivity/bt/mt66xx:btif",
    "//vendor/mediatek/kernel_modules/connectivity/bt/mt66xx/wmt:wmt",
    #"//vendor/mediatek/kernel_modules/connectivity/bt/mt76xx/sdio:btmtksdio",
    "//vendor/mediatek/kernel_modules/connectivity/common:wmt_drv",
    "//vendor/mediatek/kernel_modules/connectivity/connfem:connfem",
    "//vendor/mediatek/kernel_modules/connectivity/conninfra:conninfra",
    "//vendor/mediatek/kernel_modules/connectivity/fmradio:fmradio",
    "//vendor/mediatek/kernel_modules/connectivity/gps/data_link/plat/v010:gps_drv_dl_v010",
    "//vendor/mediatek/kernel_modules/connectivity/gps/data_link/plat/v030:gps_drv_dl_v030",
    "//vendor/mediatek/kernel_modules/connectivity/gps/data_link/plat/v050:gps_drv_dl_v050",
    "//vendor/mediatek/kernel_modules/connectivity/gps/data_link/plat/v051:gps_drv_dl_v051",
    "//vendor/mediatek/kernel_modules/connectivity/gps/data_link/plat/v060:gps_drv_dl_v060",
    "//vendor/mediatek/kernel_modules/connectivity/gps/data_link/plat/v061:gps_drv_dl_v061",
    "//vendor/mediatek/kernel_modules/connectivity/gps/data_link/plat/v062:gps_drv_dl_v062",
    "//vendor/mediatek/kernel_modules/connectivity/gps/gps_pwr:gps_pwr",
    "//vendor/mediatek/kernel_modules/connectivity/gps/gps_scp:gps_scp",
    "//vendor/mediatek/kernel_modules/connectivity/gps/gps_stp:gps_drv_stp",
    "//vendor/mediatek/kernel_modules/connectivity/wlan/adaptor/build/connac1x:wmt_chrdev_wifi",
    "//vendor/mediatek/kernel_modules/connectivity/wlan/adaptor/build/connac2x:wmt_chrdev_wifi_connac2",
    "//vendor/mediatek/kernel_modules/connectivity/wlan/adaptor/build/connac3x:wmt_chrdev_wifi_connac3",
    "//vendor/mediatek/kernel_modules/connectivity/wlan/adaptor/wlan_page_pool:wlan_page_pool",
    "//vendor/mediatek/kernel_modules/connectivity/wlan/core/gen4m/build/connac1x/6768:wlan_drv_gen4m_6768",
    "//vendor/mediatek/kernel_modules/connectivity/wlan/core/gen4m/build/connac2x/6897:wlan_drv_gen4m_6897",
    "//vendor/mediatek/kernel_modules/connectivity/wlan/core/gen4m/build/connac3x/6985_6639:wlan_drv_gen4m_6985_6639",
    "//vendor/mediatek/kernel_modules/connectivity/wlan/core/gen4m/build/connac3x/6989_6639:wlan_drv_gen4m_6989_6639",
    "//vendor/mediatek/kernel_modules/connectivity/wlan/core/gen4m/build/connac3x/6989_6639_dppm:wlan_drv_gen4m_6989_6639_dppm",
    "//vendor/mediatek/kernel_modules/connectivity/wlan/core/gen4m/build/connac3x/6991_6653:wlan_drv_gen4m_6991_6653",
    "//vendor/mediatek/kernel_modules/connectivity/wlan/core/gen4m/build/connac3x/6991_6653_2g2a:wlan_drv_gen4m_6991_6653_2g2a",
    "//vendor/mediatek/kernel_modules/connectivity/wlan/core/gen4m/build/connac3x/eap_6653:wlan_drv_gen4m_eap_6653",
    "//vendor/mediatek/kernel_modules/cpufreq_cus:cpu_freq",
    "//vendor/mediatek/kernel_modules/cpufreq_int:cpu_hwtest",
    "//vendor/mediatek/kernel_modules/fpsgo_cus:fpsgo_cus",
    "//vendor/mediatek/kernel_modules/fpsgo_int:fpsgo_int",
    "//vendor/mediatek/kernel_modules/afs_common_utils:jank_detection_common_utils",
    "//vendor/mediatek/kernel_modules/afs_core_int:jank_detection_core_int",
    "//vendor/mediatek/kernel_modules/afs_core_cus:jank_detection_core_cus",
    "//vendor/mediatek/kernel_modules/gpu:gpu",
    #"//vendor/mediatek/kernel_modules/hbt_driver_cus:hbt_cus",
    #"//vendor/mediatek/kernel_modules/hbt_driver:hbt_int",
    "//vendor/mediatek/kernel_modules/met_drv_secure_v3:met_drv_secure_v3",
    "//vendor/mediatek/kernel_modules/met_drv_v3/met_api:met_api_v3_cus",
    "//vendor/mediatek/kernel_modules/met_drv_v3/met_api:met_api_v3_int",
    "//vendor/mediatek/kernel_modules/met_drv_v3:met_drv_v3",
    #"//vendor/mediatek/kernel_modules/msync2_frd_cus/build:msync2_frd_cus",
    #"//vendor/mediatek/kernel_modules/msync2_frd_int:msync2_frd_int",
    "//vendor/mediatek/kernel_modules/mtk_input/FT3518U:ft3518u",
    "//vendor/mediatek/kernel_modules/mtk_input/GT9886:gt9886",
    "//vendor/mediatek/kernel_modules/mtk_input/GT9916:gt9916",
    "//vendor/mediatek/kernel_modules/mtk_input/NT36672C:nt36672c",
    "//vendor/mediatek/kernel_modules/mtk_input/nt36xxx_no_flash_spi:nt36xxx_no_flash_spi",
    "//vendor/mediatek/kernel_modules/mtk_input/ST61Y:st61y",
    "//vendor/mediatek/kernel_modules/mtk_input/fingerprint/goodix/5.10:gf_spi",
    "//vendor/mediatek/kernel_modules/mtk_input/synaptics_tcm:synaptics_tcm",
    "//vendor/mediatek/kernel_modules/mtkcam/camsys:camsys",
    "//vendor/mediatek/kernel_modules/mtkcam/cam_cal/src_v4l2/custom:mtk_cam_cal",
    "//vendor/mediatek/kernel_modules/mtkcam/ccusys:ccusys",
    "//vendor/mediatek/kernel_modules/mtkcam/imgsensor:mtk_imgsensor",
    "//vendor/mediatek/kernel_modules/mtkcam/imgsys/common:imgsys_common",
    "//vendor/mediatek/kernel_modules/mtkcam/imgsys/imgsys/isp8:imgsys_8",
    "//vendor/mediatek/kernel_modules/mtkcam/imgsys/imgsys/isp7sp:imgsys_7sp",
    "//vendor/mediatek/kernel_modules/mtkcam/imgsys/imgsys/isp7s:imgsys_7s",
    "//vendor/mediatek/kernel_modules/mtkcam/imgsys/imgsys/isp71:imgsys_71",
    "//vendor/mediatek/kernel_modules/mtkcam/imgsys/imgsys/cmdq/legacy:imgsys_cmdq",
    "//vendor/mediatek/kernel_modules/mtkcam/imgsys/imgsys/cmdq/isp8:imgsys_cmdq_isp8",
    "//vendor/mediatek/kernel_modules/mtkcam/mtk-aie:mtk-aie",
    "//vendor/mediatek/kernel_modules/mtkcam/mtk-mae:mtk_mae",
    "//vendor/mediatek/kernel_modules/mtkcam/mtk-hcp/legacy:mtk-hcp",
    "//vendor/mediatek/kernel_modules/mtkcam/mtk-hcp/isp8:mtk-hcp-isp8",
    "//vendor/mediatek/kernel_modules/mtkcam/mtk-ipesys-me:mtk-ipesys-me",
    "//vendor/mediatek/kernel_modules/mtkcam/mtk-dpe:mtk-dpe",
    "//vendor/mediatek/kernel_modules/mtkcam/mtk-pda:mtk-pda",
    #"//vendor/mediatek/kernel_modules/mtkcam/sched:c2ps",
    "//vendor/mediatek/kernel_modules/mtkcam/scpsys/mtk-aov:mtk_aov",
    "//vendor/mediatek/kernel_modules/sched_cus:sched_cus",
    "//vendor/mediatek/kernel_modules/sched_int:sched_int",
]

mgk_64_kleaf_eng_modules = [
    "//vendor/mediatek/tests/kernel/ktf_testcase:ktf_testcase",
    "//vendor/mediatek/tests/ktf/kernel:ktf",
]

mgk_64_kleaf_userdebug_modules = [
    "//vendor/mediatek/tests/kernel/ktf_testcase:ktf_testcase",
    "//vendor/mediatek/tests/ktf/kernel:ktf",
]

mgk_64_kleaf_user_modules = [
]


mgk_64_module_outs = [
]

mgk_64_common_modules = mgk_module_outs + mgk_64_module_outs
mgk_64_common_eng_modules = mgk_module_outs + mgk_module_eng_outs + mgk_64_module_outs
mgk_64_common_userdebug_modules = mgk_module_outs + mgk_module_userdebug_outs + mgk_64_module_outs
mgk_64_common_user_modules = mgk_module_outs + mgk_module_user_outs + mgk_64_module_outs


mgk_64_device_modules = [
    # keep sorted
    "drivers/char/hw_random/sec-rng.ko",
    "drivers/char/rpmb/rpmb.ko",
    "drivers/char/rpmb/rpmb-mtk.ko",
    "drivers/clk/mediatek/clk-bringup.ko",
    "drivers/clk/mediatek/clk-common.ko",
    "drivers/clk/mediatek/clk-disable-unused.ko",
    "drivers/clk/mediatek/fhctl.ko",
    #"drivers/clocksource/timer-mediatek.ko",
    "drivers/cpufreq/mediatek-cpufreq-hw.ko",
    "drivers/devfreq/mtk-dvfsrc-devfreq.ko",
    "drivers/dma-buf/heaps/deferred-free-helper.ko",
    "drivers/dma-buf/heaps/mtk_heap_debug.ko",
    "drivers/dma-buf/heaps/mtk_heap_refill.ko",
    "drivers/dma-buf/heaps/mtk_sec_heap.ko",
    "drivers/dma-buf/heaps/page_pool.ko",
    "drivers/dma-buf/heaps/system_heap.ko",
    "drivers/dma/mediatek/mtk-cqdma.ko",
    "drivers/dma/mediatek/mtk-uart-apdma.ko",
    "drivers/gpu/drm/mediatek/dpc/mtk_dpc.ko",
    "drivers/gpu/drm/mediatek/dpc/mtk_vdisp.ko",
    "drivers/gpu/drm/mediatek/mediatek_v2/mediatek-drm.ko",
    "drivers/gpu/drm/mediatek/mediatek_v2/mtk_aod_scp.ko",
    "drivers/gpu/drm/mediatek/mediatek_v2/mtk_disp_notify.ko",
    "drivers/gpu/drm/mediatek/mediatek_v2/mtk_disp_sec.ko",
    "drivers/gpu/drm/mediatek/mediatek_v2/mtk_panel_ext.ko",
    "drivers/gpu/drm/mediatek/mediatek_v2/mtk_sync.ko",
    "drivers/gpu/drm/mediatek/mml/mtk-mml.ko",
    "drivers/gpu/drm/panel/rt4831a.ko",
    "drivers/gpu/drm/panel/k6985v1_64_alpha/panel-nt37705-alpha-cmd.ko",
    "drivers/gpu/drm/panel/k6989v1_64_alpha/panel-ili7838e-alpha-cmd.ko",
    "drivers/gpu/drm/panel/k6989v1_64_alpha/panel-ili7838e-dv2-alpha-cmd.ko",
    "drivers/gpu/drm/panel/k6989v1_64_alpha/panel-ili7838e-dv2-spr-cmd.ko",
    "drivers/gpu/drm/panel/k6985v1_64/td2204-wqhd-amb678zy01-s6e3hc3-cmd.ko",
    "drivers/gpu/drm/panel/mediatek-cust-panel-sample.ko",
    "drivers/gpu/drm/panel/mediatek-drm-gateic.ko",
    "drivers/gpu/drm/panel/mediatek-drm-panel-drv.ko",
    "drivers/gpu/drm/panel/panel-alpha-jdi-nt36672c-cphy-vdo.ko",
    "drivers/gpu/drm/panel/panel-alpha-jdi-nt36672e-cphy-vdo.ko",
    "drivers/gpu/drm/panel/panel-alpha-jdi-nt36672e-vdo-120hz-hfp.ko",
    "drivers/gpu/drm/panel/panel-alpha-jdi-nt36672e-vdo-120hz.ko",
    "drivers/gpu/drm/panel/panel-alpha-jdi-nt36672e-vdo-144hz-hfp.ko",
    "drivers/gpu/drm/panel/panel-alpha-jdi-nt36672e-vdo-144hz.ko",
    "drivers/gpu/drm/panel/panel-alpha-jdi-nt36672e-vdo-60hz.ko",
    "drivers/gpu/drm/panel/panel-alpha-nt37706-vdo-120hz.ko",
    "drivers/gpu/drm/panel/panel-boe-jd9365da-vdo.ko",
    "drivers/gpu/drm/panel/panel-hx-nt37701-dphy-cmd.ko",
    "drivers/gpu/drm/panel/panel-hx-nt37701-dphy-cmd-120hz.ko",
    "drivers/gpu/drm/panel/panel-l12a-42-02-0a-dsc-cmd.ko",
    "drivers/gpu/drm/panel/panel-nt37801-cmd-120hz.ko",
    "drivers/gpu/drm/panel/panel-nt37801-cmd-fhd.ko",
    "drivers/gpu/drm/panel/panel-nt37801-cmd-ltpo.ko",
    "drivers/gpu/drm/panel/panel-nt37801-cmd-spr.ko",
    "drivers/gpu/drm/panel/panel-nt37801-cmd-fhd-plus.ko",
    "drivers/gpu/drm/panel/panel-samsung-ana6705-cmd.ko",
    "drivers/gpu/drm/panel/panel-samsung-ana6705-cmd-fhdp.ko",
    "drivers/gpu/drm/panel/panel-samsung-op-cmd.ko",
    "drivers/gpu/drm/panel/panel-samsung-op-cmd-msync2.ko",
    "drivers/gpu/drm/panel/panel-samsung-s68fc01-vdo-aod.ko",
    "drivers/gpu/drm/panel/panel-sc-nt36672c-vdo-120hz.ko",
    "drivers/gpu/drm/panel/panel-tianma-nt36672e-vdo-120hz-hfp.ko",
    "drivers/gpu/drm/panel/panel-tianma-r66451-cmd-120hz.ko",
    "drivers/gpu/drm/panel/panel-tianma-r66451-cmd-120hz-wa.ko",
    "drivers/gpu/drm/panel/panel-truly-ft8756-vdo.ko",
    "drivers/gpu/drm/panel/panel-truly-nt35595-cmd.ko",
    "drivers/gpu/drm/panel/panel-truly-td4330-cmd.ko",
    "drivers/gpu/drm/panel/panel-truly-td4330-vdo.ko",
    "drivers/gpu/drm/panel/panel-himax-hx83121a-vdo.ko",
    "drivers/gpu/drm/panel/panel-boe-ts127qfmll1dkp0.ko",
    "drivers/gpu/drm/panel/panel-hx83112b-auo-cmd-60hz-rt5081.ko",
    "drivers/gpu/drm/panel/panel-hx83112b-auo-vdo-60hz-rt5081.ko",
    "drivers/gpu/drm/panel/panel-td4320-fhdp-dsi-vdo-auo-rt5081.ko",
    "drivers/gpu/drm/panel/panel-sc-nt36672c-vdo-90hz-6382.ko",
    "drivers/gpu/drm/panel/panel-aw37501-i2c.ko",
    "drivers/gpu/drm/panel/panel-nt36672c-fhdp-dsi-vdo-dsc-txd-boe.ko",
    "drivers/gpu/mediatek/ged/ged.ko",
    "drivers/gpu/mediatek/gpu_bm/mtk_gpu_qos.ko",
    "drivers/gpu/mediatek/gpueb/gpueb.ko",
    "drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_wrapper.ko",
    "drivers/gpu/mediatek/hal/mtk_gpu_hal.ko",
    "drivers/i2c/busses/i2c-mt65xx.ko",
    "drivers/i3c/master/mtk-i3c-master-mt69xx.ko",
    "drivers/iio/adc/mt6338-auxadc.ko",
    "drivers/iio/adc/mt635x-auxadc.ko",
    "drivers/iio/adc/mt6360-adc.ko",
    "drivers/iio/adc/mt6370-adc.ko",
    "drivers/iio/adc/mt6375-adc.ko",
    "drivers/iio/adc/mt6379-adc.ko",
    "drivers/iio/adc/mt6375-auxadc.ko",
    "drivers/iio/adc/mt6577_auxadc.ko",
    "drivers/iio/adc/mt6681-auxadc.ko",
    "drivers/iio/adc/mtk-spmi-pmic-adc.ko",
    "drivers/iio/adc/rt9490-adc.ko",
    "drivers/input/keyboard/mtk-kpd.ko",
    "drivers/input/keyboard/mtk-pmic-keys.ko",
    "drivers/input/touchscreen/GT9895/gt9895.ko",
    "drivers/input/touchscreen/GT9896S/gt9896s.ko",
    "drivers/input/touchscreen/GT1151/gt1151.ko",
    "drivers/input/touchscreen/ILITEK/ilitek_i2c.ko",
    "drivers/input/touchscreen/k6985v1_64_alpha/tp_y761.ko",
    "drivers/input/touchscreen/tui-common.ko",
    "drivers/interconnect/mediatek/mmqos-common.ko",
    "drivers/interconnect/mediatek/mtk-emi.ko",
    "drivers/interconnect/mediatek/mtk-emibus-icc.ko",
    "drivers/iommu/arm/arm-smmu-v3/arm_smmu_v3.ko",
    "drivers/iommu/arm/arm-smmu-v3/mtk-smmuv3-lmu.ko",
    "drivers/iommu/arm/arm-smmu-v3/mtk-smmuv3-pmu.ko",
    "drivers/iommu/arm/arm-smmu-v3/mtk-smmuv3-mpam-mon.ko",
    "drivers/misc/mediatek/iommu/mtk_smmu_qos.ko",
    "drivers/iommu/mtk_iommu.ko",
    "drivers/leds/leds-mt6360.ko",
    "drivers/leds/leds-mtk-disp.ko",
    "drivers/leds/leds-mtk.ko",
    "drivers/leds/leds-mtk-pwm.ko",
    "drivers/leds/regulator-vibrator.ko",
    "drivers/leds/flash/leds-mt6370-flash.ko",
    "drivers/leds/flash/leds-mt6379.ko",
    "drivers/mailbox/mtk-ise-mailbox.ko",
    "drivers/mailbox/mtk-mbox-mailbox.ko",
    "drivers/media/platform/mtk-jpeg/mtk_jpeg.ko",
    "drivers/media/platform/mtk-vcodec/mtk-vcodec-common.ko",
    "drivers/media/platform/mtk-vcodec/mtk-vcodec-dec-v1.ko",
    "drivers/media/platform/mtk-vcodec/mtk-vcodec-dec-v2.ko",
    "drivers/media/platform/mtk-vcodec/mtk-vcodec-enc-v1.ko",
    "drivers/media/platform/mtk-vcodec/mtk-vcodec-enc-v2.ko",
    "drivers/media/platform/mtk-vcu/mtk-vcu.ko",
    "drivers/memory/mediatek/emi.ko",
    "drivers/memory/mediatek/emi-fake-eng.ko",
    "drivers/memory/mediatek/emi-mpu.ko",
    "drivers/memory/mediatek/emi-mpu-test.ko",
    "drivers/memory/mediatek/emi-mpu-test-v2.ko",
    "drivers/memory/mediatek/emi-slb.ko",
    "drivers/memory/mediatek/slc-parity.ko",
    "drivers/memory/mediatek/mtk_dramc.ko",
    "drivers/memory/mediatek/smpu.ko",
    "drivers/memory/mediatek/smpu-hook-v1.ko",
    "drivers/memory/mtk-smi.ko",
    "drivers/mfd/mt6338-core.ko",
    "drivers/mfd/mt6360-core.ko",
    "drivers/mfd/mt6370.ko",
    "drivers/mfd/mt6375.ko",
    "drivers/mfd/mt6379s.ko",
    "drivers/mfd/mt6397.ko",
    "drivers/mfd/mt63xx-debug.ko",
    "drivers/mfd/mt6681-core.ko",
    "drivers/mfd/mt6685-audclk.ko",
    "drivers/mfd/mt6685-core.ko",
    "drivers/mfd/mtk-spmi-pmic-debug.ko",
    "drivers/mfd/mtk-spmi-pmic.ko",
    "drivers/mfd/rt9490.ko",
    "drivers/misc/mediatek/adsp/adsp.ko",
    "drivers/misc/mediatek/adsp/v1/adsp-v1.ko",
    "drivers/misc/mediatek/adsp/v2/adsp-v2.ko",
    "drivers/misc/mediatek/aee/aed/aee_aed.ko",
    "drivers/misc/mediatek/aee/aed/aee_rs.ko",
    "drivers/misc/mediatek/aee/hangdet/aee_hangdet.ko",
    "drivers/misc/mediatek/aee/mrdump/mrdump.ko",
    "drivers/misc/mediatek/apusys/apusys.ko",
    "drivers/misc/mediatek/apusys/apu_aov.ko",
    "drivers/misc/mediatek/apusys/power/apu_top.ko",
    "drivers/misc/mediatek/apusys/sapu/sapu.ko",
    "drivers/misc/mediatek/atf/atf_logger.ko",
    "drivers/misc/mediatek/audio_ipi/audio_ipi.ko",
    "drivers/misc/mediatek/blocktag/blocktag.ko",
    "drivers/misc/mediatek/btif/common/btif_drv.ko",
    "drivers/misc/mediatek/cache-auditor/cpuqos_v3/cpuqos_v3.ko",
    "drivers/misc/mediatek/cameraisp/dpe/camera_dpe_isp70.ko",
    "drivers/misc/mediatek/cameraisp/pda/isp_71/camera_pda.ko",
    "drivers/misc/mediatek/cameraisp/src/isp_6s/camera_isp.ko",
    "drivers/misc/mediatek/cameraisp/src/isp_6s/cam_qos.ko",
    "drivers/misc/mediatek/camera_mem/camera_mem.ko",
    "drivers/misc/mediatek/cam_timesync/archcounter_timesync.ko",
    "drivers/misc/mediatek/ccci_util/ccci_util_lib.ko",
    "drivers/misc/mediatek/ccmni/ccmni.ko",
    "drivers/misc/mediatek/ccu/src/isp4/ccu_isp4.ko",
    "drivers/misc/mediatek/ccu/src/isp6s/ccu_isp6s.ko",
    "drivers/misc/mediatek/clkbuf/clkbuf.ko",
    "drivers/misc/mediatek/cmdq/mailbox/cmdq-sec-drv.ko",
    "drivers/misc/mediatek/cmdq/mailbox/cmdq-test.ko",
    "drivers/misc/mediatek/cmdq/mailbox/mtk-cmdq-drv-ext.ko",
    "drivers/misc/mediatek/cm_mgr/mtk_cm_ipi.ko",
    "drivers/misc/mediatek/cm_mgr/mtk_cm_mgr.ko",
    "drivers/misc/mediatek/connectivity/connadp.ko",
    "drivers/misc/mediatek/conn_scp/connscp.ko",
    "drivers/misc/mediatek/cpufreq_lite/cpudvfs.ko",
    "drivers/misc/mediatek/dcm/mtk_dcm.ko",
    "drivers/misc/mediatek/dvfsrc/mtk-dvfsrc-helper.ko",
    "drivers/misc/mediatek/eccci/ccci_auxadc.ko",
    "drivers/misc/mediatek/eccci/ccci_md_all.ko",
    "drivers/misc/mediatek/eccci/fsm/ccci_fsm_scp.ko",
    "drivers/misc/mediatek/eccci/hif/ccci_ccif.ko",
    "drivers/misc/mediatek/eccci/hif/ccci_cldma.ko",
    "drivers/misc/mediatek/eccci/hif/ccci_dpmaif.ko",
    "drivers/misc/mediatek/et/mtk_et.ko",
    "drivers/misc/mediatek/extcon/extcon-mtk-usb.ko",
    "drivers/misc/mediatek/flashlight/flashlight.ko",
    "drivers/misc/mediatek/flashlight/mtk-composite.ko",
    "drivers/misc/mediatek/flashlight/v4l2/k6983v1_64_alpha/sy7806.ko",
    "drivers/misc/mediatek/flashlight/v4l2/lm3643.ko",
    "drivers/misc/mediatek/flashlight/v4l2/lm3644.ko",
    "drivers/misc/mediatek/gate_ic/rt4831a_drv.ko",
    "drivers/misc/mediatek/geniezone/gz_main_mod.ko",
    "drivers/misc/mediatek/geniezone/gz-trusty/gz_ipc_mod.ko",
    "drivers/misc/mediatek/geniezone/gz-trusty/gz_irq_mod.ko",
    "drivers/misc/mediatek/geniezone/gz-trusty/gz_log_mod.ko",
    "drivers/misc/mediatek/geniezone/gz-trusty/gz_trusty_mod.ko",
    "drivers/misc/mediatek/geniezone/gz-trusty/gz_virtio_mod.ko",
    "drivers/misc/mediatek/geniezone/gz_tz_system.ko",
    "drivers/misc/mediatek/hw_sem/mtk-hw-semaphore.ko",
    "drivers/misc/mediatek/i3c_i2c_wrap/mtk-i3c-i2c-wrap.ko",
    "drivers/misc/mediatek/imgsensor/src/isp6s/imgsensor_isp6s.ko",
    "drivers/misc/mediatek/iommu/mtk_iommu_util.ko",
    "drivers/misc/mediatek/iommu/iommu_debug.ko",
    "drivers/misc/mediatek/iommu/iommu_engine.ko",
    "drivers/misc/mediatek/iommu/iommu_secure.ko",
    "drivers/misc/mediatek/iommu/iommu_test.ko",
    "drivers/misc/mediatek/iommu/smmu_secure.ko",
    "drivers/misc/mediatek/ips/mtk-ips-helper.ko",
    "drivers/misc/mediatek/irtx/mtk_irtx_pwm.ko",
    "drivers/misc/mediatek/ise_lpm/ise_lpm.ko",
    "drivers/misc/mediatek/jpeg/jpeg-driver.ko",
    "drivers/misc/mediatek/lens/ois/bu63169/bu63169.ko",
    "drivers/misc/mediatek/lens/vcm/proprietary/main2/main2af.ko",
    "drivers/misc/mediatek/lens/vcm/proprietary/main3/main3af.ko",
    "drivers/misc/mediatek/lens/vcm/proprietary/main/mainaf.ko",
    "drivers/misc/mediatek/lens/vcm/proprietary/sub2/sub2af.ko",
    "drivers/misc/mediatek/lens/vcm/proprietary/sub/subaf.ko",
    "drivers/misc/mediatek/lens/vcm/v4l2/ak7375c/ak7375c.ko",
    "drivers/misc/mediatek/lens/vcm/v4l2/ak7377a/ak7377a.ko",
    "drivers/misc/mediatek/lens/vcm/v4l2/bu64253gwz/bu64253gwz.ko",
    "drivers/misc/mediatek/lens/vcm/v4l2/dw9718/dw9718.ko",
    "drivers/misc/mediatek/lens/vcm/v4l2/dw9800v/dw9800v.ko",
    "drivers/misc/mediatek/lens/vcm/v4l2/dw9800w/dw9800w.ko",
    "drivers/misc/mediatek/lens/vcm/v4l2/gt9764/gt9764.ko",
    "drivers/misc/mediatek/lens/vcm/v4l2/gt9772a/gt9772a.ko",
    "drivers/misc/mediatek/lens/vcm/v4l2/gt9772b/gt9772b.ko",
    "drivers/misc/mediatek/lens/vcm/v4l2/lc898229/lc898229.ko",
    "drivers/misc/mediatek/lens/vcm/v4l2/main_vcm/main_vcm.ko",
    "drivers/misc/mediatek/lens/vcm/v4l2/main2_vcm/main2_vcm.ko",
    "drivers/misc/mediatek/lens/vcm/v4l2/main3_vcm/main3_vcm.ko",
    "drivers/misc/mediatek/lens/vcm/v4l2/main4_vcm/main4_vcm.ko",
    "drivers/misc/mediatek/lens/ois/dw9781c/dw9781c.ko",
    "drivers/misc/mediatek/lens/ois/dw9781d/dw9781d.ko",
    "drivers/misc/mediatek/lens/vcm/v4l2/media/camera_af_media.ko",
    "drivers/misc/mediatek/lens/vcm/v4l2/sub_vcm/sub_vcm.ko",
    "drivers/misc/mediatek/log_store/log_store.ko",
    "drivers/misc/mediatek/lpm/governors/MHSP/lpm-gov-MHSP.ko",
    "drivers/misc/mediatek/lpm/modules/debug/v1/mtk-lpm-dbg-common-v1.ko",
    "drivers/misc/mediatek/lpm/modules/debug/v2/mtk-lpm-dbg-common-v2.ko",
    "drivers/misc/mediatek/lpm/modules/platform/v1/mtk-lpm-plat-v1.ko",
    "drivers/misc/mediatek/lpm/modules/platform/v2/mtk-lpm-plat-v2.ko",
    "drivers/misc/mediatek/lpm/mtk-lpm.ko",
    "drivers/misc/mediatek/masp/sec.ko",
    "drivers/misc/mediatek/mbraink/mtk_mbraink.ko",
    "drivers/misc/mediatek/mcupm/v2/mcupm.ko",
    "drivers/misc/mediatek/mddp/mddp.ko",
    "drivers/misc/mediatek/mdp/cmdq_helper_inf.ko",
    "drivers/misc/mediatek/mdpm/mtk_mdpm.ko",
    #"drivers/misc/mediatek/mkp/mkp.ko",
    "drivers/misc/mediatek/mmdebug/mtk-mmdebug-vcp.ko",
    "drivers/misc/mediatek/memory-amms/memory-amms.ko",
    "drivers/misc/mediatek/mmdvfs/mtk-mmdvfs-ccu.ko",
    "drivers/misc/mediatek/mmdvfs/mtk-mmdvfs-debug.ko",
    "drivers/misc/mediatek/mmdvfs/mtk-mmdvfs-ftrace.ko",
    "drivers/misc/mediatek/mmdvfs/mtk-mmdvfs-v3-start.ko",
    "drivers/misc/mediatek/mminfra/mtk-mminfra-debug.ko",
    "drivers/misc/mediatek/mminfra/mm-fake-engine.ko",
    "drivers/misc/mediatek/mminfra/mtk-mminfra-imax.ko",
    "drivers/misc/mediatek/mmp/src/mmprofile.ko",
    "drivers/misc/mediatek/mmqos/mmqos_wrapper.ko",
    "drivers/misc/mediatek/mmstat/trace_mmstat.ko",
    "drivers/misc/mediatek/monitor_hang/monitor_hang.ko",
    "drivers/misc/mediatek/mtk-interconnect/mtk-icc-core.ko",
    "drivers/misc/mediatek/mtprintk/mtk_printk_ctrl.ko",
    "drivers/misc/mediatek/mtprof/bootprof.ko",
    "drivers/misc/mediatek/nfc/st21nfc/st21nfc.ko",
    "drivers/misc/mediatek/nfc/st54spi.ko",
    "drivers/misc/mediatek/pbm/mtk_pbm.ko",
    "drivers/misc/mediatek/pbm/mtk_peak_power_budget.ko",
    "drivers/misc/mediatek/pcie/mtk_pcie_smt.ko",
    "drivers/misc/mediatek/cg_ppt/mtk_cg_peak_power_throttling.ko",
    "drivers/misc/mediatek/perf_common/mtk_perf_common.ko",
    "drivers/misc/mediatek/performance/fpsgo_v3/mtk_fpsgo.ko",
    "drivers/misc/mediatek/performance/frs/frs.ko",
    "drivers/misc/mediatek/performance/load_track/load_track.ko",
    "drivers/misc/mediatek/performance/mtk_ioctl_touch_boost.ko",
    "drivers/misc/mediatek/performance/mtk_ioctl_powerhal.ko",
    "drivers/misc/mediatek/performance/mtk_perf_ioctl.ko",
    "drivers/misc/mediatek/performance/mtk_perf_ioctl_magt.ko",
    "drivers/misc/mediatek/performance/powerhal_cpu_ctrl/powerhal_cpu_ctrl.ko",
    "drivers/misc/mediatek/performance/touch_boost/touch_boost.ko",
    "drivers/misc/mediatek/performance/uload_ind/uload_ind.ko",
    "drivers/misc/mediatek/pgboost/pgboost.ko",
    "drivers/misc/mediatek/pidmap/pidmap.ko",
    "drivers/misc/mediatek/pmic_protect/mtk-pmic-oc-debug.ko",
    "drivers/misc/mediatek/pmsr/pmsr.ko",
    "drivers/misc/mediatek/pmsr/twam/spmtwam.ko",
    "drivers/misc/mediatek/pmsr/v2/pmsr_v2.ko",
    "drivers/misc/mediatek/pmsr/v3/pmsr_v3.ko",
    "drivers/misc/mediatek/power_throttling/mtk_battery_oc_throttling.ko",
    "drivers/misc/mediatek/power_throttling/mtk_bp_thl.ko",
    "drivers/misc/mediatek/power_throttling/mtk_cpu_power_throttling.ko",
    "drivers/misc/mediatek/power_throttling/mtk_gpu_power_throttling.ko",
    "drivers/misc/mediatek/power_throttling/mtk_dynamic_loading_throttling.ko",
    "drivers/misc/mediatek/power_throttling/mtk_low_battery_throttling.ko",
    "drivers/misc/mediatek/power_throttling/mtk_md_power_throttling.ko",
    "drivers/misc/mediatek/power_throttling/pmic_lbat_service.ko",
    "drivers/misc/mediatek/power_throttling/pmic_lvsys_notify.ko",
    "drivers/misc/mediatek/power_throttling/pmic_dual_lbat_service.ko",
    "drivers/misc/mediatek/pwm/mtk-pwm.ko",
    "drivers/misc/mediatek/qos/mtk_qos.ko",
    "drivers/misc/mediatek/rps/rps_perf.ko",
    "drivers/misc/mediatek/sched/cpufreq_sugov_ext.ko",
    "drivers/misc/mediatek/sched/mtk_core_ctl.ko",
    "drivers/misc/mediatek/sched/scheduler.ko",
    "drivers/misc/mediatek/scp/rv/scp.ko",
    "drivers/misc/mediatek/sda/btm/bus_tracer_interface.ko",
    "drivers/misc/mediatek/sda/btm/v1/bus_tracer_v1.ko",
    "drivers/misc/mediatek/sda/bus-parity.ko",
    "drivers/misc/mediatek/sda/cache-parity.ko",
	"drivers/misc/mediatek/sda/gic-ram-parity.ko",
    "drivers/misc/mediatek/sda/dbg_error_flag.ko",
    "drivers/misc/mediatek/sda/dbgtop-drm.ko",
    "drivers/misc/mediatek/sda/irq-dbg.ko",
    "drivers/misc/mediatek/sda/last_bus.ko",
    "drivers/misc/mediatek/sda/systracker.ko",
    "drivers/misc/mediatek/sensor/2.0/core/hf_manager.ko",
    "drivers/misc/mediatek/sensor/2.0/sensorhub/sensorhub.ko",
    "drivers/misc/mediatek/slbc/mmsram.ko",
    "drivers/misc/mediatek/slbc/mtk_slbc.ko",
    "drivers/misc/mediatek/slbc/slbc_ipi.ko",
    "drivers/misc/mediatek/slbc/slbc_trace.ko",
    "drivers/misc/mediatek/smi/mtk-smi-dbg.ko",
    "drivers/misc/mediatek/ssc/debug/v1/mtk-ssc-dbg-v1.ko",
    "drivers/misc/mediatek/ssc/debug/v2/mtk-ssc-dbg-v2.ko",
    "drivers/misc/mediatek/ssc/mtk-ssc.ko",
    "drivers/misc/mediatek/sspm/v3/sspm_v3.ko",
    "drivers/misc/mediatek/subpmic/extdev_io_class.ko",
    "drivers/misc/mediatek/subpmic/mt6370-dbg.ko",
    "drivers/misc/mediatek/subpmic/subpmic-dbg.ko",
    "drivers/misc/mediatek/swpm/modules/debug/v1/mtk-swpm-dbg-common-v1.ko",
    "drivers/misc/mediatek/swpm/modules/debug/v6886/mtk-swpm-core-dbg-v6886.ko",
    "drivers/misc/mediatek/swpm/modules/debug/v6886/mtk-swpm-cpu-dbg-v6886.ko",
    "drivers/misc/mediatek/swpm/modules/debug/v6886/mtk-swpm-dbg-v6886.ko",
    "drivers/misc/mediatek/swpm/modules/debug/v6886/mtk-swpm-mem-dbg-v6886.ko",
    "drivers/misc/mediatek/swpm/modules/debug/v6897/mtk-swpm-audio-dbg-v6897.ko",
    "drivers/misc/mediatek/swpm/modules/debug/v6897/mtk-swpm-core-dbg-v6897.ko",
    "drivers/misc/mediatek/swpm/modules/debug/v6897/mtk-swpm-cpu-dbg-v6897.ko",
    "drivers/misc/mediatek/swpm/modules/debug/v6897/mtk-swpm-dbg-v6897.ko",
    "drivers/misc/mediatek/swpm/modules/debug/v6897/mtk-swpm-mem-dbg-v6897.ko",
    "drivers/misc/mediatek/swpm/modules/debug/v6983/mtk-smap-dbg-v6983.ko",
    "drivers/misc/mediatek/swpm/modules/debug/v6983/mtk-swpm-core-dbg-v6983.ko",
    "drivers/misc/mediatek/swpm/modules/debug/v6983/mtk-swpm-cpu-dbg-v6983.ko",
    "drivers/misc/mediatek/swpm/modules/debug/v6983/mtk-swpm-dbg-v6983.ko",
    "drivers/misc/mediatek/swpm/modules/debug/v6983/mtk-swpm-mem-dbg-v6983.ko",
    "drivers/misc/mediatek/swpm/modules/debug/v6985/mtk-smap-dbg-v6985.ko",
    "drivers/misc/mediatek/swpm/modules/debug/v6985/mtk-swpm-core-dbg-v6985.ko",
    "drivers/misc/mediatek/swpm/modules/debug/v6985/mtk-swpm-cpu-dbg-v6985.ko",
    "drivers/misc/mediatek/swpm/modules/debug/v6985/mtk-swpm-dbg-v6985.ko",
    "drivers/misc/mediatek/swpm/modules/debug/v6985/mtk-swpm-mem-dbg-v6985.ko",
    "drivers/misc/mediatek/swpm/modules/debug/v6989/mtk-swpm-audio-dbg-v6989.ko",
    "drivers/misc/mediatek/swpm/modules/debug/v6989/mtk-swpm-core-dbg-v6989.ko",
    "drivers/misc/mediatek/swpm/modules/debug/v6989/mtk-swpm-cpu-dbg-v6989.ko",
    "drivers/misc/mediatek/swpm/modules/debug/v6989/mtk-swpm-dbg-v6989.ko",
    "drivers/misc/mediatek/swpm/modules/debug/v6989/mtk-swpm-disp-dbg-v6989.ko",
    "drivers/misc/mediatek/swpm/modules/debug/v6989/mtk-swpm-mem-dbg-v6989.ko",
    "drivers/misc/mediatek/swpm/modules/debug/v6989/mtk-swpm-mml-dbg-v6989.ko",
    "drivers/misc/mediatek/swpm/mtk-swpm.ko",
    "drivers/misc/mediatek/swpm/mtk-swpm-perf-arm-pmu.ko",
    #"drivers/misc/mediatek/task_turbo/task_turbo.ko",
    "drivers/misc/mediatek/tinysys_scmi/tinysys-scmi.ko",
    "drivers/misc/mediatek/trusted_mem/ffa_v11.ko",
    "drivers/misc/mediatek/trusted_mem/tmem_ffa.ko",
    "drivers/misc/mediatek/trusted_mem/trusted_mem.ko",
    "drivers/misc/mediatek/trusty/ise-trusty.ko",
    "drivers/misc/mediatek/trusty/ise-trusty-ipc.ko",
    "drivers/misc/mediatek/trusty/ise-trusty-log.ko",
    "drivers/misc/mediatek/trusty/ise-trusty-virtio.ko",
    "drivers/misc/mediatek/typec/mux/fusb304.ko",
    "drivers/misc/mediatek/typec/mux/mux_switch.ko",
    "drivers/misc/mediatek/typec/mux/ps5169.ko",
    "drivers/misc/mediatek/typec/mux/ps5170.ko",
    "drivers/misc/mediatek/typec/mux/ptn36241g.ko",
    "drivers/misc/mediatek/typec/mux/usb_dp_selector.ko",
    "drivers/misc/mediatek/typec/tcpc/pd_dbg_info.ko",
    "drivers/misc/mediatek/typec/tcpc/rt_pd_manager.ko",
    "drivers/misc/mediatek/typec/tcpc/tcpc_class.ko",
    "drivers/misc/mediatek/typec/tcpc/tcpci_late_sync.ko",
    "drivers/misc/mediatek/typec/tcpc/tcpc_mt6360.ko",
    "drivers/misc/mediatek/typec/tcpc/tcpc_mt6370.ko",
    "drivers/misc/mediatek/typec/tcpc/tcpc_mt6375.ko",
    "drivers/misc/mediatek/typec/tcpc/tcpc_mt6379.ko",
    "drivers/misc/mediatek/typec/tcpc/tcpc_rt1711h.ko",
    "drivers/misc/mediatek/uarthub/uarthub_drv.ko",
    "drivers/misc/mediatek/usb/c2k_usb/c2k_usb.ko",
    "drivers/misc/mediatek/usb/c2k_usb/c2k_usb_f_via_atc.ko",
    "drivers/misc/mediatek/usb/c2k_usb/c2k_usb_f_via_ets.ko",
    "drivers/misc/mediatek/usb/c2k_usb/c2k_usb_f_via_modem.ko",
    "drivers/misc/mediatek/usb/usb_boost/usb_boost.ko",
    "drivers/misc/mediatek/usb/usb_boost/musb_boost.ko",
    "drivers/misc/mediatek/usb/usb_meta/usb_meta.ko",
    "drivers/misc/mediatek/usb/usb_offload/usb_offload.ko",
    "drivers/misc/mediatek/usb/usb_rndis/mtk_u_ether.ko",
    "drivers/misc/mediatek/usb/usb_rndis/mtk_usb_f_rndis.ko",
    "drivers/misc/mediatek/usb/usb_xhci/xhci-mtk-hcd-v2.ko",
    "drivers/misc/mediatek/usb/usb20/musb_main/musb_main.ko",
    "drivers/misc/mediatek/usb/usb20/musb_hdrc.ko",
    "drivers/misc/mediatek/vcp/rv/vcp.ko",
    "drivers/misc/mediatek/vcp/rv/vcp_status.ko",
    "drivers/misc/mediatek/vcp/rv_v2/vcp_v2.ko",
    "drivers/misc/mediatek/vcp/rv_v2/vcp_status_v2.ko",
    "drivers/misc/mediatek/vdec_fmt/vdec-fmt.ko",
    "drivers/misc/mediatek/vmm/mtk-vmm-notifier.ko",
    "drivers/misc/mediatek/vmm_spm/mtk-vmm-spm.ko",
    "drivers/misc/mediatek/vow/ver02/mtk-vow.ko",
    "drivers/misc/mediatek/widevine_drm/widevine_driver.ko",
    "drivers/misc/mediatek/wlcdrv/wlcdrv.ko",
    "drivers/mmc/host/cqhci.ko",
    "drivers/mmc/host/mtk-mmc-dbg.ko",
    "drivers/mmc/host/mtk-mmc.ko",
    "drivers/mmc/host/mtk-sd.ko",
    "drivers/mmc/host/mtk-mmc-wp.ko",
    "drivers/nvmem/nvmem-mt6338-efuse.ko",
    "drivers/nvmem/nvmem-mt635x-efuse.ko",
    "drivers/nvmem/nvmem-mt6681-efuse.ko",
    "drivers/nvmem/nvmem_mtk-devinfo.ko",
    "drivers/pci/controller/pcie-mediatek-gen3.ko",
    "drivers/phy/mediatek/phy-mtk-fpgaphy.ko",
    "drivers/phy/mediatek/phy-mtk-pcie.ko",
    "drivers/phy/mediatek/phy-mtk-tphy.ko",
    "drivers/phy/mediatek/phy-mtk-ufs.ko",
    "drivers/phy/mediatek/phy-mtk-xsphy.ko",
    "drivers/phy/mediatek/phy-mtk-nxp-eusb2-repeater.ko",
    "drivers/phy/mediatek/phy-mtk-mt6379-eusb2-repeater.ko",
    "drivers/pinctrl/mediatek/pinctrl-mt6363.ko",
    "drivers/pinctrl/mediatek/pinctrl-mt6373.ko",
    "drivers/pinctrl/mediatek/pinctrl-mtk-common-v2_debug.ko",
    "drivers/pinctrl/mediatek/pinctrl-mtk-v2.ko",
    "drivers/power/supply/adapter_class.ko",
    "drivers/power/supply/charger_class.ko",
    "drivers/power/supply/k6985v1_64_alpha/bq2579x.ko",
    "drivers/power/supply/k6985v1_64_alpha/bq28z610.ko",
    "drivers/power/supply/mt6358_battery.ko",
    #"drivers/power/supply/mt6359p_battery.ko",
    "drivers/power/supply/mt6360_charger.ko",
    "drivers/power/supply/mt6360_pmu_chg.ko",
    "drivers/power/supply/mt6370-charger.ko",
    "drivers/power/supply/mt6375-battery.ko",
    "drivers/power/supply/mtk_battery_manager.ko",
    "drivers/power/supply/mt6375-charger.ko",
    "drivers/power/supply/mt6379-chg.ko",
    "drivers/power/supply/mt6379-battery.ko",
    "drivers/power/supply/ufcs/ufcs_class.ko",
    "drivers/power/supply/ufcs/ufcs_mt6379.ko",
    "drivers/power/supply/mtk_ufcs_adapter.ko",
    "drivers/power/supply/mtk_2p_charger.ko",
    "drivers/power/supply/mtk_charger_algorithm_class.ko",
    "drivers/power/supply/mtk_charger_framework.ko",
    "drivers/power/supply/mtk_chg_type_det.ko",
    "drivers/power/supply/mtk_hvbpc.ko",
    "drivers/power/supply/mtk_pd_adapter.ko",
    "drivers/power/supply/mtk_pd_charging.ko",
    "drivers/power/supply/mtk_pep.ko",
    "drivers/power/supply/mtk_pep20.ko",
    "drivers/power/supply/mtk_pep40.ko",
    "drivers/power/supply/mtk_pep45.ko",
    "drivers/power/supply/mtk_pep50.ko",
    "drivers/power/supply/mtk_pep50p.ko",
    "drivers/power/supply/rt9490-charger.ko",
    "drivers/power/supply/rt9758-charger.ko",
    "drivers/power/supply/rt9759.ko",
    "drivers/pwm/pwm-mtk-disp.ko",
    "drivers/regulator/mt6315-regulator.ko",
    "drivers/regulator/mt6316-regulator.ko",
    "drivers/regulator/mt6359p-regulator.ko",
    "drivers/regulator/mt6360-regulator.ko",
    "drivers/regulator/mt6363-regulator.ko",
    "drivers/regulator/mt6368-regulator.ko",
    "drivers/regulator/mt6369-regulator.ko",
    "drivers/regulator/mt6373-regulator.ko",
    "drivers/regulator/mt6379-regulator.ko",
     "drivers/regulator/mt6370-regulator.ko",
    "drivers/regulator/mt6681-regulator.ko",
    "drivers/regulator/mtk-dvfsrc-regulator.ko",
    "drivers/regulator/mtk-extbuck-debug.ko",
    "drivers/regulator/mtk-vmm-isp71-regulator.ko",
    "drivers/regulator/rt4803.ko",
    "drivers/regulator/rt5133-regulator.ko",
    "drivers/regulator/rt6160-regulator.ko",
    "drivers/remoteproc/mtk_ccu.ko",
    "drivers/remoteproc/mtk_ccu_mssv.ko",
    "drivers/reset/reset-ti-syscon.ko",
    "drivers/rpmsg/mtk_rpmsg_mbox.ko",
    "drivers/rtc/rtc-mt6397.ko",
    "drivers/rtc/rtc-mt6685.ko",
    "drivers/ufs/ufs-mediatek-dbg.ko",
    "drivers/ufs/vendor/ufs-mediatek-mod.ko",
    "drivers/ufs/vendor/ufs-mediatek-mod-ise.ko",
    "drivers/soc/mediatek/devapc/device-apc-common.ko",
    "drivers/soc/mediatek/devapc/device-apc-common-legacy.ko",
    "drivers/soc/mediatek/mtk-dvfsrc.ko",
    "drivers/soc/mediatek/mtk-dvfsrc-start.ko",
    "drivers/soc/mediatek/mtk-mbox.ko",
    "drivers/soc/mediatek/mtk-mmdvfs.ko",
    "drivers/soc/mediatek/mtk-mmdvfs-v3.ko",
    "drivers/soc/mediatek/mtk-mmsys.ko",
    "drivers/soc/mediatek/mtk-mutex.ko",
    "drivers/soc/mediatek/mtk-pm-domain-disable-unused.ko",
    "drivers/soc/mediatek/mtk-pmic-wrap.ko",
    "drivers/soc/mediatek/mtk-scpsys.ko",
    "drivers/soc/mediatek/mtk-scpsys-bringup.ko",
    "drivers/soc/mediatek/scpsys-dummy.ko",
    "drivers/soc/mediatek/mtk-socinfo.ko",
    "drivers/soc/mediatek/mtk_tinysys_ipi.ko",
    "drivers/spi/spi-mt65xx.ko",
    "drivers/spmi/spmi-mtk-mpu.ko",
    "drivers/spmi/spmi-mtk-pmif.ko",
    "drivers/tee/gud/600/MobiCoreDriver/mcDrvModule.ko",
    "drivers/tee/gud/600/MobiCoreDriver/mcDrvModule-ffa.ko",
    "drivers/tee/gud/600/TlcTui/t-base-tui.ko",
    "drivers/tee/teei/510/isee.ko",
    "drivers/tee/teei/510/isee-ffa.ko",
    "drivers/tee/teei/515/isee.ko",
    "drivers/tee/teei/515/isee-ffa.ko",
    "drivers/tee/teeperf/teeperf.ko",
    "drivers/thermal/mediatek/backlight_cooling.ko",
    "drivers/thermal/mediatek/board_temp.ko",
    "drivers/thermal/mediatek/charger_cooling.ko",
    "drivers/thermal/mediatek/md_cooling_all.ko",
    "drivers/thermal/mediatek/pmic_temp.ko",
    "drivers/thermal/mediatek/soc_temp_lvts.ko",
    "drivers/thermal/mediatek/thermal_interface.ko",
    "drivers/thermal/mediatek/thermal_trace.ko",
    "drivers/thermal/mediatek/vtskin_temp.ko",
    "drivers/thermal/mediatek/wifi_cooling.ko",
    "drivers/tty/serial/8250/8250_mtk.ko",
    "drivers/usb/mtu3/mtu3.ko",
    "drivers/watchdog/mtk_wdt.ko",
    "sound/soc/codecs/mt6338-accdet.ko",
    "sound/soc/codecs/mt6358-accdet.ko",
    "sound/soc/codecs/mt6359p-accdet.ko",
    "sound/soc/codecs/mt6368-accdet.ko",
    "sound/soc/codecs/mt6681-accdet.ko",
    "sound/soc/codecs/richtek/richtek_spm_cls.ko",
    "sound/soc/codecs/richtek/snd-soc-rt5512.ko",
    "sound/soc/codecs/snd-soc-mt6338.ko",
    "sound/soc/codecs/snd-soc-mt6368.ko",
    "sound/soc/codecs/snd-soc-mt6681.ko",
    "sound/soc/codecs/tfa98xx/snd-soc-tfa98xx.ko",
    "sound/soc/codecs/snd-soc-mt6359.ko",
    "sound/soc/mediatek/audio_dsp/mtk-soc-offload-common.ko",
    "sound/soc/mediatek/audio_dsp/snd-soc-audiodsp-common.ko",
    "sound/soc/mediatek/common/mtk-afe-external.ko",
    "sound/soc/mediatek/common/mtk-btcvsd.ko",
    "sound/soc/mediatek/common/mtk-sp-spk-amp.ko",
    "sound/soc/mediatek/common/snd-soc-mtk-common.ko",
    "sound/soc/mediatek/ultrasound/ultra_common/mtk-scp-ultra.ko",
    "sound/soc/mediatek/ultrasound/ultra_scp/snd-soc-mtk-scp-ultra.ko",
    "sound/soc/mediatek/vow/mtk-scp-vow.ko",
]

mgk_64_platform_device_modules = {
    # keep sorted
    "drivers/clk/mediatek/clk-chk-mt6886.ko": "mt6886",
    "drivers/clk/mediatek/clk-chk-mt6897.ko": "mt6897",
    "drivers/clk/mediatek/clk-mt6893-apu0.ko": "mt6893",
    "drivers/clk/mediatek/clk-mt6893-apu1.ko": "mt6893",
    "drivers/clk/mediatek/clk-mt6893-apu2.ko": "mt6893",
    "drivers/clk/mediatek/clk-mt6893-apuc.ko": "mt6893",
    "drivers/clk/mediatek/clk-mt6893-apum0.ko": "mt6893",
    "drivers/clk/mediatek/clk-mt6893-apum1.ko": "mt6893",
    "drivers/clk/mediatek/clk-mt6893-apuv.ko": "mt6893",
    "drivers/clk/mediatek/clk-mt6893-audsys.ko": "mt6893",
    "drivers/clk/mediatek/clk-mt6893-cam_m.ko": "mt6893",
    "drivers/clk/mediatek/clk-mt6893-cam_ra.ko": "mt6893",
    "drivers/clk/mediatek/clk-mt6893-cam_rb.ko": "mt6893",
    "drivers/clk/mediatek/clk-mt6893-cam_rc.ko": "mt6893",
    "drivers/clk/mediatek/clk-mt6893-imgsys1.ko": "mt6893",
    "drivers/clk/mediatek/clk-mt6893-imgsys2.ko": "mt6893",
    "drivers/clk/mediatek/clk-mt6893-impc.ko": "mt6893",
    "drivers/clk/mediatek/clk-mt6893-impe.ko": "mt6893",
    "drivers/clk/mediatek/clk-mt6893-impn.ko": "mt6893",
    "drivers/clk/mediatek/clk-mt6893-imps.ko": "mt6893",
    "drivers/clk/mediatek/clk-mt6893-ipe.ko": "mt6893",
    "drivers/clk/mediatek/clk-mt6893-mdp.ko": "mt6893",
    "drivers/clk/mediatek/clk-mt6893-mfgcfg.ko": "mt6893",
    "drivers/clk/mediatek/clk-mt6893-mm.ko": "mt6893",
    "drivers/clk/mediatek/clk-mt6893-scp_adsp.ko": "mt6893",
    "drivers/clk/mediatek/clk-mt6893-vde1.ko": "mt6893",
    "drivers/clk/mediatek/clk-mt6893-vde2.ko": "mt6893",
    "drivers/clk/mediatek/clk-mt6893-ven1.ko": "mt6893",
    "drivers/clk/mediatek/clk-mt6893-ven2.ko": "mt6893",
    "drivers/clk/mediatek/clk-mt6893.ko": "mt6893",
    "drivers/misc/mediatek/cmdq/mailbox/cmdq-platform-mt6893.ko": "mt6893",
    "drivers/soc/mediatek/mtk-scpsys-mt6893.ko": "mt6893",
    "drivers/clk/mediatek/clk-chk-mt6893.ko": "mt6893",
    "drivers/clk/mediatek/clk-chk-mt6983.ko": "mt6983",
    "drivers/clk/mediatek/clk-chk-mt6985.ko": "mt6985",
    "drivers/clk/mediatek/clk-chk-mt6989.ko": "mt6989",
    "drivers/clk/mediatek/clk-chk-mt6991.ko": "mt6991",
    "drivers/clk/mediatek/clk-dbg-mt6893.ko": "mt6893",
    "drivers/clk/mediatek/clk-dbg-mt6886.ko": "mt6886",
    "drivers/clk/mediatek/clk-dbg-mt6897.ko": "mt6897",
    "drivers/clk/mediatek/clk-dbg-mt6983.ko": "mt6983",
    "drivers/clk/mediatek/clk-dbg-mt6985.ko": "mt6985",
    "drivers/clk/mediatek/clk-dbg-mt6989.ko": "mt6989",
    "drivers/clk/mediatek/clk-dbg-mt6991.ko": "mt6991",
    "drivers/clk/mediatek/clk-fmeter-mt6886.ko": "mt6886",
    "drivers/clk/mediatek/clk-fmeter-mt6897.ko": "mt6897",
    "drivers/clk/mediatek/clk-fmeter-mt6983.ko": "mt6983",
    "drivers/clk/mediatek/clk-fmeter-mt6893.ko": "mt6893",
    "drivers/clk/mediatek/clk-fmeter-mt6985.ko": "mt6985",
    "drivers/clk/mediatek/clk-fmeter-mt6989.ko": "mt6989",
    "drivers/clk/mediatek/clk-fmeter-mt6991.ko": "mt6991",
    "drivers/clk/mediatek/clk-mt6886.ko": "mt6886",
    "drivers/clk/mediatek/clk-mt6886-adsp.ko": "mt6886",
    "drivers/clk/mediatek/clk-mt6886-bus.ko": "mt6886",
    "drivers/clk/mediatek/clk-mt6886-cam.ko": "mt6886",
    "drivers/clk/mediatek/clk-mt6886-ccu.ko": "mt6886",
    "drivers/clk/mediatek/clk-mt6886-img.ko": "mt6886",
    "drivers/clk/mediatek/clk-mt6886-mdpsys.ko": "mt6886",
    "drivers/clk/mediatek/clk-mt6886-mmsys.ko": "mt6886",
    "drivers/clk/mediatek/clk-mt6886-peri.ko": "mt6886",
    "drivers/clk/mediatek/clk-mt6886-scp.ko": "mt6886",
    "drivers/clk/mediatek/clk-mt6886-vcodec.ko": "mt6886",
    "drivers/clk/mediatek/clk-mt6897.ko": "mt6897",
    "drivers/clk/mediatek/clk-mt6897-adsp.ko": "mt6897",
    "drivers/clk/mediatek/clk-mt6897-bus.ko": "mt6897",
    "drivers/clk/mediatek/clk-mt6897-cam.ko": "mt6897",
    "drivers/clk/mediatek/clk-mt6897-ccu.ko": "mt6897",
    "drivers/clk/mediatek/clk-mt6897-img.ko": "mt6897",
    "drivers/clk/mediatek/clk-mt6897-mdpsys.ko": "mt6897",
    "drivers/clk/mediatek/clk-mt6897-mmsys.ko": "mt6897",
    "drivers/clk/mediatek/clk-mt6897-peri.ko": "mt6897",
    "drivers/clk/mediatek/clk-mt6897-vcodec.ko": "mt6897",
    "drivers/clk/mediatek/clk-mt6897-vlp.ko": "mt6897",
    "drivers/clk/mediatek/clk-mt6983.ko": "mt6983",
    "drivers/clk/mediatek/clk-mt6983-adsp_grp.ko": "mt6983",
    "drivers/clk/mediatek/clk-mt6983-cam.ko": "mt6983",
    "drivers/clk/mediatek/clk-mt6983-ccu_main.ko": "mt6983",
    "drivers/clk/mediatek/clk-mt6983-img.ko": "mt6983",
    "drivers/clk/mediatek/clk-mt6983-imp_iic_wrap.ko": "mt6983",
    "drivers/clk/mediatek/clk-mt6983-mdp_grp.ko": "mt6983",
    "drivers/clk/mediatek/clk-mt6983-mfg_top_config.ko": "mt6983",
    "drivers/clk/mediatek/clk-mt6983-mm.ko": "mt6983",
    "drivers/clk/mediatek/clk-mt6983-vcodec.ko": "mt6983",
    "drivers/clk/mediatek/clk-mt6985.ko": "mt6985",
    "drivers/clk/mediatek/clk-mt6985-adsp.ko": "mt6985",
    "drivers/clk/mediatek/clk-mt6985-bus.ko": "mt6985",
    "drivers/clk/mediatek/clk-mt6985-cam.ko": "mt6985",
    "drivers/clk/mediatek/clk-mt6985-ccu.ko": "mt6985",
    "drivers/clk/mediatek/clk-mt6985-img.ko": "mt6985",
    "drivers/clk/mediatek/clk-mt6985-mdpsys.ko": "mt6985",
    "drivers/clk/mediatek/clk-mt6985-mmsys.ko": "mt6985",
    "drivers/clk/mediatek/clk-mt6985-peri.ko": "mt6985",
    "drivers/clk/mediatek/clk-mt6985-vcodec.ko": "mt6985",
    "drivers/clk/mediatek/clk-mt6985-vlp.ko": "mt6985",
    "drivers/clk/mediatek/clk-mt6989.ko": "mt6989",
    "drivers/clk/mediatek/clk-mt6989-adsp.ko": "mt6989",
    "drivers/clk/mediatek/clk-mt6989-bus.ko": "mt6989",
    "drivers/clk/mediatek/clk-mt6989-cam.ko": "mt6989",
    "drivers/clk/mediatek/clk-mt6989-img.ko": "mt6989",
    "drivers/clk/mediatek/clk-mt6989-mdpsys.ko": "mt6989",
    "drivers/clk/mediatek/clk-mt6989-mmsys.ko": "mt6989",
    "drivers/clk/mediatek/clk-mt6989-peri.ko": "mt6989",
    "drivers/clk/mediatek/clk-mt6989-vcodec.ko": "mt6989",
    "drivers/clk/mediatek/clk-mt6989-vlp.ko": "mt6989",
    "drivers/clk/mediatek/clk-mt6991.ko": "mt6991",
    "drivers/clk/mediatek/clk-mt6991-adsp.ko": "mt6991",
    "drivers/clk/mediatek/clk-mt6991-cam.ko": "mt6991",
    "drivers/clk/mediatek/clk-mt6991-img.ko": "mt6991",
    "drivers/clk/mediatek/clk-mt6991-mmsys.ko": "mt6991",
    "drivers/clk/mediatek/clk-mt6991-peri.ko": "mt6991",
    "drivers/clk/mediatek/clk-mt6991-mdpsys.ko": "mt6991",
    "drivers/clk/mediatek/clk-mt6991-bus.ko": "mt6991",
    "drivers/clk/mediatek/clk-mt6991-vcodec.ko": "mt6991",
    #"drivers/clk/mediatek/clk-mt8188.ko": "mt8188",
    #"drivers/clk/mediatek/clk-mt8188-audio_src.ko": "mt8188",
    #"drivers/clk/mediatek/clk-mt8188-cam.ko": "mt8188",
    #"drivers/clk/mediatek/clk-mt8188-ccu.ko": "mt8188",
    #"drivers/clk/mediatek/clk-mt8188-img.ko": "mt8188",
    #"drivers/clk/mediatek/clk-mt8188-imp_iic_wrap.ko": "mt8188",
    #"drivers/clk/mediatek/clk-mt8188-ipe.ko": "mt8188",
    #"drivers/clk/mediatek/clk-mt8188-mfgcfg.ko": "mt8188",
    #"drivers/clk/mediatek/clk-mt8188-vdec.ko": "mt8188",
    #"drivers/clk/mediatek/clk-mt8188-vdo0.ko": "mt8188",
    #"drivers/clk/mediatek/clk-mt8188-vdo1.ko": "mt8188",
    #"drivers/clk/mediatek/clk-mt8188-venc.ko": "mt8188",
    #"drivers/clk/mediatek/clk-mt8188-vpp0.ko": "mt8188",
    #"drivers/clk/mediatek/clk-mt8188-vpp1.ko": "mt8188",
    #"drivers/clk/mediatek/clk-mt8188-wpe.ko": "mt8188",
    "drivers/clk/mediatek/pd-chk-mt6886.ko": "mt6886",
    "drivers/clk/mediatek/pd-chk-mt6893.ko": "mt6893",
    "drivers/clk/mediatek/pd-chk-mt6897.ko": "mt6897",
    "drivers/clk/mediatek/pd-chk-mt6983.ko": "mt6983",
    "drivers/clk/mediatek/pd-chk-mt6985.ko": "mt6985",
    "drivers/clk/mediatek/pd-chk-mt6989.ko": "mt6989",
    "drivers/clk/mediatek/pd-chk-mt6991.ko": "mt6991",
    "drivers/gpu/drm/mediatek/mml/mtk-mml-mt6878.ko": "mt6878",
    "drivers/gpu/drm/mediatek/mml/mtk-mml-mt6886.ko": "mt6886",
    "drivers/gpu/drm/mediatek/mml/mtk-mml-mt6897.ko": "mt6897",
    "drivers/gpu/drm/mediatek/mml/mtk-mml-mt6983.ko": "mt6983",
    "drivers/gpu/drm/mediatek/mml/mtk-mml-mt6985.ko": "mt6985",
    "drivers/gpu/drm/mediatek/mml/mtk-mml-mt6989.ko": "mt6989",
    "drivers/gpu/drm/mediatek/mml/mtk-mml-mt6991.ko": "mt6991",
    "drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6886.ko": "mt6886",
    "drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6897.ko": "mt6897",
    "drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6985.ko": "mt6985",
    "drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6989.ko": "mt6989",
    "drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6989_fpga.ko": "mt6989",
    "drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6991.ko": "mt6991",
    "drivers/gpu/mediatek/gpu_iommu/mtk_gpu_iommu_mt6989.ko": "mt6989",
    "drivers/gpu/mediatek/gpu_iommu/mtk_gpu_iommu_mt6991.ko": "mt6991",
    "drivers/interconnect/mediatek/mmqos-mt6886.ko": "mt6886",
    "drivers/interconnect/mediatek/mmqos-mt6893.ko": "mt6893",
    "drivers/interconnect/mediatek/mmqos-mt6897.ko": "mt6897",
    "drivers/interconnect/mediatek/mmqos-mt6983.ko": "mt6983",
    "drivers/interconnect/mediatek/mmqos-mt6985.ko": "mt6985",
    "drivers/interconnect/mediatek/mmqos-mt6989.ko": "mt6989",
    "drivers/interconnect/mediatek/mmqos-mt6991.ko": "mt6991",
    "drivers/misc/mediatek/cameraisp/pda/pda_drv_mt6855.ko": "mt6855",
    "drivers/misc/mediatek/cameraisp/pda/pda_drv_mt6879.ko": "mt6879",
    "drivers/misc/mediatek/cameraisp/pda/pda_drv_mt6886.ko": "mt6886",
    "drivers/misc/mediatek/cameraisp/pda/pda_drv_mt6895.ko": "mt6895",
    "drivers/misc/mediatek/cameraisp/pda/pda_drv_mt6897.ko": "mt6897",
    "drivers/misc/mediatek/cameraisp/pda/pda_drv_mt6985.ko": "mt6985",
    "drivers/misc/mediatek/cameraisp/pda/pda_drv_mt6989.ko": "mt6989",
    "drivers/misc/mediatek/cmdq/mailbox/cmdq-platform-mt6878.ko": "mt6878",
    "drivers/misc/mediatek/cmdq/mailbox/cmdq-platform-mt6886.ko": "mt6886",
    "drivers/misc/mediatek/cmdq/mailbox/cmdq-platform-mt6897.ko": "mt6897",
    "drivers/misc/mediatek/cmdq/mailbox/cmdq-platform-mt6983.ko": "mt6983",
    "drivers/misc/mediatek/cmdq/mailbox/cmdq-platform-mt6985.ko": "mt6985",
    "drivers/misc/mediatek/cmdq/mailbox/cmdq-platform-mt6989.ko": "mt6989",
    "drivers/misc/mediatek/cmdq/mailbox/cmdq-platform-mt6768.ko": "mt6768",
    "drivers/misc/mediatek/cmdq/mailbox/cmdq-platform-mt6991.ko": "mt6991",
    "drivers/soc/mediatek/mtk-scpsys-mt6768.ko": "mt6768",
    "drivers/soc/mediatek/mtk-scpsys-mt6761.ko": "mt6761",
    "drivers/soc/mediatek/mtk-scpsys-mt6877.ko": "mt6877",
    "drivers/misc/mediatek/cm_mgr/mtk_cm_mgr_mt6886.ko": "mt6886",
    "drivers/misc/mediatek/cm_mgr/mtk_cm_mgr_mt6897.ko": "mt6897",
    "drivers/misc/mediatek/cm_mgr/mtk_cm_mgr_mt6983.ko": "mt6983",
    "drivers/misc/mediatek/cm_mgr/mtk_cm_mgr_mt6985.ko": "mt6985",
    "drivers/misc/mediatek/cm_mgr/mtk_cm_mgr_mt6989.ko": "mt6989",
    "drivers/misc/mediatek/dcm/mt6897_dcm.ko": "mt6897",
    "drivers/misc/mediatek/dcm/mt6985_dcm.ko": "mt6985",
    "drivers/misc/mediatek/dcm/mt6989_dcm.ko": "mt6989",
    "drivers/misc/mediatek/dcm/mt6991_dcm.ko": "mt6991",
    "drivers/misc/mediatek/lpm/modules/debug/mt6886/mtk-lpm-dbg-mt6886.ko": "mt6886",
    "drivers/misc/mediatek/lpm/modules/debug/mt6897/mtk-lpm-dbg-mt6897.ko": "mt6897",
    "drivers/misc/mediatek/lpm/modules/debug/mt6983/mtk-lpm-dbg-mt6983.ko": "mt6983",
    "drivers/misc/mediatek/lpm/modules/debug/mt6985/mtk-lpm-dbg-mt6985.ko": "mt6985",
    "drivers/misc/mediatek/lpm/modules/debug/mt6989/mtk-lpm-dbg-mt6989.ko": "mt6989",
    "drivers/misc/mediatek/lpm/modules/debug/mt6991/mtk-lpm-dbg-mt6991.ko": "mt6991",
    "drivers/misc/mediatek/mdp/mdp_drv_mt6886.ko": "mt6886",
    "drivers/misc/mediatek/mdp/mdp_drv_mt6893.ko": "mt6893",
    "drivers/misc/mediatek/mdp/mdp_drv_mt6897.ko": "mt6897",
    "drivers/misc/mediatek/mdp/mdp_drv_mt6983.ko": "mt6983",
    "drivers/misc/mediatek/mdp/mdp_drv_mt6985.ko": "mt6985",
    "drivers/misc/mediatek/mdp/mdp_drv_mt6989.ko": "mt6989",
    "drivers/misc/mediatek/mdp/mdp_drv_mt6768.ko": "mt6768",
    "drivers/misc/mediatek/mdp/mdp_drv_mt6991.ko": "mt6991",
    "drivers/misc/mediatek/slbc/slbc_mt6886.ko": "mt6886",
    "drivers/misc/mediatek/slbc/slbc_mt6893.ko": "mt6893",
    "drivers/misc/mediatek/slbc/slbc_mt6895.ko": "mt6895",
    "drivers/misc/mediatek/slbc/slbc_mt6897.ko": "mt6897",
    "drivers/misc/mediatek/slbc/slbc_mt6983.ko": "mt6983",
    "drivers/misc/mediatek/slbc/slbc_mt6985.ko": "mt6985",
    "drivers/misc/mediatek/slbc/slbc_mt6989.ko": "mt6989",
    "drivers/misc/mediatek/slbc/slbc_mt6991.ko": "mt6991",
    "drivers/misc/mediatek/smap/smap-mt6991.ko": "mt6991",
    "drivers/pinctrl/mediatek/pinctrl-mt6886.ko": "mt6886",
    "drivers/pinctrl/mediatek/pinctrl-mt6897.ko": "mt6897",
    "drivers/pinctrl/mediatek/pinctrl-mt6983.ko": "mt6983",
    "drivers/pinctrl/mediatek/pinctrl-mt6985.ko": "mt6985",
    "drivers/pinctrl/mediatek/pinctrl-mt6989.ko": "mt6989",
    "drivers/pinctrl/mediatek/pinctrl-mt6991.ko": "mt6991",
    "drivers/soc/mediatek/devapc/device-apc-mt6768.ko": "mt6768",
    "drivers/soc/mediatek/devapc/device-apc-mt6879.ko": "mt6879",
    "drivers/soc/mediatek/devapc/device-apc-mt6886.ko": "mt6886",
    "drivers/soc/mediatek/devapc/device-apc-mt6893.ko": "mt6893",
    "drivers/soc/mediatek/devapc/device-apc-mt6895.ko": "mt6895",
    "drivers/soc/mediatek/devapc/device-apc-mt6897.ko": "mt6897",
    "drivers/soc/mediatek/devapc/device-apc-mt6983.ko": "mt6983",
    "drivers/soc/mediatek/devapc/device-apc-mt6985.ko": "mt6985",
    "drivers/soc/mediatek/devapc/device-apc-mt6989.ko": "mt6989",
    "drivers/soc/mediatek/devapc/device-apc-mt6991.ko": "mt6991",
    "drivers/soc/mediatek/mtk-pm-domains.ko": "mt8188",
    "drivers/soc/mediatek/mtk-scpsys-mt6886.ko": "mt6886",
    "drivers/soc/mediatek/mtk-scpsys-mt6897.ko": "mt6897",
    "drivers/soc/mediatek/mtk-scpsys-mt6983.ko": "mt6983",
    "drivers/soc/mediatek/mtk-scpsys-mt6985.ko": "mt6985",
    "drivers/soc/mediatek/mtk-scpsys-mt6989.ko": "mt6989",
    "drivers/soc/mediatek/mtk-scpsys-mt6991-spm.ko": "mt6991",
    "drivers/soc/mediatek/mtk-scpsys-mt6991-mmpc.ko": "mt6991",
    "drivers/misc/mediatek/vmm_spm/mtk-vmm-spm-mt6989.ko": "mt6989",
    "sound/soc/mediatek/mt6886/mt6886-mt6368.ko": "mt6886",
    "sound/soc/mediatek/mt6886/snd-soc-mt6886-afe.ko": "mt6886",
    "sound/soc/mediatek/mt6897/mt6897-mt6368.ko": "mt6897",
    "sound/soc/mediatek/mt6897/snd-soc-mt6897-afe.ko": "mt6897",
    "sound/soc/mediatek/mt6983/mt6983-mt6338.ko": "mt6983",
    "sound/soc/mediatek/mt6983/snd-soc-mt6983-afe.ko": "mt6983",
    "sound/soc/mediatek/mt6985/mt6985-mt6338.ko": "mt6985",
    "sound/soc/mediatek/mt6985/snd-soc-mt6985-afe.ko": "mt6985",
    "sound/soc/mediatek/mt6989/mt6989-mt6681.ko": "mt6989",
    "sound/soc/mediatek/mt6989/snd-soc-mt6989-afe.ko": "mt6989",
    "sound/soc/mediatek/mt6991/mt6991-mt6681.ko": "mt6991",
    "sound/soc/mediatek/mt6991/snd-soc-mt6991-afe.ko": "mt6991",
    "sound/soc/mediatek/mt6885/snd-soc-mt6885-afe.ko": "mt6893",
    "sound/soc/mediatek/mt6885/mt6885-mt6359p.ko": "mt6893",
}


mgk_64_device_eng_modules = [
    "arch/arm64/geniezone/gzvm.ko",
    "drivers/misc/mediatek/cpufreq_v1/cpuhvfs.ko",
    "drivers/misc/mediatek/locking/locking_aee.ko",
    "drivers/misc/mediatek/mtprof/irq_monitor.ko",
    "drivers/misc/mediatek/selinux_warning/mtk_selinux_aee_warning.ko",
]

mgk_64_platform_device_eng_modules = {
}


mgk_64_device_userdebug_modules = [
    "arch/arm64/geniezone/gzvm.ko",
    "drivers/misc/mediatek/cpufreq_v1/cpuhvfs.ko",
    "drivers/misc/mediatek/mtprof/irq_monitor.ko",
    "drivers/misc/mediatek/selinux_warning/mtk_selinux_aee_warning.ko",
]

mgk_64_platform_device_userdebug_modules = {
}


mgk_64_device_user_modules = [
]

mgk_64_platform_device_user_modules = {
}


def get_overlay_modules_list():
    if "auto.config" in DEFCONFIG_OVERLAYS:
        mgk_64_platform_device_modules.update({"drivers/clk/mediatek/clk-mt6991-ivi.ko":"mt6991"})
        mgk_64_platform_device_modules.update({"drivers/soc/mediatek/mtk-scpsys-mt6991-ivi.ko":"mt6991"})

    if "fpga.config" in DEFCONFIG_OVERLAYS:
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/fpsgo_cus:fpsgo_cus")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/fpsgo_int:fpsgo_int")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/met_drv_secure_v3:met_drv_secure_v3")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/met_drv_v3/met_api:met_api_v3_cus")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/met_drv_v3/met_api:met_api_v3_int")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/met_drv_v3:met_drv_v3")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/msync2_frd_cus/build:msync2_frd_cus")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/msync2_frd_int:msync2_frd_int")
        mgk_64_kleaf_eng_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase:ktf_testcase")
        mgk_64_kleaf_userdebug_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase:ktf_testcase")
        mgk_64_device_modules.remove("drivers/misc/mediatek/performance/fpsgo_v3/mtk_fpsgo.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/performance/frs/frs.ko")

        mgk_64_kleaf_modules.append("//vendor/mediatek/kernel_modules/met_drv_secure_v3:met_drv_secure_v3_default")
        mgk_64_kleaf_modules.append("//vendor/mediatek/kernel_modules/met_drv_v3:met_drv_v3_default")
        mgk_64_kleaf_eng_modules.append("//vendor/mediatek/tests/kernel/ktf_testcase:ktf_testcase_fpga")
        mgk_64_kleaf_userdebug_modules.append("//vendor/mediatek/tests/kernel/ktf_testcase:ktf_testcase_fpga")

    if "wifionly.config" in DEFCONFIG_OVERLAYS:
        mgk_64_device_modules.remove("drivers/misc/mediatek/ccci_util/ccci_util_lib.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/ccmni/ccmni.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/eccci/ccci_auxadc.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/eccci/ccci_md_all.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/eccci/fsm/ccci_fsm_scp.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/eccci/hif/ccci_ccif.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/eccci/hif/ccci_cldma.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/eccci/hif/ccci_dpmaif.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mddp/mddp.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/power_throttling/mtk_md_power_throttling.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/usb/c2k_usb/c2k_usb.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/usb/c2k_usb/c2k_usb_f_via_atc.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/usb/c2k_usb/c2k_usb_f_via_ets.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/usb/c2k_usb/c2k_usb_f_via_modem.ko")
        mgk_64_device_modules.remove("drivers/thermal/mediatek/md_cooling_all.ko")

        mgk_64_device_modules.append("drivers/misc/mediatek/pmic_tia/pmic_tia.ko")

    if "thinmodem.config" in DEFCONFIG_OVERLAYS:
        mgk_64_kleaf_modules.append("//vendor/mediatek/kernel_modules/wwan/tmi3:tmi3")
        mgk_64_kleaf_modules.append("//vendor/mediatek/kernel_modules/gpio_sap_ctrl:gpio_sap_ctrl")
        mgk_64_kleaf_modules.append("//vendor/mediatek/kernel_modules/wwan/pwrctl/common:wwan_gpio_pwrctl")
        mgk_64_device_modules.remove("drivers/misc/mediatek/ccci_util/ccci_util_lib.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/ccmni/ccmni.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/eccci/ccci_auxadc.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/eccci/ccci_md_all.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/eccci/fsm/ccci_fsm_scp.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/eccci/hif/ccci_ccif.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/eccci/hif/ccci_cldma.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/eccci/hif/ccci_dpmaif.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mddp/mddp.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/power_throttling/mtk_md_power_throttling.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/usb/c2k_usb/c2k_usb.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/usb/c2k_usb/c2k_usb_f_via_atc.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/usb/c2k_usb/c2k_usb_f_via_ets.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/usb/c2k_usb/c2k_usb_f_via_modem.ko")
        mgk_64_device_modules.remove("drivers/thermal/mediatek/md_cooling_all.ko")

    if "mt6877_overlay.config" in DEFCONFIG_OVERLAYS:
        mgk_64_platform_device_modules.update({"drivers/pinctrl/mediatek/pinctrl-mt6877.ko":"mt6877"})
        mgk_64_device_modules.remove("drivers/memory/mediatek/emi.ko")
        mgk_64_device_modules.remove("drivers/memory/mediatek/emi-fake-eng.ko")
        mgk_64_device_modules.remove("drivers/memory/mediatek/emi-mpu.ko")
        mgk_64_device_modules.remove("drivers/memory/mediatek/emi-mpu-test.ko")
        mgk_64_device_modules.remove("drivers/memory/mediatek/emi-mpu-test-v2.ko")
        mgk_64_device_modules.remove("drivers/memory/mediatek/emi-slb.ko")
        mgk_64_device_modules.remove("drivers/memory/mediatek/smpu.ko")
        mgk_64_device_modules.remove("drivers/memory/mediatek/slc-parity.ko")
        mgk_64_device_modules.remove("drivers/memory/mediatek/smpu-hook-v1.ko")
        mgk_64_device_modules.remove("drivers/char/rpmb/rpmb.ko")
        mgk_64_device_modules.remove("drivers/char/rpmb/rpmb-mtk.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi_legacy_v2/emi.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi_legacy_v2/emi-mpu.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi_legacy_v2/emi-mpu-test.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi-dummy.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi_legacy_v2/emi-mpu-hook-v1.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi_legacy_v2/emi-slb.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi_legacy_v2/emi-mpu-test-v2.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi_legacy_v2/emi-fake-eng.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi_legacy_v2/emi-mpu-v2.ko")
        mgk_64_platform_device_modules.update({"drivers/clk/mediatek/clkchk-mt6877.ko":"mt6877"})
        #mgk_64_platform_device_modules.update({"drivers/clk/mediatek/clkdbg-mt6877.ko":"mt6877"})
        #mgk_64_platform_device_modules.update({"drivers/clk/mediatek/clk-fmeter-mt6877.ko":"mt6877"})
        mgk_64_platform_device_modules.update({"drivers/clk/mediatek/clk-mt6877.ko":"mt6877"})
        mgk_64_platform_device_modules.update({"drivers/clk/mediatek/clk-mt6877-apu.ko":"mt6877"})
        mgk_64_platform_device_modules.update({"drivers/clk/mediatek/clk-mt6877-audsys.ko":"mt6877"})
        mgk_64_platform_device_modules.update({"drivers/clk/mediatek/clk-mt6877-cam.ko":"mt6877"})
        mgk_64_platform_device_modules.update({"drivers/clk/mediatek/clk-mt6877-img.ko":"mt6877"})
        mgk_64_platform_device_modules.update({"drivers/clk/mediatek/clk-mt6877-i2c.ko":"mt6877"})
        mgk_64_platform_device_modules.update({"drivers/clk/mediatek/clk-mt6877-ipe.ko":"mt6877"})
        mgk_64_platform_device_modules.update({"drivers/clk/mediatek/clk-mt6877-mdp.ko":"mt6877"})
        mgk_64_platform_device_modules.update({"drivers/clk/mediatek/clk-mt6877-mfgcfg.ko":"mt6877"})
        mgk_64_platform_device_modules.update({"drivers/clk/mediatek/clk-mt6877-mm.ko":"mt6877"})
        mgk_64_platform_device_modules.update({"drivers/clk/mediatek/clk-mt6877-msdc.ko":"mt6877"})
        mgk_64_platform_device_modules.update({"drivers/clk/mediatek/clk-mt6877-scp_par.ko":"mt6877"})
        mgk_64_platform_device_modules.update({"drivers/clk/mediatek/clk-mt6877-vde.ko":"mt6877"})
        mgk_64_platform_device_modules.update({"drivers/clk/mediatek/clk-mt6877-ven.ko":"mt6877"})
        mgk_64_platform_device_modules.update({"drivers/clk/mediatek/clk-mt6877-pg.ko":"mt6877"})
        #mgk_64_platform_device_modules.update({"drivers/clk/mediatek/pd-chk-mt6877.ko":"mt6877"})

    if "mt6768_overlay.config" in DEFCONFIG_OVERLAYS:
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/gpu:gpu")
        mgk_64_kleaf_modules.append("//vendor/mediatek/kernel_modules/gpu:gpu_mt6768")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/mtkcam/mtk-aie:mtk-aie")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/mtkcam/scpsys/mtk-aov:mtk_aov")
        mgk_64_common_eng_modules.remove("drivers/firmware/arm_ffa/ffa-module.ko")
        mgk_64_common_userdebug_modules.remove("drivers/firmware/arm_ffa/ffa-module.ko")
        mgk_64_common_user_modules.remove("drivers/firmware/arm_ffa/ffa-module.ko")
        mgk_64_common_eng_modules.append("drivers/pwm/pwm-mtk-disp.ko")
        mgk_64_common_userdebug_modules.append("drivers/pwm/pwm-mtk-disp.ko")
        mgk_64_common_user_modules.append("drivers/pwm/pwm-mtk-disp.ko")

        mgk_64_device_modules.append("drivers/misc/mediatek/qos/mtk_qos_legacy.ko")

        mgk_64_device_modules.remove("drivers/gpu/drm/mediatek/mediatek_v2/mtk_aod_scp.ko")
        mgk_64_device_modules.remove("drivers/gpu/drm/panel/mediatek-cust-panel-sample.ko")
        mgk_64_device_modules.remove("drivers/gpu/drm/panel/mediatek-drm-gateic.ko")
        mgk_64_device_modules.remove("drivers/gpu/drm/panel/mediatek-drm-panel-drv.ko")

        mgk_64_device_modules.remove("drivers/iommu/arm/arm-smmu-v3/arm_smmu_v3.ko")
        mgk_64_device_modules.remove("drivers/iommu/arm/arm-smmu-v3/mtk-smmuv3-lmu.ko")
        mgk_64_device_modules.remove("drivers/iommu/arm/arm-smmu-v3/mtk-smmuv3-pmu.ko")
        mgk_64_device_modules.remove("drivers/iommu/arm/arm-smmu-v3/mtk-smmuv3-mpam-mon.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/iommu/mtk_smmu_qos.ko")

        #mgk_64_device_modules.remove("drivers/media/platform/mtk-aie/mtk_aie.ko")
        #mgk_64_device_modules.remove("drivers/media/platform/mtk-isp/mtk-aov/mtk_aov.ko")
        mgk_64_device_modules.remove("drivers/media/platform/mtk-vcodec/mtk-vcodec-dec-v2.ko")
        mgk_64_device_modules.remove("drivers/media/platform/mtk-vcodec/mtk-vcodec-enc-v2.ko")

        mgk_64_device_modules.remove("drivers/memory/mediatek/emi.ko")
        mgk_64_device_modules.remove("drivers/memory/mediatek/emi-fake-eng.ko")
        mgk_64_device_modules.remove("drivers/memory/mediatek/emi-mpu.ko")
        mgk_64_device_modules.remove("drivers/memory/mediatek/emi-mpu-test.ko")
        mgk_64_device_modules.remove("drivers/memory/mediatek/emi-mpu-test-v2.ko")
        mgk_64_device_modules.remove("drivers/memory/mediatek/emi-slb.ko")
        mgk_64_device_modules.remove("drivers/memory/mediatek/smpu.ko")
        mgk_64_device_modules.remove("drivers/memory/mediatek/smpu-hook-v1.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi_legacy_v1/emicen.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi_legacy_v1/emiisu.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi_legacy_v1/emimpu.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi_legacy_v1/emictrl.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi-dummy.ko")

        mgk_64_device_modules.remove("drivers/misc/mediatek/adsp/adsp.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/adsp/v1/adsp-v1.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/adsp/v2/adsp-v2.ko")

        mgk_64_device_modules.remove("drivers/misc/mediatek/apusys/apusys.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/apusys/apu_aov.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/apusys/power/apu_top.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/apusys/sapu/sapu.ko")

        mgk_64_device_modules.remove("drivers/misc/mediatek/audio_ipi/audio_ipi.ko")

        mgk_64_device_modules.remove("drivers/misc/mediatek/cameraisp/src/isp_6s/camera_isp.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/cameraisp/src/isp_6s/cam_qos.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cameraisp/src/isp_4/camera_isp_4_t.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cameraisp/fdvt/camera_fdvt_isp40.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cameraisp/dpe/camera_dpe_isp40.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cameraisp/src/isp_4/cam_qos_4.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cam_cal/src/isp4/camera_eeprom_isp4.ko")

        mgk_64_device_modules.append("drivers/misc/mediatek/cmdq/bridge/cmdq-bdg-mailbox.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cmdq/bridge/cmdq-bdg-test.ko")

        mgk_64_device_modules.remove("drivers/misc/mediatek/cm_mgr/mtk_cm_ipi.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/cm_mgr/mtk_cm_mgr.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cm_mgr_legacy_v1/mtk_cm_mgr.ko")

        mgk_64_device_modules.append("drivers/misc/mediatek/conn_md/conn_md_drv.ko")

        mgk_64_device_modules.append("drivers/misc/mediatek/cpufreq_v2/src/CPU_DVFS.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cpuhotplug/mtk_cpuhp.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cpuidle/mtk_cpuidle.ko")

        mgk_64_device_modules.append("drivers/misc/mediatek/dcm/mt6768_dcm.ko")

        mgk_64_device_modules.append("drivers/misc/mediatek/dvfsrc/dvfsrc-opp-mt6768.ko")

        mgk_64_device_modules.append("drivers/misc/mediatek/eem_v2/mediatek_eem.ko")

        mgk_64_device_modules.remove("drivers/misc/mediatek/imgsensor/src/isp6s/imgsensor_isp6s.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/imgsensor/src/isp4/imgsensor_isp4.ko")

        mgk_64_device_modules.remove("drivers/misc/mediatek/iommu/smmu_secure.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/iommu/iommu_gz.ko")

        mgk_64_device_modules.append("drivers/misc/mediatek/leakage_table_v2/mediatek_static_power.ko")

        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/mtk_mbraink.ko")

        mgk_64_device_modules.remove("drivers/misc/mediatek/mcupm/v2/mcupm.ko")

        mgk_64_device_modules.append("drivers/misc/mediatek/mcdi/mcdi.ko")

        mgk_64_device_modules.remove("drivers/misc/mediatek/mminfra/mtk-mminfra-debug.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mminfra/mm-fake-engine.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mminfra/mtk-mminfra-imax.ko")

        mgk_64_device_modules.remove("drivers/misc/mediatek/pmsr/pmsr.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/pmsr/twam/spmtwam.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/pmsr/v2/pmsr_v2.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/pmsr/v3/pmsr_v3.ko")

        mgk_64_device_modules.remove("drivers/soc/mediatek/devapc/device-apc-common.ko")

        #mgk_64_device_modules.remove("drivers/misc/mediatek/power_throttling/pmic_lvsys_notify.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/power_gs_v1/mtk_power_gs_v1.ko")

        mgk_64_device_modules.remove("drivers/misc/mediatek/qos/mtk_qos.ko")

        mgk_64_device_modules.append("drivers/misc/mediatek/ppm_v3/mtk_ppm_v3.ko")

        mgk_64_device_modules.append("drivers/misc/mediatek/scp/cm4/scp.ko")

        mgk_64_device_modules.remove("drivers/misc/mediatek/sensor/2.0/sensorhub/sensorhub.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/sensor/1.0/gyroscope/gyrohub/gyrohub.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/sensor/1.0/accelerometer/accel_common.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/sensor/1.0/situation/situation.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/sensor/1.0/sensorfusion/fusion.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/sensor/1.0/sensorfusion/gamerotvechub/gamerotvechub.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/sensor/1.0/sensorfusion/gmagrotvechub/gmagrotvechub.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/sensor/1.0/sensorfusion/uncali_acchub/uncali_acchub.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/sensor/1.0/barometer/baro_common.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/sensor/1.0/step_counter/step_counter.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/sensor/1.0/sensorfusion/linearacchub/linearacchub.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/sensor/1.0/step_counter/stepsignhub/stepsignhub.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/sensor/1.0/sensorfusion/gravityhub/gravityhub.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/sensor/1.0/sensorfusion/uncali_maghub/uncali_maghub.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/sensor/1.0/sensor_probe/sensor_probe.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/sensor/1.0/gyroscope/gyro_common.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/sensor/1.0/sensorHub/sensorHub.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/sensor/1.0/alsps/alsps_common.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/sensor/1.0/barometer/barohub/barohub.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/sensor/1.0/sensorfusion/uncali_gyrohub/uncali_gyrohub.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/sensor/1.0/alsps/alspshub/alspshub.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/sensor/1.0/sensorfusion/orienthub/orienthub.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/sensor/1.0/magnetometer/maghub/maghub.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/sensor/1.0/hwmon/sensor_list/sensor_list.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/sensor/1.0/situation/situation_hub/situationhub.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/sensor/1.0/sensorfusion/rotatvechub/rotatvechub.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/sensor/1.0/magnetometer/mag_common.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/sensor/1.0/accelerometer/accelhub/accelhub.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/sensor/1.0/hwmon/hwmon.ko")

        mgk_64_device_modules.append("drivers/misc/mediatek/smi/mtk-smi-bwc.ko")

        mgk_64_device_modules.remove("drivers/misc/mediatek/ssc/debug/v1/mtk-ssc-dbg-v1.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/ssc/debug/v2/mtk-ssc-dbg-v2.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/ssc/mtk-ssc.ko")

        mgk_64_device_modules.append("drivers/misc/mediatek/spm/plat_k68/MTK_INTERNAL_SPM.ko")

        mgk_64_device_modules.append("drivers/misc/mediatek/thermal/thermal_monitor.ko")

        mgk_64_device_modules.remove("drivers/misc/mediatek/sspm/v3/sspm_v3.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/sspm/v1/sspm_v1.ko")

        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v1/mtk-swpm-dbg-common-v1.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6886/mtk-swpm-core-dbg-v6886.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6886/mtk-swpm-cpu-dbg-v6886.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6886/mtk-swpm-dbg-v6886.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6886/mtk-swpm-mem-dbg-v6886.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6897/mtk-swpm-core-dbg-v6897.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6897/mtk-swpm-cpu-dbg-v6897.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6897/mtk-swpm-dbg-v6897.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6897/mtk-swpm-mem-dbg-v6897.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6983/mtk-smap-dbg-v6983.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6983/mtk-swpm-core-dbg-v6983.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6983/mtk-swpm-cpu-dbg-v6983.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6983/mtk-swpm-dbg-v6983.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6983/mtk-swpm-mem-dbg-v6983.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6985/mtk-smap-dbg-v6985.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6985/mtk-swpm-core-dbg-v6985.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6985/mtk-swpm-cpu-dbg-v6985.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6985/mtk-swpm-dbg-v6985.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6985/mtk-swpm-mem-dbg-v6985.ko")

        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/mtk-swpm.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/mtk-swpm-perf-arm-pmu.ko")

        mgk_64_device_modules.remove("drivers/misc/mediatek/tinysys_scmi/tinysys-scmi.ko")

        mgk_64_device_modules.remove("drivers/misc/mediatek/trusted_mem/tmem_ffa.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/trusted_mem/ffa_v11.ko")

        mgk_64_device_modules.append("drivers/misc/mediatek/upower/Upower.ko")

        mgk_64_device_modules.remove("drivers/misc/mediatek/usb/usb_offload/usb_offload.ko")

        mgk_64_device_modules.remove("drivers/misc/mediatek/vcp/rv/vcp.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/vcp/rv/vcp_status.ko")

        mgk_64_device_modules.remove("drivers/misc/mediatek/vdec_fmt/vdec-fmt.ko")

        mgk_64_device_modules.remove("drivers/misc/mediatek/vow/ver02/mtk-vow.ko")

        mgk_64_device_modules.remove("drivers/power/supply/mt6375-charger.ko")
        mgk_64_device_modules.remove("drivers/power/supply/rt9490-charger.ko")
        mgk_64_device_modules.remove("drivers/power/supply/rt9758-charger.ko")

        mgk_64_device_modules.append("drivers/regulator/mt6358-regulator.ko")
        mgk_64_device_modules.remove("drivers/spmi/spmi-mtk-mpu.ko")
        mgk_64_device_modules.remove("drivers/spmi/spmi-mtk-pmif.ko")

        mgk_64_device_modules.remove("drivers/soc/mediatek/mtk-mmdvfs-v3.ko")

        mgk_64_device_modules.remove("drivers/tee/teei/510/isee-ffa.ko")
        mgk_64_device_modules.remove("drivers/tee/teei/515/isee-ffa.ko")

        mgk_64_device_modules.remove("drivers/thermal/mediatek/backlight_cooling.ko")
        mgk_64_device_modules.remove("drivers/thermal/mediatek/board_temp.ko")
        mgk_64_device_modules.remove("drivers/thermal/mediatek/charger_cooling.ko")
        mgk_64_device_modules.remove("drivers/thermal/mediatek/md_cooling_all.ko")
        mgk_64_device_modules.remove("drivers/thermal/mediatek/pmic_temp.ko")
        mgk_64_device_modules.remove("drivers/thermal/mediatek/soc_temp_lvts.ko")
        mgk_64_device_modules.remove("drivers/thermal/mediatek/thermal_interface.ko")
        #mgk_64_device_modules.remove("drivers/thermal/mediatek/thermal_jatm.ko")
        mgk_64_device_modules.remove("drivers/thermal/mediatek/thermal_trace.ko")
        mgk_64_device_modules.remove("drivers/thermal/mediatek/vtskin_temp.ko")
        mgk_64_device_modules.remove("drivers/thermal/mediatek/wifi_cooling.ko")

        mgk_64_device_modules.remove("sound/soc/codecs/mt6338-accdet.ko")
        mgk_64_device_modules.remove("sound/soc/codecs/snd-soc-mt6338.ko")
        mgk_64_device_modules.remove("sound/soc/codecs/snd-soc-mt6368.ko")
        mgk_64_device_modules.remove("sound/soc/mediatek/audio_dsp/mtk-soc-offload-common.ko")
        mgk_64_device_modules.remove("sound/soc/mediatek/audio_dsp/snd-soc-audiodsp-common.ko")
        mgk_64_device_modules.remove("sound/soc/mediatek/ultrasound/ultra_common/mtk-scp-ultra.ko")
        mgk_64_device_modules.remove("sound/soc/mediatek/ultrasound/ultra_scp/snd-soc-mtk-scp-ultra.ko")
        mgk_64_device_modules.remove("sound/soc/mediatek/vow/mtk-scp-vow.ko")
        mgk_64_device_modules.append("sound/soc/codecs/snd-soc-mt6660.ko")
        mgk_64_device_modules.append("sound/soc/codecs/snd-soc-mt6358.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/spi_slave_drv/spi_slave.ko")
        mgk_64_device_modules.remove("drivers/gpu/mediatek/gpueb/gpueb.ko")
        mgk_64_device_modules.remove("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_wrapper.ko")
        mgk_64_device_modules.append("drivers/gpu/mediatek/gpufreq/v2_legacy/mtk_gpufreq_wrapper_legacy.ko")

        mgk_64_platform_device_modules.update({"drivers/clk/mediatek/clkchk-mt6768.ko":"mt6768"})
        mgk_64_platform_device_modules.update({"drivers/clk/mediatek/clkdbg-mt6768.ko":"mt6768"})
        mgk_64_platform_device_modules.update({"drivers/clk/mediatek/clk-fmeter-mt6768.ko":"mt6768"})
        mgk_64_platform_device_modules.update({"drivers/clk/mediatek/clk-mt6768.ko":"mt6768"})
        mgk_64_platform_device_modules.update({"drivers/clk/mediatek/clk-mt6768-pg.ko":"mt6768"})
        mgk_64_platform_device_modules.update({"drivers/clk/mediatek/pd-chk-mt6768.ko":"mt6768"})
        mgk_64_platform_device_modules.pop("drivers/clk/mediatek/clk-chk-mt6991.ko")
        mgk_64_platform_device_modules.pop("drivers/clk/mediatek/pd-chk-mt6991.ko")

        mgk_64_platform_device_modules.pop("drivers/gpu/drm/mediatek/mml/mtk-mml-mt6878.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/drm/mediatek/mml/mtk-mml-mt6886.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/drm/mediatek/mml/mtk-mml-mt6897.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/drm/mediatek/mml/mtk-mml-mt6983.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/drm/mediatek/mml/mtk-mml-mt6985.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/drm/mediatek/mml/mtk-mml-mt6989.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/drm/mediatek/mml/mtk-mml-mt6991.ko")
        mgk_64_platform_device_modules.update({"drivers/gpu/mediatek/gpufreq/v2_legacy/mtk_gpufreq_mt6768.ko":"mt6768"})

        mgk_64_platform_device_modules.update({"drivers/interconnect/mediatek/mmqos-mt6768.ko":"mt6768"})

        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/cm_mgr/mtk_cm_mgr_mt6886.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/cm_mgr/mtk_cm_mgr_mt6897.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/cm_mgr/mtk_cm_mgr_mt6983.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/cm_mgr/mtk_cm_mgr_mt6985.ko")
        mgk_64_platform_device_modules.update({"drivers/misc/mediatek/cm_mgr_legacy_v1/mtk_cm_mgr_mt6768.ko":"mt6768"})
        mgk_64_platform_device_modules.update({"drivers/misc/mediatek/cm_mgr_legacy_v1/mtk_cm_mgr_mt6893.ko":"mt6893"})

        mgk_64_platform_device_modules.update({"drivers/pinctrl/mediatek/pinctrl-mt6768.ko":"mt6768"})

        mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6886/mt6886-mt6368.ko")
        mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6886/snd-soc-mt6886-afe.ko")
        mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6897/mt6897-mt6368.ko")
        mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6897/snd-soc-mt6897-afe.ko")
        mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6983/mt6983-mt6338.ko")
        mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6983/snd-soc-mt6983-afe.ko")
        mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6985/mt6985-mt6338.ko")
        mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6985/snd-soc-mt6985-afe.ko")
        mgk_64_platform_device_modules.update({"sound/soc/mediatek/mt6768/mt6768-mt6358.ko":"mt6768"})
        mgk_64_platform_device_modules.update({"sound/soc/mediatek/mt6768/snd-soc-mt6768-afe.ko":"mt6768"})

        mgk_64_device_modules.remove("drivers/memory/mediatek/slc-parity.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/ise_lpm/ise_lpm.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/lpm/governors/MHSP/lpm-gov-MHSP.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/lpm/modules/debug/v1/mtk-lpm-dbg-common-v1.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/lpm/modules/debug/v2/mtk-lpm-dbg-common-v2.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/lpm/modules/platform/v1/mtk-lpm-plat-v1.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/lpm/modules/platform/v2/mtk-lpm-plat-v2.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/lpm/mtk-lpm.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mmdebug/mtk-mmdebug-vcp.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mdpm/mtk_mdpm.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/mdpm_v1/mtk_mdpm_v1.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/slbc/mmsram.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/slbc/mtk_slbc.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/slbc/slbc_ipi.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/slbc/slbc_trace.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6897/mtk-swpm-audio-dbg-v6897.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6989/mtk-swpm-audio-dbg-v6989.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6989/mtk-swpm-core-dbg-v6989.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6989/mtk-swpm-cpu-dbg-v6989.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6989/mtk-swpm-dbg-v6989.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6989/mtk-swpm-disp-dbg-v6989.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6989/mtk-swpm-mem-dbg-v6989.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6989/mtk-swpm-mml-dbg-v6989.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/trusty/ise-trusty.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/trusty/ise-trusty-ipc.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/trusty/ise-trusty-log.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/trusty/ise-trusty-virtio.ko")
        mgk_64_device_modules.remove("sound/soc/codecs/mt6681-accdet.ko")
        mgk_64_device_modules.remove("sound/soc/codecs/snd-soc-mt6359.ko")

        mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6886.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6897.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6985.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6989.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6989_fpga.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6991.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/cm_mgr/mtk_cm_mgr_mt6989.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/lpm/modules/debug/mt6886/mtk-lpm-dbg-mt6886.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/lpm/modules/debug/mt6897/mtk-lpm-dbg-mt6897.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/lpm/modules/debug/mt6983/mtk-lpm-dbg-mt6983.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/lpm/modules/debug/mt6985/mtk-lpm-dbg-mt6985.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/lpm/modules/debug/mt6989/mtk-lpm-dbg-mt6989.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/lpm/modules/debug/mt6991/mtk-lpm-dbg-mt6991.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/slbc/slbc_mt6886.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/slbc/slbc_mt6893.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/slbc/slbc_mt6895.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/slbc/slbc_mt6897.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/slbc/slbc_mt6983.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/slbc/slbc_mt6985.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/slbc/slbc_mt6989.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/slbc/slbc_mt6991.ko")
        mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6885/snd-soc-mt6885-afe.ko")
        mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6885/mt6885-mt6359p.ko")
        mgk_64_platform_device_modules.pop("drivers/clk/mediatek/clk-chk-mt6989.ko")
        mgk_64_platform_device_modules.pop("drivers/clk/mediatek/pd-chk-mt6989.ko")
        mgk_64_platform_device_modules.pop("drivers/soc/mediatek/devapc/device-apc-mt6879.ko")
        mgk_64_platform_device_modules.pop("drivers/soc/mediatek/devapc/device-apc-mt6886.ko")
        mgk_64_platform_device_modules.pop("drivers/soc/mediatek/devapc/device-apc-mt6893.ko")
        mgk_64_platform_device_modules.pop("drivers/soc/mediatek/devapc/device-apc-mt6895.ko")
        mgk_64_platform_device_modules.pop("drivers/soc/mediatek/devapc/device-apc-mt6897.ko")
        mgk_64_platform_device_modules.pop("drivers/soc/mediatek/devapc/device-apc-mt6983.ko")
        mgk_64_platform_device_modules.pop("drivers/soc/mediatek/devapc/device-apc-mt6985.ko")
        mgk_64_platform_device_modules.pop("drivers/soc/mediatek/devapc/device-apc-mt6989.ko")
        mgk_64_platform_device_modules.pop("drivers/soc/mediatek/devapc/device-apc-mt6991.ko")

    if "mt6761_overlay.config" in DEFCONFIG_OVERLAYS:
        mgk_64_platform_device_modules.update({"drivers/regulator/mt6357-regulator.ko":"mt6761"})
        mgk_64_platform_device_modules.update({"drivers/pinctrl/mediatek/pinctrl-mt6761.ko":"mt6761"})
        mgk_64_platform_device_modules.update({"drivers/clk/mediatek/clkchk-mt6761.ko":"mt6761"})
        mgk_64_platform_device_modules.update({"drivers/clk/mediatek/clk-mt6761-audio.ko":"mt6761"})
        mgk_64_platform_device_modules.update({"drivers/clk/mediatek/clk-mt6761-cam.ko":"mt6761"})
        mgk_64_platform_device_modules.update({"drivers/clk/mediatek/clk-mt6761-mipi0a.ko":"mt6761"})
        mgk_64_platform_device_modules.update({"drivers/clk/mediatek/clk-mt6761-mm.ko":"mt6761"})
        mgk_64_platform_device_modules.update({"drivers/clk/mediatek/clk-mt6761-vcodec.ko":"mt6761"})
        mgk_64_platform_device_modules.update({"drivers/clk/mediatek/clkdbg-mt6761.ko":"mt6761"})
        mgk_64_platform_device_modules.update({"drivers/clk/mediatek/clk-fmeter-mt6761.ko":"mt6761"})
        mgk_64_platform_device_modules.update({"drivers/clk/mediatek/clk-mt6761.ko":"mt6761"})
        mgk_64_platform_device_modules.update({"drivers/clk/mediatek/clk-mt6761-pg.ko":"mt6761"})
        mgk_64_platform_device_modules.update({"drivers/clk/mediatek/pd-chk-mt6761.ko":"mt6761"})
        mgk_64_platform_device_modules.update({"drivers/misc/mediatek/leakage_table_v2/mediatek_static_power.ko":"mt6761"})
        mgk_64_platform_device_modules.update({"drivers/misc/mediatek/dvfsrc/dvfsrc-opp-mt6761.ko":"mt6761"})
        mgk_64_platform_device_modules.update({"drivers/soc/mediatek/mtk-dvfsrc.ko":"mt6761"})
        mgk_64_platform_device_modules.update({"drivers/regulator/mtk-dvfsrc-regulator.ko":"mt6761"})
        mgk_64_platform_device_modules.update({"drivers/interconnect/mediatek/mtk-emi.ko":"mt6761"})
        mgk_64_platform_device_modules.update({"drivers/misc/mediatek/dvfsrc/mtk-dvfsrc-helper.ko":"mt6761"})
        mgk_64_platform_device_modules.update({"drivers/devfreq/mtk-dvfsrc-devfreq.ko":"mt6761"})
        mgk_64_platform_device_modules.update({"drivers/soc/mediatek/mtk-dvfsrc-start.ko":"mt6761"})
        mgk_64_device_modules.remove("drivers/char/rpmb/rpmb.ko")
        mgk_64_device_modules.remove("drivers/char/rpmb/rpmb-mtk.ko")

        mgk_64_device_modules.append("drivers/misc/mediatek/hps_v3/mtk_cpuhp.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/power_gs_v1/mtk_power_gs_v1.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cm_mgr_legacy_v0/mtk_cm_mgr_v0.ko")

        mgk_64_device_modules.remove("drivers/memory/mediatek/emi.ko")
        mgk_64_device_modules.remove("drivers/memory/mediatek/emi-fake-eng.ko")
        mgk_64_device_modules.remove("drivers/memory/mediatek/emi-mpu.ko")
        mgk_64_device_modules.remove("drivers/memory/mediatek/emi-mpu-test.ko")
        mgk_64_device_modules.remove("drivers/memory/mediatek/emi-mpu-test-v2.ko")
        mgk_64_device_modules.remove("drivers/memory/mediatek/emi-slb.ko")
        mgk_64_device_modules.remove("drivers/memory/mediatek/smpu.ko")
        mgk_64_device_modules.remove("drivers/memory/mediatek/slc-parity.ko")
        mgk_64_device_modules.remove("drivers/memory/mediatek/smpu-hook-v1.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi_legacy_v1/emicen.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi_legacy_v1/emiisu.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi_legacy_v1/emimpu.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi_legacy_v1/emictrl.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi-dummy.ko")

    if "mt6768_overlay_ref.config" in DEFCONFIG_OVERLAYS:
        mgk_64_device_modules.remove("drivers/misc/mediatek/typec/mux/fusb304.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/typec/mux/mux_switch.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/typec/mux/ps5169.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/typec/mux/ps5170.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/typec/mux/ptn36241g.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/typec/mux/usb_dp_selector.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/typec/tcpc/pd_dbg_info.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/typec/tcpc/rt_pd_manager.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/typec/tcpc/tcpc_class.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/typec/tcpc/tcpci_late_sync.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/typec/tcpc/tcpc_mt6360.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/typec/tcpc/tcpc_mt6370.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/typec/tcpc/tcpc_mt6375.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/typec/tcpc/tcpc_rt1711h.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/typec/tcpc/tcpc_mt6379.ko")

        mgk_64_device_modules.remove("drivers/power/supply/mt6360_charger.ko")
        mgk_64_device_modules.remove("drivers/power/supply/mt6360_pmu_chg.ko")
        mgk_64_device_modules.remove("drivers/power/supply/mtk_pd_adapter.ko")
        mgk_64_device_modules.remove("drivers/power/supply/mtk_pd_charging.ko")
        mgk_64_device_modules.remove("drivers/power/supply/mtk_chg_type_det.ko")

        mgk_64_device_modules.remove("drivers/power/supply/mtk_hvbpc.ko")
        mgk_64_device_modules.remove("drivers/power/supply/mtk_pep.ko")
        mgk_64_device_modules.remove("drivers/power/supply/mtk_pep20.ko")
        mgk_64_device_modules.remove("drivers/power/supply/mtk_pep40.ko")
        mgk_64_device_modules.remove("drivers/power/supply/mtk_pep45.ko")
        mgk_64_device_modules.remove("drivers/power/supply/mtk_pep50.ko")
        mgk_64_device_modules.remove("drivers/power/supply/mtk_pep50p.ko")
        mgk_64_device_modules.remove("drivers/power/supply/rt9759.ko")
        mgk_64_device_modules.remove("drivers/power/supply/mtk_2p_charger.ko")

        mgk_64_device_modules.append("drivers/power/supply/sgm41516d.ko")
        mgk_64_device_modules.append("drivers/power/supply/mtk_chg_det.ko")

    if "mt6893_overlay.config" in DEFCONFIG_OVERLAYS:
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/gpu:gpu")
        mgk_64_kleaf_modules.append("//vendor/mediatek/kernel_modules/gpu:gpu_mt6893")
        mgk_64_platform_device_modules.update({"drivers/pinctrl/mediatek/pinctrl-mt6885.ko":"mt6893"})

get_overlay_modules_list()
