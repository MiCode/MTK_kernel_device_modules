load("//kernel_device_modules-mainline:kernel/kleaf/mgk_modules.bzl",
     "mgk_module_outs",
     "mgk_module_eng_outs",
     "mgk_module_userdebug_outs",
     "mgk_module_user_outs"
)

load("@mgk_info//:dict.bzl",
    "DEFCONFIG_OVERLAYS",
)

load("@mgk_info//:kernel_version.bzl",
     "kernel_version",
)

mgk_64_defconfig = "mgk_64_kmainline_defconfig"

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
    "//vendor/mediatek/kernel_modules/connectivity/wlan/core/gen4m/build/connac1x/6765:wlan_drv_gen4m_6765",
    "//vendor/mediatek/kernel_modules/connectivity/wlan/core/gen4m/build/connac1x/6768:wlan_drv_gen4m_6768",
    "//vendor/mediatek/kernel_modules/connectivity/wlan/core/gen4m/build/connac1x/6781:wlan_drv_gen4m_6781",
    "//vendor/mediatek/kernel_modules/connectivity/wlan/core/gen4m/build/connac2x/6877:wlan_drv_gen4m_6877",
    "//vendor/mediatek/kernel_modules/connectivity/wlan/core/gen4m/build/connac2x/6893:wlan_drv_gen4m_6893",
    "//vendor/mediatek/kernel_modules/connectivity/wlan/core/gen4m/build/connac2x/6897:wlan_drv_gen4m_6897",
    "//vendor/mediatek/kernel_modules/connectivity/wlan/core/gen4m/build/connac3x/6985_6639:wlan_drv_gen4m_6985_6639",
    "//vendor/mediatek/kernel_modules/connectivity/wlan/core/gen4m/build/connac3x/6989_6639:wlan_drv_gen4m_6989_6639",
    "//vendor/mediatek/kernel_modules/connectivity/wlan/core/gen4m/build/connac3x/6989_6639_dppm:wlan_drv_gen4m_6989_6639_dppm",
    "//vendor/mediatek/kernel_modules/connectivity/wlan/core/gen4m/build/connac3x/6991_6653:wlan_drv_gen4m_6991_6653",
    "//vendor/mediatek/kernel_modules/connectivity/wlan/core/gen4m/build/connac3x/6991_6653_2g2a:wlan_drv_gen4m_6991_6653_2g2a",
    "//vendor/mediatek/kernel_modules/connectivity/wlan/core/gen4m/build/connac3x/6991_6653_triband:wlan_drv_gen4m_6991_6653_triband",
    "//vendor/mediatek/kernel_modules/connectivity/wlan/core/gen4m/build/connac3x/6991_6653_dx5:wlan_drv_gen4m_6991_6653_dx5",
    "//vendor/mediatek/kernel_modules/connectivity/wlan/core/gen4m/build/connac3x/6991_6653_dx5_2g2a:wlan_drv_gen4m_6991_6653_dx5_2g2a",
    "//vendor/mediatek/kernel_modules/connectivity/wlan/core/gen4m/build/connac3x/eap_6653:wlan_drv_gen4m_eap_6653",
    "//vendor/mediatek/kernel_modules/cpufreq_cus:cpu_freq",
    "//vendor/mediatek/kernel_modules/cpufreq_int:cpu_hwtest",
    "//vendor/mediatek/kernel_modules/fpsgo_cus:fpsgo_cus",
    "//vendor/mediatek/kernel_modules/fpsgo_int:fpsgo_int",
    "//vendor/mediatek/kernel_modules/afs_common_utils:jank_detection_common_utils",
    "//vendor/mediatek/kernel_modules/afs_core_int:jank_detection_core_int",
    "//vendor/mediatek/kernel_modules/afs_core_cus:jank_detection_core_cus",
    "//vendor/mediatek/kernel_modules/gpu:gpu",
    "//vendor/mediatek/kernel_modules/hbt_driver_cus:hbt_cus",
    "//vendor/mediatek/kernel_modules/hbt_driver:hbt_int",
    "//vendor/mediatek/kernel_modules/met_drv_secure_v3:met_drv_secure_v3",
    "//vendor/mediatek/kernel_modules/met_drv_v3/met_api:met_api_v3_cus",
    "//vendor/mediatek/kernel_modules/met_drv_v3/met_api:met_api_v3_int",
    "//vendor/mediatek/kernel_modules/met_drv_v3:met_drv_v3",
    #"//vendor/mediatek/kernel_modules/msync2_frd_cus/build:msync2_frd_cus",
    #"//vendor/mediatek/kernel_modules/msync2_frd_int:msync2_frd_int",
    "//vendor/mediatek/kernel_modules/mtk_input/FT3518U:ft3518u",
    #"//vendor/mediatek/kernel_modules/mtk_input/GT9886:gt9886",
    "//vendor/mediatek/kernel_modules/mtk_input/GT9916:gt9916",
    "//vendor/mediatek/kernel_modules/mtk_input/NT36672C:nt36672c",
    "//vendor/mediatek/kernel_modules/mtk_input/nt36xxx_no_flash_spi:nt36xxx_no_flash_spi",
    "//vendor/mediatek/kernel_modules/mtk_input/hxchipset_hx83102p:hxchipset_hx83102p",
    "//vendor/mediatek/kernel_modules/mtk_input/TD4320:td4320",
    "//vendor/mediatek/kernel_modules/mtk_input/ST61Y:st61y",
    "//vendor/mediatek/kernel_modules/mtk_input/fingerprint/goodix/5.10:gf_spi",
    "//vendor/mediatek/kernel_modules/mtk_input/synaptics_tcm:synaptics_tcm",
    "//vendor/mediatek/kernel_modules/mtkcam/camsys:ccd_rpmsg",
    "//vendor/mediatek/kernel_modules/mtkcam/camsys:mtk_ccd_remoteproc",
    "//vendor/mediatek/kernel_modules/mtkcam/camsys:mtk-cam-plat-util",

    "//vendor/mediatek/kernel_modules/mtkcam/camsys:camsys",
    "//vendor/mediatek/kernel_modules/mtkcam/cam_cal/src_v4l2/custom:mtk_cam_cal",
    "//vendor/mediatek/kernel_modules/mtkcam/ccusys:ccusys",
    "//vendor/mediatek/kernel_modules/mtkcam/imgsensor:mtk_imgsensor",
    "//vendor/mediatek/kernel_modules/mtkcam/imgsys/common:imgsys_common",
    "//vendor/mediatek/kernel_modules/mtkcam/imgsys/imgsys/isp8s:imgsys_8s",
    "//vendor/mediatek/kernel_modules/mtkcam/imgsys/imgsys/isp8:imgsys_8",
    "//vendor/mediatek/kernel_modules/mtkcam/imgsys/imgsys/isp7sp:imgsys_7sp",
    "//vendor/mediatek/kernel_modules/mtkcam/imgsys/imgsys/isp7s:imgsys_7s",
    "//vendor/mediatek/kernel_modules/mtkcam/imgsys/imgsys/isp71:imgsys_71",
    "//vendor/mediatek/kernel_modules/mtkcam/imgsys/imgsys/cmdq/legacy:imgsys_cmdq",
    "//vendor/mediatek/kernel_modules/mtkcam/imgsys/imgsys/cmdq/isp8:imgsys_cmdq_isp8",
    "//vendor/mediatek/kernel_modules/mtkcam/imgsys/imgsys/cmdq/isp8s:imgsys_cmdq_isp8s",
    "//vendor/mediatek/kernel_modules/mtkcam/mtk-aie:mtk-aie",
    "//vendor/mediatek/kernel_modules/mtkcam/mtk-mae:mtk_mae",
    "//vendor/mediatek/kernel_modules/mtkcam/mtk-hcp/legacy:mtk-hcp",
    "//vendor/mediatek/kernel_modules/mtkcam/mtk-hcp/isp8:mtk-hcp-isp8",
    "//vendor/mediatek/kernel_modules/mtkcam/mtk-hcp/isp8s:mtk-hcp-isp8s",
    "//vendor/mediatek/kernel_modules/mtkcam/mtk-ipesys-me:mtk-ipesys-me",
    "//vendor/mediatek/kernel_modules/mtkcam/mtk-dpe:mtk-dpe",
    "//vendor/mediatek/kernel_modules/mtkcam/mtk-pda:mtk-pda",
    "//vendor/mediatek/kernel_modules/mtkcam/sched:c2ps",
    "//vendor/mediatek/kernel_modules/mtkcam/sched/c2ps_ioctl:c2ps_perf_ioctl",
    "//vendor/mediatek/kernel_modules/mtkcam/scpsys/mtk-aov:mtk_aov",
    "//vendor/mediatek/kernel_modules/mtkcam/isp_pspm:isp_pspm",
    "//vendor/mediatek/kernel_modules/sched_cus:sched_cus",
    "//vendor/mediatek/kernel_modules/sched_int:sched_int",
    "//vendor/mediatek/kernel_modules/mtkcam/img_frm_sync:mtk-img-frm-sync",
    "//vendor/mediatek/kernel_modules/task_turbo_cus:task_turbo_cus",
    "//vendor/mediatek/kernel_modules/task_turbo_int:task_turbo_int",
    "//vendor/mediatek/kernel_modules/mtkcam/imgsensor/src-v4l2/imgsensor-glue:imgsensor-glue",
    "//vendor/mediatek/kernel_modules/mtkcam/imgsensor/src-isp8/imgsensor-glue:imgsensor-glue_isp8",
    "//vendor/mediatek/kernel_modules/mtkcam/imgsensor/src_spm-isp8/imgsensor-glue:imgsensor-glue_spm_isp8",
    "//vendor/mediatek/kernel_modules/mtkcam/imgsensor/src-isp8s/imgsensor-glue:imgsensor-glue_isp8s",
]

mgk_64_kleaf_eng_modules = [
    "//vendor/mediatek/tests/kernel/ktf_testcase:ktf_testcase",
    "//vendor/mediatek/tests/kernel/ktf_testcase/efuse/efuse_ut:ktf_efuse_ut",
    "//vendor/mediatek/tests/kernel/ktf_testcase/example:ktf_hello",
    "//vendor/mediatek/tests/kernel/ktf_testcase/mmc:ktf_mmc_ut",
    "//vendor/mediatek/tests/kernel/ktf_testcase/mkp:ktf_mkp_ut",
    "//vendor/mediatek/tests/kernel/ktf_testcase/mkp/mkp_ait:ktf_mkp_ait",
    "//vendor/mediatek/tests/kernel/ktf_testcase/mkp/mkp_it:ktf_mkp_it",
    "//vendor/mediatek/tests/kernel/ktf_testcase/selftest:ktf_selftest",
    "//vendor/mediatek/tests/ktf/kernel:ktf_ddk",
    "//vendor/mediatek/tests/kernel/ktf_testcase/atf_fuzzer:atf_fuzzer",
    "//vendor/mediatek/tests/kernel/ktf_testcase/accdet/accdet_ait:accdet_ait",
    "//vendor/mediatek/tests/kernel/ktf_testcase/accdet/accdet_ait:accdet_ait_fuzz",
    "//vendor/mediatek/tests/kernel/ktf_testcase/dma_buf/dma_buf_ait:ktf_dma_buf_ait",
    "//vendor/mediatek/tests/kernel/ktf_testcase/i2c/i2c_ait:ktf_i2c_ait",
    "//vendor/mediatek/tests/kernel/ktf_testcase/typec:ktf_i2c_suspend",
    "//vendor/mediatek/tests/kernel/ktf_testcase/iommu/iommu_ait:ktf_iommu_ait",
    "//vendor/mediatek/tests/kernel/ktf_testcase/pw_charger:ktf_pw_charger",
    "//vendor/mediatek/tests/kernel/ktf_testcase/pw_gauge:ktf_pw_gauge_it",
    "//vendor/mediatek/tests/kernel/ktf_testcase/selinux:ktf_selinux",
    "//vendor/mediatek/tests/kernel/ktf_testcase/sensorhub/sensorhub_ait:ktf_sensorhub_ait",
    "//vendor/mediatek/tests/kernel/ktf_testcase/smi/smi_ait:ktf_smi_ait",
    "//vendor/mediatek/tests/kernel/ktf_testcase/cmdq_gce/cmdq_gce_ait:ktf_cmdq_ait",
    "//vendor/mediatek/tests/kernel/ktf_testcase/mml:ktf_mml_ait",
    "//vendor/mediatek/tests/kernel/ktf_testcase/trusted_mem/trusted_mem_ait:ktf_trusted_mem_ait",
]

mgk_64_kleaf_userdebug_modules = [
    "//vendor/mediatek/tests/kernel/ktf_testcase:ktf_testcase",
    "//vendor/mediatek/tests/kernel/ktf_testcase/efuse/efuse_ut:ktf_efuse_ut",
    "//vendor/mediatek/tests/kernel/ktf_testcase/example:ktf_hello",
    "//vendor/mediatek/tests/kernel/ktf_testcase/mmc:ktf_mmc_ut",
    "//vendor/mediatek/tests/kernel/ktf_testcase/mkp:ktf_mkp_ut",
    "//vendor/mediatek/tests/kernel/ktf_testcase/mkp/mkp_ait:ktf_mkp_ait",
    "//vendor/mediatek/tests/kernel/ktf_testcase/mkp/mkp_it:ktf_mkp_it",
    "//vendor/mediatek/tests/kernel/ktf_testcase/selftest:ktf_selftest",
    "//vendor/mediatek/tests/ktf/kernel:ktf_ddk",
    "//vendor/mediatek/tests/kernel/ktf_testcase/atf_fuzzer:atf_fuzzer",
    "//vendor/mediatek/tests/kernel/ktf_testcase/accdet/accdet_ait:accdet_ait",
    "//vendor/mediatek/tests/kernel/ktf_testcase/accdet/accdet_ait:accdet_ait_fuzz",
    "//vendor/mediatek/tests/kernel/ktf_testcase/dma_buf/dma_buf_ait:ktf_dma_buf_ait",
    "//vendor/mediatek/tests/kernel/ktf_testcase/i2c/i2c_ait:ktf_i2c_ait",
    "//vendor/mediatek/tests/kernel/ktf_testcase/typec:ktf_i2c_suspend",
    "//vendor/mediatek/tests/kernel/ktf_testcase/iommu/iommu_ait:ktf_iommu_ait",
    "//vendor/mediatek/tests/kernel/ktf_testcase/pw_charger:ktf_pw_charger",
    "//vendor/mediatek/tests/kernel/ktf_testcase/pw_gauge:ktf_pw_gauge_it",
    "//vendor/mediatek/tests/kernel/ktf_testcase/selinux:ktf_selinux",
    "//vendor/mediatek/tests/kernel/ktf_testcase/sensorhub/sensorhub_ait:ktf_sensorhub_ait",
    "//vendor/mediatek/tests/kernel/ktf_testcase/smi/smi_ait:ktf_smi_ait",
    "//vendor/mediatek/tests/kernel/ktf_testcase/cmdq_gce/cmdq_gce_ait:ktf_cmdq_ait",
    "//vendor/mediatek/tests/kernel/ktf_testcase/mml:ktf_mml_ait",
    "//vendor/mediatek/tests/kernel/ktf_testcase/trusted_mem/trusted_mem_ait:ktf_trusted_mem_ait",
]

mgk_64_kleaf_user_modules = [
]


mgk_64_module_outs = [
]

mgk_64_common_modules = mgk_module_outs + mgk_64_module_outs
mgk_64_common_eng_modules = mgk_module_outs + mgk_module_eng_outs + mgk_64_module_outs
mgk_64_common_userdebug_modules = mgk_module_outs + mgk_module_userdebug_outs + mgk_64_module_outs
mgk_64_common_user_modules = mgk_module_outs + mgk_module_user_outs + mgk_64_module_outs

mgk_64_kleaf_device_modules_srcs = [
    # keep sorted
    "//kernel_device_modules-{}/drivers/char/rpmb:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/cpufreq:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/dma-buf/heaps:ddk_src".format(kernel_version),
    "//kernel_device_modules-{}/drivers/edac/mediatek:makefiles".format(kernel_version),
    "//kernel_device_modules-{}/drivers/iommu/arm/arm-smmu-v3:ddk_src".format(kernel_version),
    "//kernel_device_modules-{}/drivers/clk/mediatek:ddk_makefile".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/aee/mrdump:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/drm/panel:ddk_makefile".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/drm/mediatek/mediatek_v2:ddk_makefile".format(kernel_version),
    "//kernel_device_modules-{}/drivers/iio/adc:ddk_src".format(kernel_version),
    "//kernel_device_modules-{}/drivers/interconnect/mediatek:makefiles".format(kernel_version),
    "//kernel_device_modules-{}/drivers/leds:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/nvmem:ddk_makefiles".format(kernel_version),
    "//kernel_device_modules-{}/drivers/mfd:ddk_src".format(kernel_version),
    "//kernel_device_modules-{}/drivers/memory/mediatek:makefiles".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/btif:ddk_makefiles".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/blocktag:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/ccmni:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/dvfsrc:ddk_makefiles".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/geniezone:ddk_makefile".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/log_store:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mdpm:ddk_makefile".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/monitor_hang:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/pbm:ddk_makefile".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/pcie:ddk_makefile".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/power_throttling:ddk_makefile".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/rps:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/sensor/2.0/core:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/trusted_mem:ddk_makefile".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/aee/aed:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/iommu:ddk_src".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mmdebug:ddk_makefile".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mmdvfs:ddk_makefile".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mtprintk:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/sda:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/pmsr:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/pmsr/v2:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/slbc:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/subpmic:ddk_src".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/swpm:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug/v1:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/typec/tcpc:ddk_src".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/typec/mux:ddk_src".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/usb/usb_rndis:ddk_src".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/usb/usb_xhci:ddk_makefile".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/usb/usb20:ddk_makefile".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/gate_ic:ddk_makefile".format(kernel_version),
    "//kernel_device_modules-{}/drivers/mmc/host:ddk_srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/phy/mediatek:ddk_makefile".format(kernel_version),
    "//kernel_device_modules-{}/drivers/power/supply/ufcs:ddk_src".format(kernel_version),
    "//kernel_device_modules-{}/drivers/power/supply:ddk_src".format(kernel_version),
    "//kernel_device_modules-{}/drivers/regulator:ddk_src".format(kernel_version),
    "//kernel_device_modules-{}/drivers/soc/mediatek:makefiles".format(kernel_version),
    "//kernel_device_modules-{}/drivers/thermal/mediatek:ddk_makefile".format(kernel_version),
    "//kernel_device_modules-{}/drivers/usb/mtu3:ddk_srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/cpufreq_lite:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/performance:ddk_src".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mtprof:srcs".format(kernel_version),
    "//kernel_device_modules-{}/sound/soc/mediatek/common:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mminfra:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mdp:ddk_src".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/cameraisp/src/isp_6s:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/tinysys_scmi:ddk_srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/mailbox:ddk_srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/lpm:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/lpm/modules/debug:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mkp:ddk_makefile".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mmstat:ddk_makefile".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/sspm/v1:ddk_makefile".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mtk-interconnect:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/remoteproc:srcs".format(kernel_version),
]

mgk_64_kleaf_device_modules_kconfigs = [
    # keep sorted
    "//kernel_device_modules-{}/drivers/char/rpmb:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/cpufreq:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/dma-buf/heaps:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/edac/mediatek:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/iommu/arm/arm-smmu-v3:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/clk/mediatek:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/drm/panel:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/drm/mediatek/mediatek_v2:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/mediatek/hal:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/iio/adc:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/input/keyboard:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/interconnect/mediatek:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/leds:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/nvmem:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/memory/mediatek:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/mfd:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/blocktag:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/btif:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/ccmni:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/ccu/src/isp6s:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/cpufreq_v1:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/cpufreq_lite:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/dvfsrc:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/geniezone:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/geniezone/gz-trusty:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/iommu:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/irtx:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/log_store:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mmdebug:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/monitor_hang:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mtprintk:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/ips:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mdpm:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/pbm:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/pcie:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/pidmap:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/pmsr:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/pmsr/v2:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/power_throttling:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/pwm:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/rps:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/sda:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/selinux_warning:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/slbc:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/smap:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/subpmic:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/swpm:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/trusted_mem:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/typec/tcpc:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/typec/mux:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/usb/usb_meta:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/usb/usb_rndis:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/usb/usb_xhci:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/usb/usb20:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/wla:wla_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/gate_ic:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/mmc/host:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/phy/mediatek:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/pinctrl/mediatek:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/power/supply/ufcs:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/power/supply:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/regulator:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/remoteproc:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/reset:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/rpmsg:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/soc/mediatek:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/thermal/mediatek:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/trusty:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/usb/mtu3:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/performance:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mdp:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/cameraisp/src/isp_6s:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/watchdog:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mtprof:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/sound/soc/mediatek/common:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mminfra:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/tinysys_scmi:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/mailbox:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/lpm:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mkp:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mmstat:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/sspm/v1:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mtk-interconnect:ddk_kconfigs".format(kernel_version),
]

mgk_64_kleaf_device_modules = [
    # keep sorted
    "//kernel_device_modules-{}/drivers/char/rpmb:rpmb".format(kernel_version),
    "//kernel_device_modules-{}/drivers/cpufreq:mediatek-cpufreq-hw".format(kernel_version),
    "//kernel_device_modules-{}/drivers/dma-buf/heaps:mtk_heap_refill".format(kernel_version),
    "//kernel_device_modules-{}/drivers/dma-buf/heaps:system_heap".format(kernel_version),
    "//kernel_device_modules-{}/drivers/edac/mediatek:mtk_edac_slc".format(kernel_version),
    "//kernel_device_modules-{}/drivers/iommu/arm/arm-smmu-v3:mtk-smmuv3-mpam-mon".format(kernel_version),
    "//kernel_device_modules-{}/drivers/clk/mediatek:clk-disable-unused".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/aee/mrdump:mrdump".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/gate_ic:rt4831a_drv".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/drm/panel:panel-aw37501-i2c".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/drm/panel:rt4831a".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/drm/panel:panel-hx83112b-auo-cmd-60hz-rt5081".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/drm/panel:panel-hx83112b-auo-vdo-60hz-rt5081".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/drm/panel:panel-nt35521_hd_dsi_vdo_truly_rt5081".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/drm/panel:panel-nt35695b-auo-vdo".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/drm/panel:panel-sc-nt36672c-vdo-90hz-6382".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/drm/panel:panel-td4320-fhdp-dsi-vdo-auo-rt5081".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/drm/panel:panel-nt36672c-fhdp-dsi-vdo-dsc-txd-boe".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/drm/panel:panel-alpha-jdi-nt36672e-cphy-vdo".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/drm/panel:panel-alpha-jdi-nt36672e-vdo-120hz-frameratev5".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/drm/panel:panel-alpha-jdi-nt36672e-vdo-144hz".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/drm/panel:panel-nt35695b-auo-cmd".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/drm/panel:panel-nt37707-c2v-arp".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/drm/panel:panel-rm692h5-alpha-cmd-spr".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/drm/panel:panel-rm692h5-cmd".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/drm/panel:panel-tianma-r66451-cmd-120hz".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/drm/panel:panel-tianma-r66451-cmd-120hz-wa".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/drm/panel:panel-truly-nt35595-cmd".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/drm/panel:panel-truly-td4330-vdo".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/drm/mediatek/mediatek_v2:mtk_disp_notify".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/drm/mediatek/mediatek_v2:mtk_panel_ext".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/drm/mediatek/mediatek_v2:mtk_sync".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/mediatek/hal:mtk_gpu_hal".format(kernel_version),
    "//kernel_device_modules-{}/drivers/iio/adc:mt635x-auxadc".format(kernel_version),
    "//kernel_device_modules-{}/drivers/iio/adc:mt6360-adc".format(kernel_version),
    "//kernel_device_modules-{}/drivers/iio/adc:mt6370-adc".format(kernel_version),
    "//kernel_device_modules-{}/drivers/iio/adc:mt6375-adc".format(kernel_version),
    "//kernel_device_modules-{}/drivers/iio/adc:mt6375-auxadc".format(kernel_version),
    "//kernel_device_modules-{}/drivers/iio/adc:mt6379-adc".format(kernel_version),
    "//kernel_device_modules-{}/drivers/iio/adc:mtk-spmi-pmic-adc".format(kernel_version),
    "//kernel_device_modules-{}/drivers/iio/adc:rt9490-adc".format(kernel_version),
    "//kernel_device_modules-{}/drivers/input/keyboard:mtk-kpd".format(kernel_version),
    "//kernel_device_modules-{}/drivers/input/touchscreen/GT1151:gt1151".format(kernel_version),
    "//kernel_device_modules-{}/drivers/input/touchscreen/tui_common:tui-common".format(kernel_version),
    "//kernel_device_modules-{}/drivers/input/touchscreen/GT9886:gt9886".format(kernel_version),
    "//kernel_device_modules-{}/drivers/input/keyboard:mtk-pmic-keys".format(kernel_version),
    "//kernel_device_modules-{}/drivers/input/touchscreen/NT36672C_I2C:nt36672c_i2c".format(kernel_version),
    "//kernel_device_modules-{}/drivers/interconnect/mediatek:mtk-emi".format(kernel_version),
    "//kernel_device_modules-{}/drivers/interconnect/mediatek:mtk-emibus-icc".format(kernel_version),
    "//kernel_device_modules-{}/drivers/leds:leds-mtk".format(kernel_version),
    "//kernel_device_modules-{}/drivers/leds:regulator-vibrator".format(kernel_version),
    "//kernel_device_modules-{}/drivers/nvmem:nvmem-mt635x-efuse".format(kernel_version),
    "//kernel_device_modules-{}/drivers/nvmem:nvmem_mtk-devinfo".format(kernel_version),
    "//kernel_device_modules-{}/drivers/memory/mediatek:emi-mpu".format(kernel_version),
    "//kernel_device_modules-{}/drivers/memory/mediatek:emi-slb".format(kernel_version),
    "//kernel_device_modules-{}/drivers/memory/mediatek:slc-parity".format(kernel_version),
    "//kernel_device_modules-{}/drivers/mfd:mt6360-core".format(kernel_version),
    "//kernel_device_modules-{}/drivers/mfd:mt6370".format(kernel_version),
    "//kernel_device_modules-{}/drivers/mfd:mt6375".format(kernel_version),
    "//kernel_device_modules-{}/drivers/mfd:mt6379s".format(kernel_version),
    "//kernel_device_modules-{}/drivers/mfd:mt6397".format(kernel_version),
    "//kernel_device_modules-{}/drivers/mfd:mt63xx-debug".format(kernel_version),
    "//kernel_device_modules-{}/drivers/mfd:mtk-spmi-pmic".format(kernel_version),
    "//kernel_device_modules-{}/drivers/mfd:mtk-spmi-pmic-debug".format(kernel_version),
    "//kernel_device_modules-{}/drivers/mfd:mt6685-core".format(kernel_version),
    "//kernel_device_modules-{}/drivers/mfd:mt6687-core".format(kernel_version),
    "//kernel_device_modules-{}/drivers/mfd:rt9490".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/atf:atf_logger".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/blocktag:blocktag".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/btif:btif_drv".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/ccmni:ccmni".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/ccu/src/isp6s:ccu_isp6s".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/cpufreq_lite:cpudvfs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/dvfsrc:mtk-dvfsrc-helper".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/geniezone:gz-main".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/geniezone/gz-trusty:gz-trusty".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/ips:mtk-ips-helper".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/log_store:log_store".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mdpm:mtk_mdpm".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/monitor_hang:monitor_hang".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mtprintk:mtk_printk_ctrl".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/pbm:mtk_pbm".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/pcie:mtk_pcie_smt".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/pidmap:pidmap".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/power_throttling:mtk_battery_oc_throttling".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/power_throttling:mtk_bp_thl".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/power_throttling:mtk_cpu_power_throttling".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/power_throttling:mtk_dynamic_loading_throttling".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/power_throttling:mtk_low_battery_throttling".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/power_throttling:pmic_dual_lbat_service".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/power_throttling:pmic_lbat_service".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/power_throttling:pmic_lvsys_notify".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/rps:rps_perf".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/sda:cache-parity".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/sda:dbg_error_flag".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/slbc:mtk_slbc".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/slbc:slbc_ipi".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/slbc:slbc_trace".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/smap:smap-mt6991".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/subpmic:extdev_io_class".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/subpmic:mt6360-dbg".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/subpmic:mt6370-dbg".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/subpmic:subpmic-dbg".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/sensor/2.0/core:hf_manager".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/trusted_mem:ffa".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/aee/aed:aee_aed".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/aee/aed:aee_rs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/iommu:iommu_debug".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/iommu:iommu_engine".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/iommu:iommu_secure".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/iommu:iommu_test".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/iommu:mtk_iommu_util".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/iommu:mtk_smmu_qos".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/iommu:smmu_secure".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/irtx:mtk_irtx_pwm".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mmdebug:mtk-mmdebug-vcp-stub".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mmdvfs:mtk-mmdvfs-debug".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mmdvfs:mtk-mmdvfs-ftrace".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/pmsr:pmsr".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/pmsr/v2:pmsr_v2".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/pwm:mtk-pwm".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/typec/tcpc:pd_dbg_info".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/typec/tcpc:rt_pd_manager".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/typec/tcpc:tcpci_late_sync".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/typec/tcpc:tcpc_class".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/typec/tcpc:tcpc_mt6360".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/typec/tcpc:tcpc_mt6370".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/typec/tcpc:tcpc_mt6375".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/typec/tcpc:tcpc_mt6379".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/typec/tcpc:tcpc_rt1711h".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/typec/mux:fusb304".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/typec/mux:mux_switch".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/typec/mux:ps5169".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/typec/mux:ps5170".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/typec/mux:ptn36241g".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/usb/usb_meta:usb_meta".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/usb/usb_rndis:mtk_u_ether".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/usb/usb_rndis:mtk_usb_f_rndis".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/usb/usb_xhci:xhci-mtk-hcd-v2".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/usb/usb20:musb_hdrc".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/usb/usb20/musb_main:musb_main".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/swpm:mtk-swpm-perf-arm-pmu".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug/v1:mtk-swpm-isp-wrapper".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/wla:wla-v1".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/wla:wla-v1-dbg".format(kernel_version),
    "//kernel_device_modules-{}/drivers/phy/mediatek:phy-mtk-fpgaphy".format(kernel_version),
    "//kernel_device_modules-{}/drivers/memory/mediatek:mtk_dramc".format(kernel_version),
    "//kernel_device_modules-{}/drivers/phy/mediatek:phy-mtk-mt6379-eusb2-repeater".format(kernel_version),
    "//kernel_device_modules-{}/drivers/phy/mediatek:phy-mtk-nxp-eusb2-repeater".format(kernel_version),
    "//kernel_device_modules-{}/drivers/phy/mediatek:phy-mtk-pcie".format(kernel_version),
    "//kernel_device_modules-{}/drivers/phy/mediatek:phy-mtk-tphy".format(kernel_version),
    "//kernel_device_modules-{}/drivers/phy/mediatek:phy-mtk-ufs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/phy/mediatek:phy-mtk-xsphy".format(kernel_version),
    "//kernel_device_modules-{}/drivers/pinctrl/mediatek:pinctrl-mtk-v2".format(kernel_version),
    "//kernel_device_modules-{}/drivers/pinctrl/mediatek:pinctrl-mtk-common-v2_debug".format(kernel_version),
    "//kernel_device_modules-{}/drivers/pinctrl/mediatek:pinctrl-mt6363".format(kernel_version),
    "//kernel_device_modules-{}/drivers/pinctrl/mediatek:pinctrl-mt6373".format(kernel_version),
    "//kernel_device_modules-{}/drivers/pinctrl/mediatek:pinctrl-mt6661".format(kernel_version),
    "//kernel_device_modules-{}/drivers/pinctrl/mediatek:pinctrl-mt6667".format(kernel_version),
    "//kernel_device_modules-{}/drivers/power/supply/ufcs:ufcs_class".format(kernel_version),
    "//kernel_device_modules-{}/drivers/power/supply/ufcs:ufcs_mt6379".format(kernel_version),
    "//kernel_device_modules-{}/drivers/power/supply:adapter_class".format(kernel_version),
    "//kernel_device_modules-{}/drivers/power/supply:charger_class".format(kernel_version),
    "//kernel_device_modules-{}/drivers/power/supply:mt6357_battery".format(kernel_version),
    "//kernel_device_modules-{}/drivers/power/supply:mt6358_battery".format(kernel_version),
    "//kernel_device_modules-{}/drivers/power/supply:mt6359p_battery".format(kernel_version),
    "//kernel_device_modules-{}/drivers/power/supply:mt6360_charger".format(kernel_version),
    "//kernel_device_modules-{}/drivers/power/supply:mt6360_pmu_chg".format(kernel_version),
    "//kernel_device_modules-{}/drivers/power/supply:mt6370-charger".format(kernel_version),
    "//kernel_device_modules-{}/drivers/power/supply:mt6375-battery".format(kernel_version),
    "//kernel_device_modules-{}/drivers/power/supply:mt6375-charger".format(kernel_version),
    "//kernel_device_modules-{}/drivers/power/supply:mt6379-battery".format(kernel_version),
    "//kernel_device_modules-{}/drivers/power/supply:mt6379-chg".format(kernel_version),
    "//kernel_device_modules-{}/drivers/power/supply:mtk_battery_manager".format(kernel_version),
    "//kernel_device_modules-{}/drivers/power/supply:mtk_charger_algorithm_class".format(kernel_version),
    "//kernel_device_modules-{}/drivers/power/supply:mtk_charger_framework".format(kernel_version),
    "//kernel_device_modules-{}/drivers/power/supply:mtk_chg_type_det".format(kernel_version),
    "//kernel_device_modules-{}/drivers/power/supply:mtk_pd_adapter".format(kernel_version),
    "//kernel_device_modules-{}/drivers/power/supply:mtk_pd_charging".format(kernel_version),
    "//kernel_device_modules-{}/drivers/power/supply:mtk_ufcs_adapter".format(kernel_version),
    "//kernel_device_modules-{}/drivers/power/supply:rt9490-charger".format(kernel_version),
    "//kernel_device_modules-{}/drivers/power/supply:rt9758-charger".format(kernel_version),
    "//kernel_device_modules-{}/drivers/power/supply:rt9759".format(kernel_version),
    "//kernel_device_modules-{}/drivers/regulator:mt6315-regulator".format(kernel_version),
    "//kernel_device_modules-{}/drivers/regulator:mt6316-regulator".format(kernel_version),
    "//kernel_device_modules-{}/drivers/regulator:mt6359p-regulator".format(kernel_version),
    "//kernel_device_modules-{}/drivers/regulator:mt6360-regulator".format(kernel_version),
    "//kernel_device_modules-{}/drivers/regulator:mt6363-regulator".format(kernel_version),
    "//kernel_device_modules-{}/drivers/regulator:mt6368-regulator".format(kernel_version),
    "//kernel_device_modules-{}/drivers/regulator:mt6369-regulator".format(kernel_version),
    "//kernel_device_modules-{}/drivers/regulator:mt6370-regulator".format(kernel_version),
    "//kernel_device_modules-{}/drivers/regulator:mt6373-regulator".format(kernel_version),
    "//kernel_device_modules-{}/drivers/regulator:mt6379-regulator".format(kernel_version),
    "//kernel_device_modules-{}/drivers/regulator:mtk-dvfsrc-regulator".format(kernel_version),
    "//kernel_device_modules-{}/drivers/regulator:mtk-extbuck-debug".format(kernel_version),
    "//kernel_device_modules-{}/drivers/regulator:rt4803".format(kernel_version),
    "//kernel_device_modules-{}/drivers/regulator:rt5133-regulator".format(kernel_version),
    "//kernel_device_modules-{}/drivers/regulator:rt6160-regulator".format(kernel_version),
    "//kernel_device_modules-{}/drivers/remoteproc:mtk_ccu_mssv".format(kernel_version),
    "//kernel_device_modules-{}/drivers/reset:reset-ti-syscon".format(kernel_version),
    "//kernel_device_modules-{}/drivers/rpmsg:mtk_rpmsg_mbox".format(kernel_version),
    "//kernel_device_modules-{}/drivers/soc/mediatek:mtk-dvfsrc".format(kernel_version),
    "//kernel_device_modules-{}/drivers/soc/mediatek:mtk-dvfsrc-start".format(kernel_version),
    "//kernel_device_modules-{}/drivers/soc/mediatek:mtk-pm-domain-disable-unused".format(kernel_version),
    "//kernel_device_modules-{}/drivers/soc/mediatek:mtk-mbox".format(kernel_version),
    "//kernel_device_modules-{}/drivers/soc/mediatek:mtk_tinysys_ipi".format(kernel_version),
    "//kernel_device_modules-{}/drivers/thermal/mediatek:backlight_cooling".format(kernel_version),
    "//kernel_device_modules-{}/drivers/thermal/mediatek:board_temp".format(kernel_version),
    "//kernel_device_modules-{}/drivers/thermal/mediatek:charger_cooling".format(kernel_version),
    "//kernel_device_modules-{}/drivers/thermal/mediatek:pmic_temp".format(kernel_version),
    "//kernel_device_modules-{}/drivers/trusty:trusty-core".format(kernel_version),
    "//kernel_device_modules-{}/drivers/trusty:trusty-ipc".format(kernel_version),
    "//kernel_device_modules-{}/drivers/trusty:trusty-log".format(kernel_version),
    "//kernel_device_modules-{}/drivers/trusty:trusty-test".format(kernel_version),
    "//kernel_device_modules-{}/drivers/trusty:trusty-virtio".format(kernel_version),
    "//kernel_device_modules-{}/drivers/usb/mtu3:mtu3".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/performance:mtk_ioctl_touch_boost".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/performance:mtk_ioctl_powerhal".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mdp:mdp_drv_dummy".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/cameraisp/src/isp_6s:cam_qos".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/performance:mtk_perf_ioctl_magt".format(kernel_version),
    "//kernel_device_modules-{}/drivers/watchdog:mtk_wdt".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mtprof:bootprof".format(kernel_version),
    "//kernel_device_modules-{}/sound/soc/mediatek/common:mtk-btcvsd".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mminfra:mm-fake-engine".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/tinysys_scmi:tinysys-scmi".format(kernel_version),
    "//kernel_device_modules-{}/drivers/mailbox:mtk-ise-mailbox".format(kernel_version),
    "//kernel_device_modules-{}/drivers/mailbox:mtk-mbox-mailbox".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/lpm:mtk-lpm".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/lpm/modules/debug:mtk-lpm-dbg-common-v2".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/lpm/modules/platform/v2:mtk-lpm-plat-v2".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mkp:mkp".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mmstat:trace_mmstat".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mtk-interconnect:mtk-icc-core".format(kernel_version),
]

mgk_64_kleaf_platform_modules = {
    # keep sorted
    "//kernel_device_modules-{}/drivers/misc/mediatek/lpm/modules/debug/mt6991:mtk-lpm-dbg-mt6991".format(kernel_version): "mt6991",
    "//kernel_device_modules-{}/drivers/misc/mediatek/lpm/modules/debug/v6993:mtk-lpm-dbg-v6993".format(kernel_version): "v6993",
    "//kernel_device_modules-{}/drivers/pinctrl/mediatek:pinctrl-mt6789".format(kernel_version): "mt6789",
    "//kernel_device_modules-{}/drivers/pinctrl/mediatek:pinctrl-mt6886".format(kernel_version): "mt6886",
    "//kernel_device_modules-{}/drivers/pinctrl/mediatek:pinctrl-mt6897".format(kernel_version): "mt6897",
    "//kernel_device_modules-{}/drivers/pinctrl/mediatek:pinctrl-mt6899".format(kernel_version): "mt6899",
    "//kernel_device_modules-{}/drivers/pinctrl/mediatek:pinctrl-mt6983".format(kernel_version): "mt6983",
    "//kernel_device_modules-{}/drivers/pinctrl/mediatek:pinctrl-mt6985".format(kernel_version): "mt6985",
    "//kernel_device_modules-{}/drivers/pinctrl/mediatek:pinctrl-mt6989".format(kernel_version): "mt6989",
    "//kernel_device_modules-{}/drivers/pinctrl/mediatek:pinctrl-mt6991".format(kernel_version): "mt6991",
    "//kernel_device_modules-{}/drivers/pinctrl/mediatek:pinctrl-mt6993".format(kernel_version): "mt6993",

    ## write vendor file by platform here
    "//vendor/mediatek/kernel_modules/mtkcam/camsys:mtk-cam-plat-mt6983":"mt6983",
    "//vendor/mediatek/kernel_modules/mtkcam/camsys:mtk-cam-plat-mt6895":"mt6895",
    "//vendor/mediatek/kernel_modules/mtkcam/camsys:mtk-cam-plat-mt6879":"mt6879",
}

mgk_64_kleaf_eng_device_modules = [
    # keep sorted
    "//kernel_device_modules-{}/drivers/misc/mediatek/cpufreq_v1:cpuhvfs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/selinux_warning:mtk_selinux_aee_warning".format(kernel_version),
]

mgk_64_kleaf_platform_eng_modules = {
    # keep sorted

    ## write vendor file by platform here

}

mgk_64_kleaf_userdebug_device_modules = [
    # keep sorted
    "//kernel_device_modules-{}/drivers/misc/mediatek/cpufreq_v1:cpuhvfs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/selinux_warning:mtk_selinux_aee_warning".format(kernel_version),
]

mgk_64_kleaf_platform_userdebug_modules = {
    # keep sorted

    ## write vendor file by platform here

}

mgk_64_kleaf_user_device_modules = [
    # keep sorted
]

mgk_64_kleaf_platform_user_modules = {
    # keep sorted

    ## write vendor file by platform here

}

mgk_64_device_modules = [
    # keep sorted
    "drivers/char/hw_random/sec-rng.ko",
    "drivers/char/rpmb/rpmb-mtk.ko",
    "drivers/clk/mediatek/clk-common.ko",
    #"drivers/clk/mediatek/clk-common-dummy.ko",
    "drivers/clk/mediatek/fhctl.ko",
    #"drivers/clocksource/timer-mediatek.ko",
    "drivers/devfreq/mtk-dvfsrc-devfreq.ko",
    "drivers/dma-buf/heaps/mtk_heap_debug.ko",
    "drivers/dma-buf/heaps/mtk_sec_heap.ko",
    #"drivers/misc/mediatek/pkvm_smmu/pkvm_smmu.ko",
    "drivers/dma/mediatek/mtk-cqdma.ko",
    "drivers/dma/mediatek/mtk-uart-apdma.ko",
    "drivers/gpu/drm/mediatek/dpc/dpc_v1/mtk_dpc_v1.ko",
    "drivers/gpu/drm/mediatek/dpc/dpc_v1/mtk_vdisp_v1.ko",
    "drivers/gpu/drm/mediatek/dpc/dpc_v2/mtk_dpc_v2.ko",
    "drivers/gpu/drm/mediatek/dpc/dpc_v2/mtk_vdisp_v2.ko",
    "drivers/gpu/drm/mediatek/dpc/dpc_v3/mtk_dpc_v3.ko",
    "drivers/gpu/drm/mediatek/dpc/dpc_v3/mtk_vdisp_v3.ko",
    "drivers/gpu/drm/mediatek/mediatek_v2/mediatek-drm.ko",
    "drivers/gpu/drm/mediatek/mediatek_v2/mtk_aod_scp.ko",
    "drivers/gpu/drm/mediatek/mediatek_v2/mtk_disp_sec.ko",
    "drivers/gpu/drm/mediatek/mml/mtk-mml.ko",
    "drivers/gpu/drm/panel/mediatek-cust-panel-sample.ko",
    "drivers/gpu/drm/panel/mediatek-drm-gateic.ko",
    "drivers/gpu/drm/panel/mediatek-drm-panel-drv.ko",
    "drivers/gpu/drm/panel/panel-alpha-jdi-nt36672e-vdo-120hz-hfp.ko",
    "drivers/gpu/drm/panel/panel-alpha-jdi-nt36672e-vdo-120hz.ko",
    "drivers/gpu/drm/panel/panel-alpha-jdi-nt36672e-vdo-120hz-threshold.ko",
    "drivers/gpu/drm/panel/panel-alpha-jdi-nt36672e-vdo-144hz-hfp.ko",
    "drivers/gpu/drm/panel/panel-alpha-jdi-nt36672e-vdo-60hz.ko",
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
    "drivers/gpu/drm/panel/panel-tianma-nt36672e-vdo-120hz-vfp.ko",
    "drivers/gpu/drm/panel/panel-truly-ft8756-vdo.ko",
    "drivers/gpu/drm/panel/panel-nt36672a-rt4801-vdo.ko",
    "drivers/gpu/drm/panel/panel-truly-td4330-cmd.ko",
    "drivers/gpu/drm/panel/panel-boe-ts127qfmll1dkp0.ko",
    "drivers/gpu/drm/panel/ocp2138_i2c.ko",
    "drivers/gpu/drm/panel/panel-tianma-nt36672e-vdo-120hz-vfp-6382.ko",
    "drivers/gpu/mediatek/ged/ged.ko",
    "drivers/gpu/mediatek/gpu_bm/mtk_gpu_qos.ko",
    "drivers/gpu/mediatek/gpueb/mtk_gpueb.ko",
    "drivers/gpu/mediatek/gpueb/mtk_ghpm.ko",
    "drivers/gpu/mediatek/gpueb/mtk_ghpm_swwa.ko",
    "drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_wrapper.ko",
    "drivers/i2c/busses/i2c-mt65xx.ko",
    "drivers/i3c/master/mtk-i3c-master-mt69xx.ko",
    "drivers/iio/adc/mt6338-auxadc.ko",
    "drivers/iio/adc/mt6577_auxadc.ko",
    "drivers/iio/adc/mt6681-auxadc.ko",
    "drivers/input/touchscreen/BoTai_Multi_Touch/BoTai_touch_one/botai_touch_one.ko",
    "drivers/input/touchscreen/BoTai_Multi_Touch/BoTai_touch_two/botai_touch_two.ko",
    #"drivers/input/touchscreen/GT9886/gt9886.ko",
    "drivers/input/touchscreen/GT9895/gt9895.ko",
    "drivers/input/touchscreen/ts_scp/ts_scp_common.ko",
    "drivers/input/touchscreen/GT9896S/gt9896s.ko",
    #"drivers/input/touchscreen/GT1151/gt1151.ko",
    "drivers/input/touchscreen/NT36532/nt36532.ko",
    "drivers/input/touchscreen/GT9966/gt9966.ko",
    "drivers/input/touchscreen/gt9xx/gt9xx_touch.ko",
    "drivers/input/touchscreen/ILITEK/ilitek_i2c.ko",
    #"drivers/input/touchscreen/NT36672C_I2C/nt36672c_i2c.ko",
    #"drivers/input/touchscreen/tui-common.ko",
    "drivers/interconnect/mediatek/mmqos-common.ko",
    "drivers/iommu/arm/arm-smmu-v3/arm_smmu_v3.ko",
    "drivers/iommu/arm/arm-smmu-v3/mtk-smmuv3-pmu.ko",
    "drivers/iommu/mtk_iommu.ko",
    "drivers/leds/leds-mt6360.ko",
    "drivers/leds/leds-mtk-disp.ko",
    "drivers/leds/leds-mtk-pwm.ko",
    "drivers/leds/flash/leds-mt6370-flash.ko",
    "drivers/leds/flash/leds-mt6379.ko",
    "drivers/media/platform/mtk-jpeg/mtk_jpeg.ko",
    "drivers/media/platform/mtk-vcodec/mtk-vcodec-common.ko",
    "drivers/media/platform/mtk-vcodec/mtk-vcodec-dec-v1.ko",
    "drivers/media/platform/mtk-vcodec/mtk-vcodec-dec-v2.ko",
    "drivers/media/platform/mtk-vcodec/mtk-vcodec-enc-v1.ko",
    "drivers/media/platform/mtk-vcodec/mtk-vcodec-enc-v2.ko",
    "drivers/media/platform/mtk-vcu/mtk-vcu.ko",
    "drivers/memory/mediatek/emi.ko",
    "drivers/memory/mediatek/emi-fake-eng.ko",
    "drivers/memory/mediatek/emi-mpu-test.ko",
    "drivers/memory/mediatek/emi-mpu-test-v2.ko",
    "drivers/memory/mediatek/smpu.ko",
    "drivers/memory/mediatek/smpu-hook-v1.ko",
    "drivers/memory/mtk-smi.ko",
    "drivers/mfd/mt6338-core.ko",
    "drivers/mfd/mt6681-core.ko",
    "drivers/mfd/mt6685-audclk.ko",
    "drivers/misc/mediatek/adsp/adsp.ko",
    "drivers/misc/mediatek/adsp/v1/adsp-v1.ko",
    "drivers/misc/mediatek/adsp/v2/adsp-v2.ko",
    "drivers/misc/mediatek/adsp/v3/adsp-v3.ko",
    "drivers/misc/mediatek/aee/hangdet/aee_hangdet.ko",
    "drivers/misc/mediatek/apusys/apusys.ko",
    "drivers/misc/mediatek/apusys/apu_aov.ko",
    "drivers/misc/mediatek/apusys/power/apu_top.ko",
    "drivers/misc/mediatek/apusys/sapu/sapu.ko",
    "drivers/misc/mediatek/audio_ipi/audio_ipi.ko",
    "drivers/misc/mediatek/cache-auditor/cpuqos_v3/cpuqos_v3.ko",
    "drivers/misc/mediatek/cameraisp/dpe/camera_dpe_isp70.ko",
    "drivers/misc/mediatek/cameraisp/pda/isp_71/camera_pda.ko",
    "drivers/misc/mediatek/cameraisp/src/isp_6s/camera_isp.ko",
    "drivers/misc/mediatek/camera_mem/camera_mem.ko",
    "drivers/misc/mediatek/cam_timesync/archcounter_timesync.ko",
    "drivers/misc/mediatek/ccci_util/ccci_util_lib.ko",
    "drivers/misc/mediatek/ccu/src/isp4/ccu_isp4.ko",
    "drivers/misc/mediatek/hwccf/hwccf.ko",
    "drivers/misc/mediatek/clkbuf/clkbuf.ko",
    "drivers/misc/mediatek/cmdq/mailbox/cmdq-sec-drv.ko",
    "drivers/misc/mediatek/cmdq/mailbox/cmdq-test.ko",
    "drivers/misc/mediatek/cmdq/mailbox/mtk-cmdq-drv-ext.ko",
    "drivers/misc/mediatek/cm_mgr/mtk_cm_ipi.ko",
    "drivers/misc/mediatek/cm_mgr/mtk_cm_mgr.ko",
    "drivers/misc/mediatek/connectivity/connadp.ko",
    "drivers/misc/mediatek/conn_scp/connscp.ko",
    "drivers/misc/mediatek/cci_lite/ccidvfs.ko",
    "drivers/misc/mediatek/cross_sched/cross_sched.ko",
    "drivers/misc/mediatek/dcm/mtk_dcm.ko",
    #"drivers/misc/mediatek/dvfsrc/mtk-dvfsrc-helper.ko",
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
    "drivers/misc/mediatek/flashlight/v4l2/lm3643.ko",
    "drivers/misc/mediatek/flashlight/v4l2/lm3644.ko",
    "drivers/misc/mediatek/geniezone/gz_main_mod.ko",
    "drivers/misc/mediatek/hw_sem/mtk-hw-semaphore.ko",
    "drivers/misc/mediatek/i3c_i2c_wrap/mtk-i3c-i2c-wrap.ko",
    "drivers/misc/mediatek/imgsensor/src/isp6s/imgsensor_isp6s.ko",
    "drivers/misc/mediatek/cam_cal/src/custom/camera_eeprom.ko",
    "drivers/misc/mediatek/ise_lpm/ise_lpm.ko",
    "drivers/misc/mediatek/ise_lpm/ise_lpm_v2.ko",
    "drivers/misc/mediatek/ise_trusty/ise-trusty.ko",
    "drivers/misc/mediatek/ise_trusty/ise-trusty-ipc.ko",
    "drivers/misc/mediatek/ise_trusty/ise-trusty-log.ko",
    "drivers/misc/mediatek/ise_trusty/ise-trusty-virtio.ko",
    "drivers/misc/mediatek/jpeg/jpeg-driver.ko",
    "drivers/misc/mediatek/ktchbst/ktchbst.ko",
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
    "drivers/misc/mediatek/lens/tof/stmvl53l4.ko",
    "drivers/misc/mediatek/lens/vcm/v4l2/media/camera_af_media.ko",
    "drivers/misc/mediatek/lens/vcm/v4l2/sub_vcm/sub_vcm.ko",
    "drivers/misc/mediatek/lpm/governors/MHSP/lpm-gov-MHSP.ko",
    #"drivers/misc/mediatek/lpm/modules/debug/v1/mtk-lpm-dbg-common-v1.ko",
    #"drivers/misc/mediatek/lpm/modules/platform/v1/mtk-lpm-plat-v1.ko",
    "drivers/misc/mediatek/masp/sec.ko",
    "drivers/misc/mediatek/mbraink/bridge/mtk_mbraink_bridge.ko",
    "drivers/misc/mediatek/mbraink/mtk_mbraink.ko",
    "drivers/misc/mediatek/mbraink/modules/v6989/mtk_mbraink_v6989.ko",
    "drivers/misc/mediatek/mbraink/modules/v6991/mtk_mbraink_v6991.ko",
    "drivers/misc/mediatek/mbraink/modules/v6899/mtk_mbraink_v6899.ko",
    "drivers/misc/mediatek/mcupm/v2/mcupm.ko",
    "drivers/misc/mediatek/mddp/mddp.ko",
    "drivers/misc/mediatek/mdp/cmdq_helper_inf.ko",
    #"drivers/misc/mediatek/mdp/mdp_drv_dummy.ko",
    "drivers/misc/mediatek/mmdebug/mtk-mmdebug-vcp.ko",
    "drivers/misc/mediatek/memory-amms/memory-amms.ko",
    "drivers/misc/mediatek/mmdvfs/mtk-mmdvfs-ccu.ko",
    "drivers/misc/mediatek/mmdvfs/mtk-mmdvfs-debug-v3.ko",
    "drivers/misc/mediatek/mmdvfs/mtk-mmdvfs-debug-v5.ko",
    "drivers/misc/mediatek/mmdvfs/mtk-mmdvfs-v3-start.ko",
    "drivers/misc/mediatek/mminfra/mtk-mminfra-util.ko",
    "drivers/misc/mediatek/mminfra/mtk-mminfra-debug.ko",
    "drivers/misc/mediatek/mminfra/mtk-mminfra-imax.ko",
    "drivers/misc/mediatek/mmp/src/mmprofile.ko",
    "drivers/misc/mediatek/mme/src/mme.ko",
    "drivers/misc/mediatek/mmqos/mmqos_wrapper.ko",
    "drivers/misc/mediatek/pbm/mtk_peak_power_budget.ko",
    "drivers/misc/mediatek/cg_ppt/mtk_cg_peak_power_throttling.ko",
    "drivers/misc/mediatek/perf_common/mtk_perf_common.ko",
    "drivers/misc/mediatek/performance/fpsgo_v3/mtk_fpsgo.ko",
    "drivers/misc/mediatek/performance/frs/frs.ko",
    "drivers/misc/mediatek/performance/load_track/load_track.ko",
    "drivers/misc/mediatek/performance/mtk_perf_ioctl.ko",
    "drivers/misc/mediatek/performance/powerhal_cpu_ctrl/powerhal_cpu_ctrl.ko",
    "drivers/misc/mediatek/performance/touch_boost/touch_boost.ko",
    "drivers/misc/mediatek/performance/uload_ind/uload_ind.ko",
    #"drivers/misc/mediatek/pkvm_mgmt/pkvm_mgmt.ko",
    #"drivers/misc/mediatek/pkvm_tmem/pkvm_tmem.ko",
    #"drivers/misc/mediatek/pkvm_mkp/pkvm_mkp.ko",
    "drivers/misc/mediatek/pmic_protect/mtk-pmic-oc-debug.ko",
    "drivers/misc/mediatek/pmsr/v3/pmsr_v3.ko",
    "drivers/misc/mediatek/power_throttling/mtk_gpu_power_throttling.ko",
    "drivers/misc/mediatek/power_throttling/mtk_md_power_throttling.ko",
    "drivers/misc/mediatek/qos/mtk_qos.ko",
    "drivers/misc/mediatek/sched/cpufreq_sugov_ext.ko",
    "drivers/misc/mediatek/sched/mtk_core_ctl.ko",
    "drivers/misc/mediatek/sched/scheduler.ko",
    "drivers/misc/mediatek/scp/rv/scp.ko",
    "drivers/misc/mediatek/sda/btm/bus_tracer_interface.ko",
    "drivers/misc/mediatek/sda/btm/v1/bus_tracer_v1.ko",
    "drivers/misc/mediatek/sda/bus-parity.ko",
    "drivers/misc/mediatek/sda/gic-ram-parity.ko",
    "drivers/misc/mediatek/sda/dbgtop-drm.ko",
    "drivers/misc/mediatek/sda/irq-dbg.ko",
    "drivers/misc/mediatek/sda/last_bus.ko",
    "drivers/misc/mediatek/sda/systracker.ko",
    "drivers/misc/mediatek/sensor/2.0/sensorhub/sensorhub.ko",
    "drivers/misc/mediatek/slbc/mmsram.ko",
    "drivers/misc/mediatek/smi/mtk-smi-dbg.ko",
    "drivers/misc/mediatek/ssc/debug/v1/mtk-ssc-dbg-v1.ko",
    "drivers/misc/mediatek/ssc/debug/v2/mtk-ssc-dbg-v2.ko",
    "drivers/misc/mediatek/ssc/mtk-ssc.ko",
    "drivers/misc/mediatek/sspm/v3/sspm_v3.ko",
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
    "drivers/misc/mediatek/swpm/modules/debug/v6991/mtk-swpm-audio-dbg-v6991.ko",
    "drivers/misc/mediatek/swpm/modules/debug/v6991/mtk-swpm-dbg-v6991.ko",
    "drivers/misc/mediatek/swpm/modules/debug/v6991/mtk-swpm-cpu-dbg-v6991.ko",
    "drivers/misc/mediatek/swpm/modules/debug/v6991/mtk-swpm-disp-dbg-v6991.ko",
    "drivers/misc/mediatek/swpm/modules/debug/v6991/mtk-swpm-mml-dbg-v6991.ko",
    "drivers/misc/mediatek/swpm/modules/debug/v6991/mtk-swpm-isp-dbg-v6991.ko",
    "drivers/misc/mediatek/swpm/mtk-swpm.ko",
    "drivers/misc/mediatek/task_turbo/task_turbo.ko",
    "drivers/misc/mediatek/trusted_mem/tmem_ffa.ko",
    "drivers/misc/mediatek/trusted_mem/trusted_mem.ko",
    "drivers/misc/mediatek/typec/mux/usb_dp_selector.ko",
    "drivers/misc/mediatek/uarthub/uarthub_drv.ko",
    "drivers/misc/mediatek/usb/c2k_usb/c2k_usb.ko",
    "drivers/misc/mediatek/usb/c2k_usb/c2k_usb_f_via_atc.ko",
    "drivers/misc/mediatek/usb/c2k_usb/c2k_usb_f_via_ets.ko",
    "drivers/misc/mediatek/usb/c2k_usb/c2k_usb_f_via_modem.ko",
    "drivers/misc/mediatek/usb/usb_boost/usb_boost.ko",
    "drivers/misc/mediatek/usb/usb_boost/musb_boost.ko",
    "drivers/misc/mediatek/usb/usb_offload/usb_offload.ko",
    "drivers/misc/mediatek/usb/usb_logger/usb_logger.ko",
    "drivers/misc/mediatek/usb/usb_sram/usb_sram.ko",
    "drivers/misc/mediatek/vcp/rv/vcp.ko",
    "drivers/misc/mediatek/vcp/rv/vcp_status.ko",
    "drivers/misc/mediatek/vcp/rv_v2/vcp_v2.ko",
    "drivers/misc/mediatek/vcp/rv_v2/vcp_status_v2.ko",
    "drivers/misc/mediatek/vdec_fmt/vdec-fmt.ko",
    "drivers/misc/mediatek/vmm/mtk-vmm-notifier.ko",
    "drivers/misc/mediatek/vmm_spm/mtk-vmm-spm.ko",
    "drivers/misc/mediatek/vow/ver02/mtk-vow.ko",
    "drivers/misc/mediatek/widevine_drm/widevine_driver.ko",
    "drivers/mmc/host/cqhci.ko",
    "drivers/mmc/host/mtk-mmc-dbg.ko",
    "drivers/mmc/host/mtk-mmc.ko",
    "drivers/mmc/host/mtk-sd.ko",
    #"drivers/mmc/host/mtk-mmc-wp.ko",
    "drivers/net/ethernet/stmicro/stmmac/dwmac-mediatek.ko",
    "drivers/net/ethernet/stmicro/stmmac/mtk_sgmii_pwr.ko",
    "drivers/net/ethernet/stmicro/stmmac/stmmac-platform.ko",
    "drivers/net/ethernet/stmicro/stmmac/stmmac.ko",
    "drivers/net/phy/mxl-gpy.ko",
    "drivers/net/phy/realtek.ko",
    "drivers/nvmem/nvmem-mt6338-efuse.ko",
    "drivers/nvmem/nvmem-mt6681-efuse.ko",
    "drivers/pci/controller/pcie-mediatek-gen3.ko",
    "drivers/power/supply/mtk_2p_charger.ko",
    "drivers/power/supply/mtk_hvbpc.ko",
    "drivers/power/supply/mtk_pep.ko",
    "drivers/power/supply/mtk_pep20.ko",
    "drivers/power/supply/mtk_pep40.ko",
    "drivers/power/supply/mtk_pep45.ko",
    "drivers/power/supply/mtk_pep50.ko",
    "drivers/power/supply/mtk_pep50p.ko",
    "drivers/pwm/pwm-mtk-disp.ko",
    "drivers/regulator/mt6681-regulator.ko",
    "drivers/regulator/mtk-vmm-isp71-regulator.ko",
    "drivers/remoteproc/mtk_ccu.ko",
    "drivers/rtc/rtc-mt6397.ko",
    "drivers/rtc/rtc-mt6685.ko",
    "drivers/ufs/ufs-mediatek-dbg.ko",
    "drivers/ufs/vendor/ufs-mediatek-mod.ko",
    "drivers/ufs/vendor/ufs-mediatek-mod-ise.ko",
    "drivers/soc/mediatek/devapc/device-apc-common.ko",
    "drivers/soc/mediatek/devapc/device-apc-common-legacy.ko",
    "drivers/soc/mediatek/mtk-mmdvfs.ko",
    "drivers/soc/mediatek/mtk-mmdvfs-v3.ko",
    "drivers/soc/mediatek/mmdvfs/mtk-mmdvfs-v5.ko",
    "drivers/soc/mediatek/mtk-mmsys.ko",
    "drivers/soc/mediatek/mtk-mutex.ko",
    "drivers/soc/mediatek/mtk-pmic-wrap.ko",
    "drivers/soc/mediatek/mtk-scpsys.ko",
    "drivers/soc/mediatek/scpsys-dummy.ko",
    "drivers/soc/mediatek/mtk-socinfo.ko",
    "drivers/spi/spi-mt65xx.ko",
    "drivers/spmi/spmi-mtk-mpu.ko",
    "drivers/spmi/spmi-mtk-pmif.ko",
    "drivers/tee/gud/700/MobiCoreDriver/mcDrvModule.ko",
    "drivers/tee/gud/700/MobiCoreDriver/mcDrvModule-ffa.ko",
    "drivers/tee/gud/700/TlcTui/t-base-tui.ko",
    "drivers/tee/teei/510/isee.ko",
    "drivers/tee/teei/510/isee-ffa.ko",
    "drivers/tee/teeperf/teeperf.ko",
    "drivers/thermal/mediatek/md_cooling_all.ko",
    "drivers/thermal/mediatek/soc_temp_lvts.ko",
    "drivers/thermal/mediatek/thermal_interface.ko",
    "drivers/thermal/mediatek/thermal_trace.ko",
    "drivers/thermal/mediatek/vtskin_temp.ko",
    "drivers/thermal/mediatek/wifi_cooling.ko",
    "drivers/tty/serial/8250/8250_mtk.ko",
    "sound/soc/codecs/mt6338-accdet.ko",
    "sound/soc/codecs/mt6357-accdet.ko",
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
    "sound/soc/mediatek/common/mtk-sp-spk-amp.ko",
    "sound/soc/mediatek/common/snd-soc-mtk-common.ko",
    "sound/soc/mediatek/ultrasound/ultra_common/mtk-scp-ultra.ko",
    "sound/soc/mediatek/ultrasound/ultra_scp/snd-soc-mtk-scp-ultra.ko",
    "sound/soc/mediatek/vow/mtk-scp-vow.ko",
]

mgk_64_platform_device_modules = {
    # keep sorted
    "drivers/clk/mediatek/clk-bringup.ko": "mt6781 mt6789 mt6833 mt6877 mt6897 mt6886 mt6893 mt6983 mt6985 mt6989 mt8192 mt8188 mt6899",
    "drivers/clk/mediatek/clk-chk-mt6789.ko": "mt6789",
    "drivers/clk/mediatek/clk-chk-mt6886.ko": "mt6886",
    "drivers/clk/mediatek/clk-chk-mt6897.ko": "mt6897",
    "drivers/clk/mediatek/clk-mt6789.ko": "mt6789",
    "drivers/clk/mediatek/clk-mt6789-audiosys.ko": "mt6789",
    "drivers/clk/mediatek/clk-mt6789-cam.ko": "mt6789",
    "drivers/clk/mediatek/clk-mt6789-mmsys.ko": "mt6789",
    "drivers/clk/mediatek/clk-mt6789-img.ko": "mt6789",
    "drivers/clk/mediatek/clk-mt6789-i2c.ko": "mt6789",
    "drivers/clk/mediatek/clk-mt6789-bus.ko": "mt6789",
    "drivers/clk/mediatek/clk-mt6789-vcodec.ko": "mt6789",
    "drivers/clk/mediatek/clk-mt6789-disp_dsc.ko": "mt6789",
    "drivers/clk/mediatek/clk-mt6789-impen.ko": "mt6789",
    "drivers/clk/mediatek/clk-mt6789-mfgcfg.ko": "mt6789",
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
    "drivers/clk/mediatek/clk-mt6893-pg.ko": "mt6893",
    "drivers/misc/mediatek/cmdq/mailbox/cmdq-platform-mt6893.ko": "mt6893",
    "drivers/soc/mediatek/mtk-scpsys-mt6833.ko": "mt6833",
    "drivers/soc/mediatek/mtk-scpsys-mt6893.ko": "mt6893",
    "drivers/misc/mediatek/cmdq/mailbox/cmdq-platform-mt6853.ko": "mt6853",
    "drivers/misc/mediatek/cmdq/mailbox/cmdq-platform-mt6781.ko": "mt6781",
    "drivers/soc/mediatek/mtk-scpsys-mt6853.ko": "mt6853",
    "drivers/clk/mediatek/clk-chk-mt6893.ko": "mt6893",
    "drivers/clk/mediatek/clk-chk-mt6899.ko": "mt6899",
    "drivers/clk/mediatek/clk-chk-mt6983.ko": "mt6983",
    "drivers/clk/mediatek/clk-chk-mt6985.ko": "mt6985",
    "drivers/clk/mediatek/clk-chk-mt6989.ko": "mt6989",
    "drivers/clk/mediatek/clk-chk-mt6991.ko": "mt6991",
    "drivers/clk/mediatek/clk-dbg-mt6789.ko": "mt6789",
    "drivers/clk/mediatek/clk-dbg-mt6893.ko": "mt6893",
    "drivers/clk/mediatek/clk-dbg-mt6886.ko": "mt6886",
    "drivers/clk/mediatek/clk-dbg-mt6897.ko": "mt6897",
    "drivers/clk/mediatek/clk-dbg-mt6899.ko": "mt6899",
    "drivers/clk/mediatek/clk-dbg-mt6983.ko": "mt6983",
    "drivers/clk/mediatek/clk-dbg-mt6985.ko": "mt6985",
    "drivers/clk/mediatek/clk-dbg-mt6989.ko": "mt6989",
    "drivers/clk/mediatek/clk-dbg-mt6991.ko": "mt6991",
    "drivers/clk/mediatek/clk-fmeter-mt6789.ko": "mt6789",
    "drivers/clk/mediatek/clk-fmeter-mt6886.ko": "mt6886",
    "drivers/clk/mediatek/clk-fmeter-mt6897.ko": "mt6897",
    "drivers/clk/mediatek/clk-fmeter-mt6899.ko": "mt6899",
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
    "drivers/clk/mediatek/clk-mt6899.ko": "mt6899",
    "drivers/clk/mediatek/clk-mt6899-adsp.ko": "mt6899",
    "drivers/clk/mediatek/clk-mt6899-cam.ko": "mt6899",
    "drivers/clk/mediatek/clk-mt6899-img.ko": "mt6899",
    "drivers/clk/mediatek/clk-mt6899-mmsys.ko": "mt6899",
    "drivers/clk/mediatek/clk-mt6899-peri.ko": "mt6899",
    "drivers/clk/mediatek/clk-mt6899-infracfg_ao.ko": "mt6899",
    "drivers/clk/mediatek/clk-mt6899-mdpsys.ko": "mt6899",
    "drivers/clk/mediatek/clk-mt6899-vcodec.ko": "mt6899",
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
    "drivers/clk/mediatek/clk-mt6991-pd.ko": "mt6991",
    "drivers/clk/mediatek/clk-mt6993.ko" : "mt6993",
    "drivers/clk/mediatek/clk-mt6993-vcodec.ko" : "mt6993",
    "drivers/clk/mediatek/clk-mt6993-adsp.ko" : "mt6993",
    "drivers/clk/mediatek/clk-mt6993-mmsys.ko" : "mt6993",
    "drivers/clk/mediatek/clk-mt6993-img.ko" : "mt6993",
    "drivers/clk/mediatek/clk-mt6993-peri.ko" : "mt6993",
    "drivers/clk/mediatek/clk-mt6993-cam.ko" : "mt6993",
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
    "drivers/clk/mediatek/pd-chk-mt6789.ko": "mt6789",
    "drivers/clk/mediatek/pd-chk-mt6886.ko": "mt6886",
    "drivers/clk/mediatek/pd-chk-mt6893.ko": "mt6893",
    "drivers/clk/mediatek/pd-chk-mt6897.ko": "mt6897",
    "drivers/clk/mediatek/pd-chk-mt6899.ko": "mt6899",
    "drivers/clk/mediatek/pd-chk-mt6983.ko": "mt6983",
    "drivers/clk/mediatek/pd-chk-mt6985.ko": "mt6985",
    "drivers/clk/mediatek/pd-chk-mt6989.ko": "mt6989",
    "drivers/clk/mediatek/pd-chk-mt6991.ko": "mt6991",
    "drivers/gpu/drm/mediatek/mml/mtk-mml-mt6878.ko": "mt6878",
    "drivers/gpu/drm/mediatek/mml/mtk-mml-mt6886.ko": "mt6886",
    "drivers/gpu/drm/mediatek/mml/mtk-mml-mt6897.ko": "mt6897",
    "drivers/gpu/drm/mediatek/mml/mtk-mml-mt6899.ko": "mt6899",
    "drivers/gpu/drm/mediatek/mml/mtk-mml-mt6983.ko": "mt6983",
    "drivers/gpu/drm/mediatek/mml/mtk-mml-mt6985.ko": "mt6985",
    "drivers/gpu/drm/mediatek/mml/mtk-mml-mt6989.ko": "mt6989",
    "drivers/gpu/drm/mediatek/mml/mtk-mml-mt6991.ko": "mt6991",
    "drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6886.ko": "mt6886",
    "drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6897.ko": "mt6897",
    "drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6899.ko": "mt6899",
    "drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6985.ko": "mt6985",
    "drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6989.ko": "mt6989",
    "drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6989_fpga.ko": "mt6989",
    "drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6991.ko": "mt6991",
    "drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6993.ko": "mt6993",
    "drivers/gpu/mediatek/gpu_iommu/mtk_gpu_iommu_mt6989.ko": "mt6989",
    "drivers/gpu/mediatek/gpu_iommu/mtk_gpu_iommu_mt6991.ko": "mt6991",
    "drivers/gpu/mediatek/gpu_iommu/mtk_gpu_iommu_mt6993.ko": "mt6993",
    "drivers/gpu/mediatek/gpu_pdma/mtk_gpu_pdma_mt6991.ko": "mt6991",
    "drivers/interconnect/mediatek/mmqos-mt6886.ko": "mt6886",
    "drivers/interconnect/mediatek/mmqos-mt6893.ko": "mt6893",
    "drivers/interconnect/mediatek/mmqos-mt6897.ko": "mt6897",
    "drivers/interconnect/mediatek/mmqos-mt6983.ko": "mt6983",
    "drivers/interconnect/mediatek/mmqos-mt6985.ko": "mt6985",
    "drivers/interconnect/mediatek/mmqos-mt6989.ko": "mt6989",
    "drivers/interconnect/mediatek/mmqos-mt6991.ko": "mt6991",
    "drivers/interconnect/mediatek/mmqos-mt6877.ko": "mt6877",
    "drivers/soc/mediatek/mmdvfs/mmdvfs-mt6991.ko": "mt6991",
    "drivers/misc/mediatek/cameraisp/pda/pda_drv_mt6855.ko": "mt6855",
    "drivers/misc/mediatek/cameraisp/pda/pda_drv_mt6879.ko": "mt6879",
    "drivers/misc/mediatek/cameraisp/pda/pda_drv_mt6886.ko": "mt6886",
    "drivers/misc/mediatek/cameraisp/pda/pda_drv_mt6895.ko": "mt6895",
    "drivers/misc/mediatek/cameraisp/pda/pda_drv_mt6897.ko": "mt6897",
    "drivers/misc/mediatek/cameraisp/pda/pda_drv_mt6985.ko": "mt6985",
    "drivers/misc/mediatek/cameraisp/pda/pda_drv_mt6989.ko": "mt6989",
    "drivers/misc/mediatek/cameraisp/pda/pda_drv_mt6878.ko": "mt6878",
    "drivers/misc/mediatek/cmdq/mailbox/cmdq-platform-mt6761.ko": "mt6761",
    "drivers/misc/mediatek/cmdq/mailbox/cmdq-platform-mt6765.ko": "mt6765",
    "drivers/misc/mediatek/cmdq/mailbox/cmdq-platform-mt6878.ko": "mt6878",
    "drivers/misc/mediatek/cmdq/mailbox/cmdq-platform-mt6886.ko": "mt6886",
    "drivers/misc/mediatek/cmdq/mailbox/cmdq-platform-mt6897.ko": "mt6897",
    "drivers/misc/mediatek/cmdq/mailbox/cmdq-platform-mt6983.ko": "mt6983",
    "drivers/misc/mediatek/cmdq/mailbox/cmdq-platform-mt6985.ko": "mt6985",
    "drivers/misc/mediatek/cmdq/mailbox/cmdq-platform-mt6989.ko": "mt6989",
    "drivers/misc/mediatek/cmdq/mailbox/cmdq-platform-mt6768.ko": "mt6768",
    "drivers/misc/mediatek/cmdq/mailbox/cmdq-platform-mt6991.ko": "mt6991",
    "drivers/misc/mediatek/cmdq/mailbox/cmdq-platform-mt6877.ko": "mt6877",
    "drivers/misc/mediatek/cmdq/mailbox/cmdq-platform-mt6899.ko": "mt6899",
    "drivers/misc/mediatek/cmdq/mailbox/cmdq-platform-mt6833.ko": "mt6833",
    "drivers/soc/mediatek/mtk-scpsys-mt6765.ko": "mt6765",
    "drivers/soc/mediatek/mtk-scpsys-mt6768.ko": "mt6768",
    "drivers/soc/mediatek/mtk-scpsys-mt6781.ko": "mt6781",
    "drivers/soc/mediatek/mtk-scpsys-mt6761.ko": "mt6761",
    "drivers/soc/mediatek/mtk-scpsys-mt6877.ko": "mt6877",
    "drivers/misc/mediatek/cm_mgr/mtk_cm_mgr_mt6886.ko": "mt6886",
    "drivers/misc/mediatek/cm_mgr/mtk_cm_mgr_mt6897.ko": "mt6897",
    "drivers/misc/mediatek/cm_mgr/mtk_cm_mgr_mt6983.ko": "mt6983",
    "drivers/misc/mediatek/cm_mgr/mtk_cm_mgr_mt6985.ko": "mt6985",
    "drivers/misc/mediatek/cm_mgr/mtk_cm_mgr_mt6989.ko": "mt6989",
    "drivers/misc/mediatek/cm_mgr/mtk_cm_mgr_mt6991.ko": "mt6991",
    "drivers/misc/mediatek/dcm/mt6897_dcm.ko": "mt6897",
    "drivers/misc/mediatek/dcm/mt6899_dcm.ko": "mt6899",
    "drivers/misc/mediatek/dcm/mt6985_dcm.ko": "mt6985",
    "drivers/misc/mediatek/dcm/mt6989_dcm.ko": "mt6989",
    "drivers/misc/mediatek/dcm/mt6991_dcm.ko": "mt6991",
    #"drivers/misc/mediatek/lpm/modules/debug/mt6886/mtk-lpm-dbg-mt6886.ko": "mt6886",
    #"drivers/misc/mediatek/lpm/modules/debug/mt6897/mtk-lpm-dbg-mt6897.ko": "mt6897",
    #"drivers/misc/mediatek/lpm/modules/debug/mt6983/mtk-lpm-dbg-mt6983.ko": "mt6983",
    #"drivers/misc/mediatek/lpm/modules/debug/mt6985/mtk-lpm-dbg-mt6985.ko": "mt6985",
    #"drivers/misc/mediatek/lpm/modules/debug/mt6989/mtk-lpm-dbg-mt6989.ko": "mt6989",
    "drivers/misc/mediatek/mdp/mdp_drv_mt6761.ko": "mt6761",
    "drivers/misc/mediatek/mdp/mdp_drv_mt6765.ko": "mt6765",
    "drivers/misc/mediatek/mdp/mdp_drv_mt6768.ko": "mt6768",
    "drivers/misc/mediatek/mdp/mdp_drv_mt6781.ko": "mt6781",
    "drivers/misc/mediatek/mdp/mdp_drv_mt6789.ko": "mt6789",
    "drivers/misc/mediatek/mdp/mdp_drv_mt6833.ko": "mt6833",
    "drivers/misc/mediatek/mdp/mdp_drv_mt6853.ko": "mt6853",
    "drivers/misc/mediatek/mdp/mdp_drv_mt6877.ko": "mt6877",
    "drivers/misc/mediatek/mdp/mdp_drv_mt6886.ko": "mt6886",
    "drivers/misc/mediatek/mdp/mdp_drv_mt6893.ko": "mt6893",
    "drivers/misc/mediatek/mdp/mdp_drv_mt6897.ko": "mt6897",
    "drivers/misc/mediatek/mdp/mdp_drv_mt6983.ko": "mt6983",
    "drivers/misc/mediatek/mdp/mdp_drv_mt6985.ko": "mt6985",
    "drivers/misc/mediatek/mdp/mdp_drv_mt6989.ko": "mt6989",
    "drivers/misc/mediatek/mdp/mdp_drv_mt6991.ko": "mt6991",
    "drivers/misc/mediatek/slbc/slbc_mt6886.ko": "mt6886",
    "drivers/misc/mediatek/slbc/slbc_mt6893.ko": "mt6893",
    "drivers/misc/mediatek/slbc/slbc_mt6895.ko": "mt6895",
    "drivers/misc/mediatek/slbc/slbc_mt6897.ko": "mt6897",
    "drivers/misc/mediatek/slbc/slbc_mt6899.ko": "mt6899",
    "drivers/misc/mediatek/slbc/slbc_mt6983.ko": "mt6983",
    "drivers/misc/mediatek/slbc/slbc_mt6985.ko": "mt6985",
    "drivers/misc/mediatek/slbc/slbc_mt6989.ko": "mt6989",
    "drivers/misc/mediatek/slbc/slbc_mt6991.ko": "mt6991",
    "drivers/soc/mediatek/devapc/device-apc-mt6761.ko": "mt6761",
    "drivers/soc/mediatek/devapc/device-apc-mt6765.ko": "mt6765",
    "drivers/soc/mediatek/devapc/device-apc-mt6768.ko": "mt6768",
    "drivers/soc/mediatek/devapc/device-apc-mt6781.ko": "mt6781",
    "drivers/soc/mediatek/devapc/device-apc-mt6833.ko": "mt6833",
    "drivers/soc/mediatek/devapc/device-apc-mt6853.ko": "mt6853",
    "drivers/soc/mediatek/devapc/device-apc-mt6877.ko": "mt6877",
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
    "drivers/soc/mediatek/mtk-scpsys-bringup.ko": "mt6761 mt6765 mt6768 mt6781 mt6833 mt6853 mt6877 mt6897 mt6886 mt6893 mt6983 mt6985 mt6989 mt8192 mt8188 mt6899",
    "drivers/soc/mediatek/mtk-scpsys-mt6886.ko": "mt6886",
    "drivers/soc/mediatek/mtk-scpsys-mt6897.ko": "mt6897",
    "drivers/soc/mediatek/mtk-scpsys-mt6899.ko": "mt6899",
    "drivers/soc/mediatek/mtk-scpsys-mt6983.ko": "mt6983",
    "drivers/soc/mediatek/mtk-scpsys-mt6985.ko": "mt6985",
    "drivers/soc/mediatek/mtk-scpsys-mt6989.ko": "mt6989",
    "drivers/soc/mediatek/mtk-scpsys-mt6991-spm.ko": "mt6991",
    "drivers/soc/mediatek/mtk-scpsys-mt6991-mmpc.ko": "mt6991",
    "drivers/misc/mediatek/vmm_spm/mtk-vmm-spm-mt6989.ko": "mt6989",
    "drivers/misc/mediatek/vmm/mtk-vmm-notifier-mt6991.ko": "mt6991",
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
    "drivers/misc/mediatek/locking/locking_aee.ko",
    "drivers/misc/mediatek/irq_monitor/irq_monitor.ko",
]

mgk_64_platform_device_eng_modules = {
}


mgk_64_device_userdebug_modules = [
    "drivers/misc/mediatek/irq_monitor/irq_monitor.ko",
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
        mgk_64_device_modules.append("drivers/gpu/drm/bridge/maxiam-max96851.ko")
        mgk_64_device_modules.append("drivers/gpu/drm/mediatek/mediatek_v2/mtk_drm_edp/mtk_drm_edp.ko")
        mgk_64_device_modules.append("drivers/gpu/drm/panel/panel-maxiam-max96851.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/carevent/carevent.ko")
        mgk_64_device_modules.append("drivers/thermal/mediatek/fan_cooling.ko")
        mgk_64_device_modules.append("sound/soc/codecs/ak7709/snd-soc-ak7709.ko")
        mgk_64_device_modules.append("sound/soc/codecs/hfda80x/snd-soc-hfda80x.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/vow/ver02/mtk-vow.ko")
        mgk_64_device_modules.remove("sound/soc/mediatek/audio_dsp/mtk-soc-offload-common.ko")
        mgk_64_device_modules.remove("sound/soc/mediatek/ultrasound/ultra_common/mtk-scp-ultra.ko")
        mgk_64_device_modules.remove("sound/soc/mediatek/ultrasound/ultra_scp/snd-soc-mtk-scp-ultra.ko")
        mgk_64_device_modules.remove("sound/soc/mediatek/vow/mtk-scp-vow.ko")

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
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/connectivity/conninfra:conninfra")
        mgk_64_kleaf_modules.append("//vendor/mediatek/kernel_modules/connectivity/conninfra/build/thinmd:conninfra")
        mgk_64_kleaf_modules.append("//vendor/mediatek/kernel_modules/thinmd_exception:thinmd_exception")
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
        mgk_64_common_eng_modules.append("drivers/pps/clients/pps-gpio.ko")
        mgk_64_common_userdebug_modules.append("drivers/pps/clients/pps-gpio.ko")
        mgk_64_common_user_modules.append("drivers/pps/clients/pps-gpio.ko")

    if "mt6877_overlay.config" in DEFCONFIG_OVERLAYS:
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/gpu:gpu")
        mgk_64_kleaf_modules.append("//vendor/mediatek/kernel_modules/gpu:gpu_mt6877")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/mtkcam/sched:c2ps")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/mtkcam/sched/c2ps_ioctl:c2ps_perf_ioctl")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/mtkcam/isp_pspm:isp_pspm")
        mgk_64_kleaf_platform_modules.update({"//kernel_device_modules-{}/drivers/pinctrl/mediatek:pinctrl-mt6877".format(kernel_version): "mt6877"})
        mgk_64_device_modules.remove("drivers/misc/mediatek/performance/mtk_perf_ioctl_magt.ko")
        mgk_64_device_modules.remove("drivers/memory/mediatek/emi.ko")
        mgk_64_device_modules.remove("drivers/memory/mediatek/emi-fake-eng.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/memory/mediatek:emi-mpu".format(kernel_version))
        mgk_64_device_modules.remove("drivers/memory/mediatek/emi-mpu-test.ko")
        mgk_64_device_modules.remove("drivers/memory/mediatek/emi-mpu-test-v2.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/memory/mediatek:emi-slb".format(kernel_version))
        mgk_64_device_modules.remove("drivers/memory/mediatek/smpu.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/memory/mediatek:slc-parity".format(kernel_version))
        mgk_64_device_modules.remove("drivers/memory/mediatek/smpu-hook-v1.ko")
        mgk_64_common_eng_modules.remove("drivers/firmware/arm_ffa/ffa-module.ko")
        mgk_64_common_userdebug_modules.remove("drivers/firmware/arm_ffa/ffa-module.ko")
        mgk_64_common_user_modules.remove("drivers/firmware/arm_ffa/ffa-module.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/eemgpu/mtk_eem.ko")
        mgk_64_device_modules.append("drivers/tee/gud/600/MobiCoreDriver/mcDrvModule.ko")
        mgk_64_device_modules.append("drivers/tee/gud/600/MobiCoreDriver/mcDrvModule-ffa.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/drm/mediatek/mml/mtk-mml-mt6878.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/drm/mediatek/mml/mtk-mml-mt6886.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/drm/mediatek/mml/mtk-mml-mt6897.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/drm/mediatek/mml/mtk-mml-mt6899.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/drm/mediatek/mml/mtk-mml-mt6983.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/drm/mediatek/mml/mtk-mml-mt6985.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/drm/mediatek/mml/mtk-mml-mt6989.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/drm/mediatek/mml/mtk-mml-mt6991.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/pkvm_tmem/pkvm_tmem.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/trusted_mem/tmem_ffa.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/trusted_mem/ffa_v11.ko")
        mgk_64_device_modules.remove("drivers/tee/teei/510/isee-ffa.ko")
        mgk_64_device_modules.append("drivers/tee/teei/515/isee.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6991/mtk-swpm-audio-dbg-v6991.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6991/mtk-swpm-dbg-v6991.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6991/mtk-swpm-cpu-dbg-v6991.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6991/mtk-swpm-mml-dbg-v6991.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6991/mtk-swpm-isp-dbg-v6991.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi_legacy_v2/emi.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi_legacy_v2/emi-mpu.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi_legacy_v2/emi-mpu-test.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi-dummy.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi_legacy_v2/emi-mpu-hook-v1.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi_legacy_v2/emi-slb.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi_legacy_v2/emi-mpu-test-v2.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi_legacy_v2/emi-fake-eng.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi_legacy_v2/emi-mpu-v2.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/lpm_legacy/modules/platform/v1/mtk-lpm-plat-v1-legacy.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/lpm_legacy/modules/debug/k6877/mtk-lpm-dbg-mt6877-legacy.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/lpm_legacy/mtk-lpm-legacy.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/lpm_legacy/modules/debug/v1/mtk-lpm-dbg-common-v1-legacy.ko")
        mgk_64_device_modules.remove("drivers/iommu/arm/arm-smmu-v3/arm_smmu_v3.ko")
        mgk_64_device_modules.remove("drivers/iommu/arm/arm-smmu-v3/mtk-smmuv3-pmu.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/iommu/arm/arm-smmu-v3:mtk-smmuv3-mpam-mon".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/iommu:mtk_smmu_qos".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/iommu:smmu_secure".format(kernel_version))
        mgk_64_device_modules.append("drivers/misc/mediatek/iommu/iommu_gz.ko")
        mgk_64_device_modules.remove("drivers/tee/gud/610/TlcTui/t-base-tui.ko")

        mgk_64_device_modules.remove("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_wrapper.ko")
        mgk_64_device_modules.append("drivers/gpu/mediatek/gpufreq/v2_legacy/mtk_gpufreq_wrapper_legacy.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cpuhotplug/mtk_cpuhp.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/sspm/v3/sspm_v3.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/sspm/v2/sspm.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/cm_mgr/mtk_cm_ipi.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/cm_mgr/mtk_cm_mgr.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cm_mgr_legacy_v1/mtk_cm_mgr.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/ise_lpm/ise_lpm.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/ise_trusty/ise-trusty.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/ise_trusty/ise-trusty-ipc.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/ise_trusty/ise-trusty-log.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/ise_trusty/ise-trusty-virtio.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mminfra/mtk-mminfra-debug.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mminfra/mtk-mminfra-imax.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/pmsr:pmsr".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/pmsr/v2:pmsr_v2".format(kernel_version))
        mgk_64_device_modules.remove("drivers/misc/mediatek/pmsr/v3/pmsr_v3.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/slbc/mmsram.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/slbc:mtk_slbc".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/slbc:slbc_ipi".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/slbc:slbc_trace".format(kernel_version))
        mgk_64_device_modules.remove("drivers/misc/mediatek/ssc/debug/v1/mtk-ssc-dbg-v1.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/ssc/debug/v2/mtk-ssc-dbg-v2.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/ssc/mtk-ssc.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v1/mtk-swpm-dbg-common-v1.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug/v1:mtk-swpm-isp-wrapper".format(kernel_version))
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
        mgk_64_device_modules.remove("drivers/misc/mediatek/mdpm/mtk_mdpm.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/mdpm_v1/mtk_mdpm_v1.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm:mtk-swpm-perf-arm-pmu".format(kernel_version))
        mgk_64_device_modules.remove("drivers/misc/mediatek/tinysys_scmi/tinysys-scmi.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6897/mtk-swpm-audio-dbg-v6897.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6989/mtk-swpm-audio-dbg-v6989.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6989/mtk-swpm-core-dbg-v6989.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6989/mtk-swpm-cpu-dbg-v6989.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6989/mtk-swpm-dbg-v6989.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6989/mtk-swpm-disp-dbg-v6989.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6989/mtk-swpm-mem-dbg-v6989.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6989/mtk-swpm-mml-dbg-v6989.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6991/mtk-swpm-disp-dbg-v6991.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/bridge/mtk_mbraink_bridge.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/mtk_mbraink.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/modules/v6989/mtk_mbraink_v6989.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/modules/v6991/mtk_mbraink_v6991.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/modules/v6899/mtk_mbraink_v6899.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/vcp/rv/vcp.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/vcp/rv/vcp_status.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/vdec_fmt/vdec-fmt.ko")
        mgk_64_device_modules.remove("drivers/media/platform/mtk-vcodec/mtk-vcodec-dec-v2.ko")
        mgk_64_device_modules.remove("drivers/media/platform/mtk-vcodec/mtk-vcodec-enc-v2.ko")
        mgk_64_device_modules.remove("drivers/soc/mediatek/mtk-mmdvfs-v3.ko")
        mgk_64_device_modules.append("drivers/soc/mediatek/devmpu/devmpu.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/adsp/v0/adsp-v0.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/adsp/v1/adsp-v1.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/adsp/v2/adsp-v2.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/adsp/v3/adsp-v3.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/usb/usb_offload/usb_offload.ko")
        mgk_64_device_modules.remove("sound/soc/codecs/snd-soc-mt6338.ko")
        mgk_64_device_modules.remove("sound/soc/codecs/snd-soc-mt6368.ko")
        mgk_64_device_modules.remove("sound/soc/codecs/snd-soc-mt6681.ko")

        mgk_64_device_modules.append("drivers/misc/mediatek/sensor/2.0/mtk_nanohub/nanohub.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/sensor/2.0/sensorhub/sensorhub.ko")

        mgk_64_platform_device_modules.pop("drivers/clk/mediatek/clk-chk-mt6989.ko")
        mgk_64_platform_device_modules.pop("drivers/clk/mediatek/pd-chk-mt6989.ko")
        mgk_64_platform_device_modules.pop("drivers/clk/mediatek/clk-chk-mt6991.ko")
        mgk_64_platform_device_modules.pop("drivers/clk/mediatek/pd-chk-mt6991.ko")

        mgk_64_platform_device_modules.update({"drivers/clk/mediatek/clkchk-mt6877.ko":"mt6877"})
        mgk_64_platform_device_modules.update({"drivers/clk/mediatek/clkdbg-mt6877.ko":"mt6877"})
        mgk_64_platform_device_modules.update({"drivers/clk/mediatek/clk-fmeter-mt6877.ko":"mt6877"})
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
        mgk_64_platform_device_modules.update({"drivers/clk/mediatek/pd-chk-mt6877.ko":"mt6877"})
        mgk_64_platform_device_modules.update({"drivers/gpu/mediatek/gpufreq/v2_legacy/mtk_gpufreq_mt6877.ko":"mt6877"})
        mgk_64_platform_device_modules.update({"drivers/misc/mediatek/eem_v2/mediatek_eem.ko":"mt6877"})
        mgk_64_platform_device_modules.update({"drivers/misc/mediatek/eem_v2/mtk_picachu.ko":"mt6877"})
        mgk_64_platform_device_modules.update({"drivers/misc/mediatek/ppm_v3/mtk_ppm_v3.ko":"mt6877"})
        mgk_64_platform_device_modules.update({"drivers/misc/mediatek/cpufreq_v2/src/CPU_DVFS.ko":"mt6877"})
        mgk_64_platform_device_modules.update({"drivers/misc/mediatek/leakage_table_v2/mediatek_static_power.ko":"mt6877"})
        mgk_64_platform_device_modules.update({"drivers/misc/mediatek/upower/Upower.ko":"mt6877"})
        mgk_64_kleaf_device_modules.update("//kernel_device_modules-{}/drivers/misc/mediatek/subpmic:generic_debugfs".format(kernel_version))
        mgk_64_kleaf_device_modules.update("//kernel_device_modules-{}/drivers/misc/mediatek/subpmic:mt6360-dbg".format(kernel_version))
        mgk_64_platform_device_modules.update({"drivers/misc/mediatek/cm_mgr_legacy_v1/mtk_cm_mgr_mt6877.ko":"mt6877"})
        mgk_64_platform_device_modules.update({"drivers/misc/mediatek/dcm/mt6877_dcm.ko":"mt6877"})
        #mgk_64_platform_device_modules.pop("drivers/misc/mediatek/lpm/modules/debug/mt6989/mtk-lpm-dbg-mt6989.ko")
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/lpm/modules/debug/mt6991:mtk-lpm-dbg-mt6991".format(kernel_version))
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/slbc/slbc_mt6886.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/slbc/slbc_mt6893.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/slbc/slbc_mt6895.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/slbc/slbc_mt6897.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/slbc/slbc_mt6899.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/slbc/slbc_mt6983.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/slbc/slbc_mt6985.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/slbc/slbc_mt6989.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/slbc/slbc_mt6991.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/cm_mgr/mtk_cm_mgr_mt6989.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/cm_mgr/mtk_cm_mgr_mt6886.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/cm_mgr/mtk_cm_mgr_mt6897.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/cm_mgr/mtk_cm_mgr_mt6983.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/cm_mgr/mtk_cm_mgr_mt6985.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/cm_mgr/mtk_cm_mgr_mt6991.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6886.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6897.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6899.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6985.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6989.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6989_fpga.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6991.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6993.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpu_pdma/mtk_gpu_pdma_mt6991.ko")
        mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6886/mt6886-mt6368.ko")
        mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6886/snd-soc-mt6886-afe.ko")
        mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6897/mt6897-mt6368.ko")
        mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6897/snd-soc-mt6897-afe.ko")
        mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6983/mt6983-mt6338.ko")
        mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6983/snd-soc-mt6983-afe.ko")
        mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6985/mt6985-mt6338.ko")
        mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6985/snd-soc-mt6985-afe.ko")
        mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6989/mt6989-mt6681.ko")
        mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6989/snd-soc-mt6989-afe.ko")
        mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6991/mt6991-mt6681.ko")
        mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6991/snd-soc-mt6991-afe.ko")
        mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6885/mt6885-mt6359p.ko")
        mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6885/snd-soc-mt6885-afe.ko")
        mgk_64_platform_device_modules.update({"sound/soc/mediatek/mt6877/mt6877-mt6359.ko":"mt6877"})
        mgk_64_platform_device_modules.update({"sound/soc/mediatek/mt6877/snd-soc-mt6877-afe.ko":"mt6877"})
        mgk_64_device_modules.append("drivers/misc/mediatek/cameraisp/dip/isp_6s/camera_dip_isp6s.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cameraisp/mfb/camera_mfb_isp6s.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/thermal/thermal_monitor.ko")

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
        mgk_64_device_modules.remove("drivers/trusty/trusty-core.ko")
        mgk_64_device_modules.remove("drivers/trusty/trusty-ipc.ko")
        mgk_64_device_modules.remove("drivers/trusty/trusty-log.ko")
        mgk_64_device_modules.remove("drivers/trusty/trusty-test.ko")
        mgk_64_device_modules.remove("drivers/trusty/trusty-virtio.ko")

        mgk_64_device_modules.remove("drivers/gpu/mediatek/gpueb/mtk_gpueb.ko")
        mgk_64_device_modules.remove("drivers/gpu/mediatek/gpueb/mtk_ghpm.ko")
        mgk_64_device_modules.remove("drivers/gpu/mediatek/gpueb/mtk_ghpm_swwa.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/imgsensor/src/isp6s/imgsensor_isp6s.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/imgsensor/src/isp6s_mon/imgsensor_isp6s_mon.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/cam_cal/src/custom/camera_eeprom.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cam_cal/src/isp6s_mon/camera_eeprom_isp6s_mon.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cameraisp/wpe/isp_6s/camera_wpe_isp6s.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cameraisp/fdvt/camera_fdvt_isp51.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cameraisp/dpe/camera_dpe_isp60.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cameraisp/rsc/camera_rsc_isp6s.ko")

    if "mt6781_overlay.config" in DEFCONFIG_OVERLAYS:
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/gpu:gpu")
        mgk_64_kleaf_modules.append("//vendor/mediatek/kernel_modules/gpu:gpu_mt6781")
        mgk_64_platform_device_modules.update({"drivers/gpu/mediatek/gpufreq/v2_legacy/mtk_gpufreq_mt6781.ko":"mt6781"})
        #mgk_64_device_modules.append("drivers/gpu/mediatek/gpufreq/v2_legacy/mtk_gpufreq_mt6781.ko")
        mgk_64_device_modules.remove("drivers/char/rpmb/rpmb.ko")
        mgk_64_device_modules.remove("drivers/char/rpmb/rpmb-mtk.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/adsp/adsp.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/adsp/v1/adsp-v1.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/adsp/v2/adsp-v2.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/adsp/v3/adsp-v3.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/audio_ipi/audio_ipi.ko")
        mgk_64_device_modules.remove("sound/soc/mediatek/audio_dsp/mtk-soc-offload-common.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/usb/usb_offload/usb_offload.ko")
        mgk_64_device_modules.remove("sound/soc/mediatek/ultrasound/ultra_common/mtk-scp-ultra.ko")
        mgk_64_device_modules.remove("sound/soc/mediatek/ultrasound/ultra_scp/snd-soc-mtk-scp-ultra.ko")
        mgk_64_device_modules.remove("sound/soc/mediatek/audio_dsp/snd-soc-audiodsp-common.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/vow/ver02/mtk-vow.ko")
        mgk_64_device_modules.remove("sound/soc/mediatek/vow/mtk-scp-vow.ko")
        mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6985/mt6985-mt6338.ko")
        mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6985/snd-soc-mt6985-afe.ko")
        mgk_64_device_modules.append("sound/soc/codecs/snd-soc-mt6660.ko")
        mgk_64_device_modules.append("sound/soc/codecs/snd-soc-mt6366.ko")
        mgk_64_platform_device_modules.update({"sound/soc/mediatek/mt6781/mt6781-mt6366.ko":"mt6781"})
        mgk_64_platform_device_modules.update({"sound/soc/mediatek/mt6781/snd-soc-mt6781-afe.ko":"mt6781"})
        mgk_64_platform_device_modules.update({"drivers/interconnect/mediatek/mmqos-mt6781.ko":"mt6781"})
        mgk_64_kleaf_platform_modules.update({"//kernel_device_modules-{}/drivers/pinctrl/mediatek:pinctrl-mt6781".format(kernel_version): "mt6781"})
        mgk_64_device_modules.remove("drivers/misc/mediatek/pkvm_tmem/pkvm_tmem.ko")
        mgk_64_common_eng_modules.remove("drivers/firmware/arm_ffa/ffa-module.ko")
        mgk_64_common_userdebug_modules.remove("drivers/firmware/arm_ffa/ffa-module.ko")
        mgk_64_common_user_modules.remove("drivers/firmware/arm_ffa/ffa-module.ko")
        mgk_64_device_modules.remove("drivers/iommu/arm/arm-smmu-v3/arm_smmu_v3.ko")
        mgk_64_device_modules.remove("drivers/iommu/arm/arm-smmu-v3/mtk-smmuv3-pmu.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/iommu/arm/arm-smmu-v3:mtk-smmuv3-mpam-mon".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/iommu:smmu_secure".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/iommu:mtk_smmu_qos".format(kernel_version))
        mgk_64_device_modules.append("drivers/misc/mediatek/iommu/iommu_gz.ko")
        mgk_64_device_modules.append("drivers/clk/mediatek/clk-mt6781.ko")
        mgk_64_device_modules.append("drivers/clk/mediatek/clkdbg-mt6781.ko")
        mgk_64_device_modules.append("drivers/clk/mediatek/clk-chk-mt6781.ko")
        mgk_64_device_modules.append("drivers/clk/mediatek/clk-mt6781-pg.ko")
        mgk_64_device_modules.append("drivers/soc/mediatek/devmpu/devmpu.ko")
        mgk_64_device_modules.remove("drivers/memory/mediatek/emi.ko")
        mgk_64_device_modules.remove("drivers/memory/mediatek/emi-fake-eng.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/memory/mediatek:emi-mpu".format(kernel_version))
        mgk_64_device_modules.remove("drivers/memory/mediatek/emi-mpu-test.ko")
        mgk_64_device_modules.remove("drivers/memory/mediatek/emi-mpu-test-v2.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/memory/mediatek:emi-slb".format(kernel_version))
        mgk_64_device_modules.remove("drivers/memory/mediatek/smpu.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/memory/mediatek:slc-parity".format(kernel_version))
        mgk_64_device_modules.remove("drivers/memory/mediatek/smpu-hook-v1.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi_legacy_v2/emi.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi_legacy_v2/emi-mpu.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi_legacy_v2/emi-mpu-test.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi-dummy.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi_legacy_v2/emi-mpu-hook-v1.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi_legacy_v2/emi-slb.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi_legacy_v2/emi-mpu-test-v2.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi_legacy_v2/emi-fake-eng.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi_legacy_v2/emi-mpu-v2.ko")
        mgk_64_device_modules.append("drivers/gpu/mediatek/gpufreq/v2_legacy/mtk_gpufreq_wrapper_legacy.ko")
        mgk_64_device_modules.remove("drivers/gpu/mediatek/gpueb/mtk_gpueb.ko")
        mgk_64_device_modules.remove("drivers/gpu/mediatek/gpueb/mtk_ghpm.ko")
        mgk_64_device_modules.remove("drivers/gpu/mediatek/gpueb/mtk_ghpm_swwa.ko")
        mgk_64_device_modules.remove("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_wrapper.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6886.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6897.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6899.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6985.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6989.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6989_fpga.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6991.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6993.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpu_pdma/mtk_gpu_pdma_mt6991.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/vcp/rv/vcp.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/vcp/rv/vcp_status.ko")
        mgk_64_device_modules.remove("drivers/media/platform/mtk-vcodec/mtk-vcodec-dec-v2.ko")
        mgk_64_device_modules.remove("drivers/media/platform/mtk-vcodec/mtk-vcodec-enc-v2.ko")
        mgk_64_device_modules.remove("drivers/soc/mediatek/mtk-mmdvfs-v3.ko")
        mgk_64_platform_device_modules.pop("drivers/clk/mediatek/clk-chk-mt6989.ko")
        mgk_64_platform_device_modules.pop("drivers/clk/mediatek/pd-chk-mt6989.ko")
        mgk_64_platform_device_modules.pop("drivers/clk/mediatek/clk-chk-mt6991.ko")
        mgk_64_platform_device_modules.pop("drivers/clk/mediatek/pd-chk-mt6991.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/performance/mtk_perf_ioctl_magt.ko")

        mgk_64_device_modules.append("drivers/misc/mediatek/scp/cm4/scp.ko")
        mgk_64_device_modules.remove("sound/soc/codecs/mt6338-accdet.ko")
        mgk_64_device_modules.remove("sound/soc/codecs/mt6681-accdet.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/apusys/apusys.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/apusys/apu_aov.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/sensor/2.0/sensorhub/sensorhub.ko")
        mgk_64_device_modules.remove("drivers/gpu/drm/mediatek/mediatek_v2/mtk_aod_scp.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/apusys/power/apu_top.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/apusys/sapu/sapu.ko")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/mtkcam/scpsys/mtk-aov:mtk_aov")

        mgk_64_platform_device_modules.pop("drivers/gpu/drm/mediatek/mml/mtk-mml-mt6878.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/drm/mediatek/mml/mtk-mml-mt6886.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/drm/mediatek/mml/mtk-mml-mt6897.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/drm/mediatek/mml/mtk-mml-mt6899.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/drm/mediatek/mml/mtk-mml-mt6983.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/drm/mediatek/mml/mtk-mml-mt6985.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/drm/mediatek/mml/mtk-mml-mt6989.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/drm/mediatek/mml/mtk-mml-mt6991.ko")

        mgk_64_device_modules.remove("drivers/misc/mediatek/trusted_mem/tmem_ffa.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/trusted_mem/ffa_v11.ko")
        mgk_64_device_modules.append("drivers/tee/gud/600/MobiCoreDriver/mcDrvModule.ko")
        mgk_64_device_modules.append("drivers/tee/gud/600/MobiCoreDriver/mcDrvModule-ffa.ko")
        mgk_64_device_modules.append("drivers/tee/gud/600/TlcTui/t-base-tui.ko")
        mgk_64_device_modules.remove("drivers/tee/teei/510/isee-ffa.ko")
        mgk_64_device_modules.append("drivers/tee/teei/515/isee.ko")

        mgk_64_platform_device_modules.pop("drivers/soc/mediatek/devapc/device-apc-mt6761.ko")
        mgk_64_platform_device_modules.pop("drivers/soc/mediatek/devapc/device-apc-mt6765.ko")
        mgk_64_platform_device_modules.pop("drivers/soc/mediatek/devapc/device-apc-mt6768.ko")
        mgk_64_platform_device_modules.pop("drivers/soc/mediatek/devapc/device-apc-mt6833.ko")
        mgk_64_platform_device_modules.pop("drivers/soc/mediatek/devapc/device-apc-mt6853.ko")
        mgk_64_platform_device_modules.pop("drivers/soc/mediatek/devapc/device-apc-mt6877.ko")
        mgk_64_platform_device_modules.pop("drivers/soc/mediatek/devapc/device-apc-mt6879.ko")
        mgk_64_platform_device_modules.pop("drivers/soc/mediatek/devapc/device-apc-mt6893.ko")
        mgk_64_platform_device_modules.pop("drivers/soc/mediatek/devapc/device-apc-mt6895.ko")
        mgk_64_platform_device_modules.pop("drivers/soc/mediatek/devapc/device-apc-mt6985.ko")
        mgk_64_platform_device_modules.pop("drivers/soc/mediatek/devapc/device-apc-mt6989.ko")
        mgk_64_platform_device_modules.pop("drivers/soc/mediatek/devapc/device-apc-mt6991.ko")

        mgk_64_device_modules.append("drivers/misc/mediatek/eem_v2/mediatek_eem.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cpuhotplug/mtk_cpuhp.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cpufreq_v2/src/CPU_DVFS.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/upower/Upower.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/leakage_table_v2/mediatek_static_power.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/ppm_v3/mtk_ppm_v3.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/conn_md/conn_md_drv.ko")

    if "mt6768_overlay.config" in DEFCONFIG_OVERLAYS:
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/gpu:gpu")
        mgk_64_kleaf_modules.append("//vendor/mediatek/kernel_modules/gpu:gpu_mt6768")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/mtkcam/mtk-aie:mtk-aie")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/mtkcam/scpsys/mtk-aov:mtk_aov")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/mtkcam/sched:c2ps")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/mtkcam/sched/c2ps_ioctl:c2ps_perf_ioctl")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/mtkcam/isp_pspm:isp_pspm")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/performance:mtk_perf_ioctl_magt".format(kernel_version))
        mgk_64_common_eng_modules.remove("drivers/firmware/arm_ffa/ffa-module.ko")
        mgk_64_common_userdebug_modules.remove("drivers/firmware/arm_ffa/ffa-module.ko")
        mgk_64_common_user_modules.remove("drivers/firmware/arm_ffa/ffa-module.ko")
        mgk_64_common_eng_modules.remove("drivers/firmware/arm_ffa/ffa-core.ko")
        mgk_64_common_userdebug_modules.remove("drivers/firmware/arm_ffa/ffa-core.ko")
        mgk_64_common_user_modules.remove("drivers/firmware/arm_ffa/ffa-core.ko")
        #mgk_64_device_modules.remove("drivers/misc/mediatek/pkvm_tmem/pkvm_tmem.ko")
        mgk_64_device_modules.append("drivers/tee/gud/600/MobiCoreDriver/mcDrvModule.ko")
        mgk_64_device_modules.append("drivers/tee/gud/600/MobiCoreDriver/mcDrvModule-ffa.ko")
        mgk_64_device_modules.append("drivers/tee/gud/600/TlcTui/t-base-tui.ko")

        mgk_64_device_modules.append("drivers/misc/mediatek/qos/mtk_qos_legacy.ko")

        mgk_64_device_modules.remove("drivers/gpu/drm/mediatek/mediatek_v2/mtk_aod_scp.ko")
        mgk_64_device_modules.remove("drivers/gpu/drm/panel/mediatek-cust-panel-sample.ko")
        mgk_64_device_modules.remove("drivers/gpu/drm/panel/mediatek-drm-gateic.ko")
        mgk_64_device_modules.remove("drivers/gpu/drm/panel/mediatek-drm-panel-drv.ko")

        mgk_64_device_modules.remove("drivers/iommu/arm/arm-smmu-v3/arm_smmu_v3.ko")
        mgk_64_device_modules.remove("drivers/iommu/arm/arm-smmu-v3/mtk-smmuv3-pmu.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/iommu/arm/arm-smmu-v3:mtk-smmuv3-mpam-mon".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/iommu:mtk_smmu_qos".format(kernel_version))

        #mgk_64_device_modules.remove("drivers/media/platform/mtk-aie/mtk_aie.ko")
        #mgk_64_device_modules.remove("drivers/media/platform/mtk-isp/mtk-aov/mtk_aov.ko")
        mgk_64_device_modules.remove("drivers/media/platform/mtk-vcodec/mtk-vcodec-dec-v2.ko")
        mgk_64_device_modules.remove("drivers/media/platform/mtk-vcodec/mtk-vcodec-enc-v2.ko")

        mgk_64_device_modules.remove("drivers/memory/mediatek/emi.ko")
        mgk_64_device_modules.remove("drivers/memory/mediatek/emi-fake-eng.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/memory/mediatek:emi-mpu".format(kernel_version))
        mgk_64_device_modules.remove("drivers/memory/mediatek/emi-mpu-test.ko")
        mgk_64_device_modules.remove("drivers/memory/mediatek/emi-mpu-test-v2.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/memory/mediatek:emi-slb".format(kernel_version))
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
        mgk_64_device_modules.remove("drivers/misc/mediatek/adsp/v3/adsp-v3.ko")

        mgk_64_device_modules.remove("drivers/misc/mediatek/apusys/apusys.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/apusys/apu_aov.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/apusys/power/apu_top.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/apusys/sapu/sapu.ko")

        mgk_64_device_modules.remove("drivers/misc/mediatek/audio_ipi/audio_ipi.ko")

        mgk_64_device_modules.remove("drivers/misc/mediatek/cameraisp/src/isp_6s/camera_isp.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cameraisp/src/isp_4/camera_isp_4_t.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cameraisp/fdvt/camera_fdvt_isp40.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cameraisp/dpe/camera_dpe_isp40.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cameraisp/src/isp_4/cam_qos_4.ko")

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

        mgk_64_kleaf_device_modules.append("//kernel_device_modules-{}/drivers/misc/mediatek/dvfsrc:dvfsrc-opp-mt6768".format(kernel_version))

        mgk_64_device_modules.append("drivers/misc/mediatek/eem_v2/mediatek_eem.ko")

        mgk_64_device_modules.remove("drivers/misc/mediatek/imgsensor/src/isp6s/imgsensor_isp6s.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/imgsensor/src/isp4_t/imgsensor_isp4_t.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/cam_cal/src/custom/camera_eeprom.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cam_cal/src/isp4_t/camera_eeprom_isp4_t.ko")

        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/iommu:smmu_secure".format(kernel_version))
        mgk_64_device_modules.append("drivers/misc/mediatek/iommu/iommu_gz.ko")

        mgk_64_device_modules.append("drivers/misc/mediatek/leakage_table_v2/mediatek_static_power.ko")

        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/bridge/mtk_mbraink_bridge.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/mtk_mbraink.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/modules/v6989/mtk_mbraink_v6989.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/modules/v6991/mtk_mbraink_v6991.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/modules/v6899/mtk_mbraink_v6899.ko")

        mgk_64_device_modules.remove("drivers/misc/mediatek/mcupm/v2/mcupm.ko")

        mgk_64_device_modules.append("drivers/misc/mediatek/mcdi/mcdi.ko")

        mgk_64_device_modules.remove("drivers/misc/mediatek/mminfra/mtk-mminfra-debug.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mminfra/mtk-mminfra-imax.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mminfra/mtk-mminfra-util.ko")

        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/pmsr:pmsr".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/pmsr/v2:pmsr_v2".format(kernel_version))
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
        mgk_64_kleaf_device_modules.append("//kernel_device_modules-{}/drivers/misc/mediatek/sspm/v1:sspm_v1".format(kernel_version))

        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v1/mtk-swpm-dbg-common-v1.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug/v1:mtk-swpm-isp-wrapper".format(kernel_version))
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
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6991/mtk-swpm-dbg-v6991.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6991/mtk-swpm-cpu-dbg-v6991.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6991/mtk-swpm-mml-dbg-v6991.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6991/mtk-swpm-isp-dbg-v6991.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6991/mtk-swpm-audio-dbg-v6991.ko")

        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/mtk-swpm.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm:mtk-swpm-perf-arm-pmu".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/tinysys_scmi:tinysys-scmi".format(kernel_version))

        mgk_64_device_modules.remove("drivers/misc/mediatek/trusted_mem/tmem_ffa.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/trusted_mem:ffa".format(kernel_version))

        mgk_64_device_modules.append("drivers/misc/mediatek/upower/Upower.ko")

        mgk_64_device_modules.remove("drivers/misc/mediatek/usb/usb_offload/usb_offload.ko")

        mgk_64_device_modules.remove("drivers/misc/mediatek/vcp/rv/vcp.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/vcp/rv/vcp_status.ko")

        mgk_64_device_modules.remove("drivers/misc/mediatek/vdec_fmt/vdec-fmt.ko")

        mgk_64_device_modules.remove("drivers/misc/mediatek/vow/ver02/mtk-vow.ko")

        mgk_64_device_modules.append("drivers/power/supply/bq2589x_charger.ko")

        mgk_64_device_modules.append("drivers/regulator/mt6358-regulator.ko")
        mgk_64_device_modules.remove("drivers/spmi/spmi-mtk-mpu.ko")
        mgk_64_device_modules.remove("drivers/spmi/spmi-mtk-pmif.ko")

        mgk_64_device_modules.remove("drivers/soc/mediatek/mtk-mmdvfs-v3.ko")
        mgk_64_device_modules.remove("drivers/soc/mediatek/mmdvfs/mtk-mmdvfs-v5.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mmdvfs/mtk-mmdvfs-debug-v5.ko")
        mgk_64_platform_device_modules.pop("drivers/soc/mediatek/mmdvfs/mmdvfs-mt6991.ko")

        mgk_64_device_modules.remove("drivers/tee/teei/510/isee-ffa.ko")
        mgk_64_device_modules.append("drivers/tee/teei/515/isee.ko")

        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/thermal/mediatek:backlight_cooling".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/thermal/mediatek:board_temp".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/thermal/mediatek:charger_cooling".format(kernel_version))
        if "drivers/thermal/mediatek/md_cooling_all.ko" in mgk_64_device_modules:
            mgk_64_device_modules.remove("drivers/thermal/mediatek/md_cooling_all.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/thermal/mediatek:pmic_temp".format(kernel_version))
        mgk_64_device_modules.remove("drivers/thermal/mediatek/soc_temp_lvts.ko")
        mgk_64_device_modules.remove("drivers/thermal/mediatek/thermal_interface.ko")
        #mgk_64_device_modules.remove("drivers/thermal/mediatek/thermal_jatm.ko")
        mgk_64_device_modules.remove("drivers/thermal/mediatek/thermal_trace.ko")
        mgk_64_device_modules.remove("drivers/thermal/mediatek/vtskin_temp.ko")
        mgk_64_device_modules.remove("drivers/thermal/mediatek/wifi_cooling.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/trusty:trusty-core".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/trusty:trusty-ipc".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/trusty:trusty-log".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/trusty:trusty-test".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/trusty:trusty-virtio".format(kernel_version))
        mgk_64_device_modules.remove("sound/soc/codecs/mt6338-accdet.ko")
        mgk_64_device_modules.remove("sound/soc/codecs/snd-soc-mt6338.ko")
        mgk_64_device_modules.remove("sound/soc/codecs/snd-soc-mt6368.ko")
        mgk_64_device_modules.remove("sound/soc/mediatek/audio_dsp/mtk-soc-offload-common.ko")
        mgk_64_device_modules.remove("sound/soc/mediatek/audio_dsp/snd-soc-audiodsp-common.ko")
        mgk_64_device_modules.remove("sound/soc/mediatek/ultrasound/ultra_common/mtk-scp-ultra.ko")
        mgk_64_device_modules.remove("sound/soc/mediatek/ultrasound/ultra_scp/snd-soc-mtk-scp-ultra.ko")
        mgk_64_device_modules.remove("sound/soc/mediatek/vow/mtk-scp-vow.ko")
        mgk_64_device_modules.append("sound/soc/codecs/snd-soc-mt6660.ko")
        mgk_64_device_modules.append("sound/soc/codecs/fs18xx/snd-soc-fsm.ko")
        mgk_64_device_modules.append("sound/soc/codecs/snd-soc-mt6358.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/spi_slave_drv/spi_slave.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpu_pdma/mtk_gpu_pdma_mt6991.ko")
        mgk_64_device_modules.remove("drivers/gpu/mediatek/gpueb/mtk_gpueb.ko")
        mgk_64_device_modules.remove("drivers/gpu/mediatek/gpueb/mtk_ghpm.ko")
        mgk_64_device_modules.remove("drivers/gpu/mediatek/gpueb/mtk_ghpm_swwa.ko")
        mgk_64_device_modules.remove("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_wrapper.ko")
        mgk_64_device_modules.append("drivers/gpu/mediatek/gpufreq/v2_legacy/mtk_gpufreq_wrapper_legacy.ko")

        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/power/supply:mt6375-charger".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/power/supply:rt9490-charger".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/power/supply:rt9758-charger".format(kernel_version))

        mgk_64_platform_device_modules.update({"drivers/clk/mediatek/clkchk-mt6768.ko":"mt6768"})
        mgk_64_platform_device_modules.update({"drivers/clk/mediatek/clkdbg-mt6768.ko":"mt6768"})
        mgk_64_platform_device_modules.update({"drivers/clk/mediatek/clk-fmeter-mt6768.ko":"mt6768"})
        mgk_64_platform_device_modules.update({"drivers/clk/mediatek/clk-mt6768.ko":"mt6768"})
        mgk_64_platform_device_modules.update({"drivers/clk/mediatek/clk-mt6768-pg.ko":"mt6768"})
        mgk_64_platform_device_modules.update({"drivers/clk/mediatek/pd-chk-mt6768.ko":"mt6768"})
        mgk_64_platform_device_modules.update({"drivers/power/supply/mm8013.ko":"mt6768"})
        mgk_64_platform_device_modules.pop("drivers/clk/mediatek/clk-chk-mt6991.ko")
        mgk_64_platform_device_modules.pop("drivers/clk/mediatek/pd-chk-mt6991.ko")

        mgk_64_platform_device_modules.pop("drivers/gpu/drm/mediatek/mml/mtk-mml-mt6878.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/drm/mediatek/mml/mtk-mml-mt6886.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/drm/mediatek/mml/mtk-mml-mt6897.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/drm/mediatek/mml/mtk-mml-mt6899.ko")
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
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/cm_mgr/mtk_cm_mgr_mt6991.ko")
        mgk_64_platform_device_modules.update({"drivers/misc/mediatek/cm_mgr_legacy_v1/mtk_cm_mgr_mt6768.ko":"mt6768"})
        mgk_64_platform_device_modules.update({"drivers/misc/mediatek/cm_mgr_legacy_v1/mtk_cm_mgr_mt6893.ko":"mt6893"})

        mgk_64_kleaf_platform_modules.update({"//kernel_device_modules-{}/drivers/pinctrl/mediatek:pinctrl-mt6768".format(kernel_version): "mt6768"})

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

        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/memory/mediatek:slc-parity".format(kernel_version))
        mgk_64_device_modules.remove("drivers/misc/mediatek/ise_lpm/ise_lpm.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/ise_trusty/ise-trusty.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/ise_trusty/ise-trusty-ipc.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/ise_trusty/ise-trusty-log.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/ise_trusty/ise-trusty-virtio.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/lpm/governors/MHSP/lpm-gov-MHSP.ko")
        #mgk_64_device_modules.remove("drivers/misc/mediatek/lpm/modules/debug/v1/mtk-lpm-dbg-common-v1.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/lpm/modules/debug:mtk-lpm-dbg-common-v2".format(kernel_version))
        #mgk_64_device_modules.remove("drivers/misc/mediatek/lpm/modules/platform/v1/mtk-lpm-plat-v1.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/lpm/modules/platform/v2:mtk-lpm-plat-v2".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/lpm:mtk-lpm".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/mdpm:mtk_mdpm".format(kernel_version))
        mgk_64_device_modules.append("drivers/misc/mediatek/mdpm_v1/mtk_mdpm_v1.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/slbc/mmsram.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/slbc:mtk_slbc".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/slbc:slbc_ipi".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/slbc:slbc_trace".format(kernel_version))
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6897/mtk-swpm-audio-dbg-v6897.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6989/mtk-swpm-audio-dbg-v6989.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6989/mtk-swpm-core-dbg-v6989.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6989/mtk-swpm-cpu-dbg-v6989.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6989/mtk-swpm-dbg-v6989.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6989/mtk-swpm-disp-dbg-v6989.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6989/mtk-swpm-mem-dbg-v6989.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6989/mtk-swpm-mml-dbg-v6989.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6991/mtk-swpm-disp-dbg-v6991.ko")
        mgk_64_device_modules.remove("sound/soc/codecs/mt6681-accdet.ko")
        mgk_64_device_modules.remove("sound/soc/codecs/snd-soc-mt6359.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/usb/usb_sram/usb_sram.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/ise_lpm/ise_lpm_v2.ko")

        mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6886.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6897.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6899.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6985.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6989.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6989_fpga.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6991.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6993.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/cm_mgr/mtk_cm_mgr_mt6989.ko")
        #mgk_64_platform_device_modules.pop("drivers/misc/mediatek/lpm/modules/debug/mt6886/mtk-lpm-dbg-mt6886.ko")
        #mgk_64_platform_device_modules.pop("drivers/misc/mediatek/lpm/modules/debug/mt6897/mtk-lpm-dbg-mt6897.ko")
        #mgk_64_platform_device_modules.pop("drivers/misc/mediatek/lpm/modules/debug/mt6983/mtk-lpm-dbg-mt6983.ko")
        #mgk_64_platform_device_modules.pop("drivers/misc/mediatek/lpm/modules/debug/mt6985/mtk-lpm-dbg-mt6985.ko")
        #mgk_64_platform_device_modules.pop("drivers/misc/mediatek/lpm/modules/debug/mt6989/mtk-lpm-dbg-mt6989.ko")
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/lpm/modules/debug/mt6991:mtk-lpm-dbg-mt6991".format(kernel_version))
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/slbc/slbc_mt6886.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/slbc/slbc_mt6893.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/slbc/slbc_mt6895.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/slbc/slbc_mt6897.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/slbc/slbc_mt6899.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/slbc/slbc_mt6983.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/slbc/slbc_mt6985.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/slbc/slbc_mt6989.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/slbc/slbc_mt6991.ko")
        mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6885/snd-soc-mt6885-afe.ko")
        mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6885/mt6885-mt6359p.ko")
        mgk_64_platform_device_modules.pop("drivers/clk/mediatek/clk-chk-mt6989.ko")
        mgk_64_platform_device_modules.pop("drivers/clk/mediatek/pd-chk-mt6989.ko")
        mgk_64_platform_device_modules.pop("drivers/soc/mediatek/devapc/device-apc-mt6833.ko")
        mgk_64_platform_device_modules.pop("drivers/soc/mediatek/devapc/device-apc-mt6853.ko")
        mgk_64_platform_device_modules.pop("drivers/soc/mediatek/devapc/device-apc-mt6877.ko")
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
        mgk_64_kleaf_eng_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase:ktf_testcase")
        mgk_64_kleaf_userdebug_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase:ktf_testcase")
        mgk_64_kleaf_eng_modules.append("//vendor/mediatek/tests/kernel/ktf_testcase:ktf_testcase_k61")
        mgk_64_kleaf_userdebug_modules.append("//vendor/mediatek/tests/kernel/ktf_testcase:ktf_testcase_k61")

        mgk_64_platform_device_modules.update({"drivers/regulator/mt6357-regulator.ko":"mt6761"})
        mgk_64_kleaf_platform_modules.update({"//kernel_device_modules-{}/drivers/pinctrl/mediatek:pinctrl-mt6761".format(kernel_version): "mt6761"})
        mgk_64_platform_device_modules.update({"drivers/clk/mediatek/clkchk-mt6761.ko":"mt6761"})
        mgk_64_platform_device_modules.update({"drivers/clk/mediatek/clk-mt6761-audio.ko":"mt6761"})
        mgk_64_platform_device_modules.update({"drivers/clk/mediatek/clk-mt6761-cam.ko":"mt6761"})
        mgk_64_platform_device_modules.update({"drivers/clk/mediatek/clk-mt6761-mipi0a.ko":"mt6761"})
        mgk_64_platform_device_modules.update({"drivers/clk/mediatek/clk-mt6761-mm.ko":"mt6761"})
        mgk_64_common_eng_modules.remove("drivers/firmware/arm_ffa/ffa-module.ko")
        mgk_64_common_userdebug_modules.remove("drivers/firmware/arm_ffa/ffa-module.ko")
        mgk_64_common_user_modules.remove("drivers/firmware/arm_ffa/ffa-module.ko")
        mgk_64_device_modules.append("drivers/tee/gud/600/MobiCoreDriver/mcDrvModule.ko")
        mgk_64_device_modules.append("drivers/tee/gud/600/MobiCoreDriver/mcDrvModule-ffa.ko")
        mgk_64_device_modules.remove("drivers/spmi/spmi-mtk-mpu.ko")
        mgk_64_device_modules.remove("drivers/spmi/spmi-mtk-pmif.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/trusted_mem/tmem_ffa.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/trusted_mem/ffa_v11.ko")
        mgk_64_kleaf_device_modules.update("//kernel_device_modules-{}/drivers/power/supply:rt9465".format(kernel_version))
        mgk_64_platform_device_modules.update({"drivers/clk/mediatek/clk-mt6761-vcodec.ko":"mt6761"})
        mgk_64_platform_device_modules.update({"drivers/clk/mediatek/clkdbg-mt6761.ko":"mt6761"})
        mgk_64_platform_device_modules.update({"drivers/clk/mediatek/clk-fmeter-mt6761.ko":"mt6761"})
        mgk_64_platform_device_modules.update({"drivers/clk/mediatek/clk-mt6761.ko":"mt6761"})
        mgk_64_platform_device_modules.update({"drivers/clk/mediatek/clk-mt6761-pg.ko":"mt6761"})
        mgk_64_platform_device_modules.update({"drivers/clk/mediatek/pd-chk-mt6761.ko":"mt6761"})
        mgk_64_platform_device_modules.update({"drivers/misc/mediatek/leakage_table_v2/mediatek_static_power.ko":"mt6761"})
        mgk_64_platform_device_modules.update({"drivers/misc/mediatek/upower/Upower.ko":"mt6761"})
        mgk_64_platform_device_modules.update({"drivers/misc/mediatek/ppm_v3/mtk_ppm_v3.ko":"mt6761"})
        mgk_64_platform_device_modules.update({"drivers/misc/mediatek/cpufreq_v2/src/CPU_DVFS.ko":"mt6761"})
        mgk_64_platform_device_modules.update({"drivers/misc/mediatek/eem_v2/mediatek_eem.ko":"mt6761"})
        mgk_64_platform_device_modules.update({"drivers/misc/mediatek/dvfsrc/dvfsrc-opp-mt6761.ko":"mt6761"})
        mgk_64_platform_device_modules.update({"drivers/soc/mediatek/mtk-dvfsrc.ko":"mt6761"})
        mgk_64_platform_device_modules.update({"drivers/regulator/mtk-dvfsrc-regulator.ko":"mt6761"})
        mgk_64_platform_device_modules.update({"drivers/interconnect/mediatek/mtk-emi.ko":"mt6761"})
        mgk_64_platform_device_modules.update({"drivers/misc/mediatek/dvfsrc/mtk-dvfsrc-helper.ko":"mt6761"})
        mgk_64_platform_device_modules.update({"drivers/devfreq/mtk-dvfsrc-devfreq.ko":"mt6761"})
        mgk_64_platform_device_modules.update({"drivers/soc/mediatek/mtk-dvfsrc-start.ko":"mt6761"})
        mgk_64_platform_device_modules.update({"drivers/misc/mediatek/qos/mtk_qos_legacy.ko":"mt6761"})
        mgk_64_platform_device_modules.update({"drivers/interconnect/mediatek/mmqos-mt6761.ko":"mt6761"})
        mgk_64_platform_device_modules.update({"drivers/power/supply/mt6357-charger-type.ko":"mt6761"})
        mgk_64_platform_device_modules.pop("drivers/gpu/drm/mediatek/mml/mtk-mml-mt6878.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/drm/mediatek/mml/mtk-mml-mt6886.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/drm/mediatek/mml/mtk-mml-mt6897.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/drm/mediatek/mml/mtk-mml-mt6899.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/drm/mediatek/mml/mtk-mml-mt6983.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/drm/mediatek/mml/mtk-mml-mt6985.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/drm/mediatek/mml/mtk-mml-mt6989.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/drm/mediatek/mml/mtk-mml-mt6991.ko")
        mgk_64_device_modules.remove("drivers/gpu/drm/mediatek/mediatek_v2/mtk_disp_sec.ko")
        mgk_64_device_modules.remove("drivers/gpu/mediatek/gpueb/mtk_gpueb.ko")
        mgk_64_device_modules.remove("drivers/gpu/mediatek/gpueb/mtk_ghpm.ko")
        mgk_64_device_modules.remove("drivers/gpu/mediatek/gpueb/mtk_ghpm_swwa.ko")
        mgk_64_device_modules.remove("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_wrapper.ko")
        mgk_64_device_modules.append("drivers/gpu/mediatek/gpufreq/v2_legacy/mtk_gpufreq_wrapper_legacy.ko")
        mgk_64_platform_device_modules.update({"drivers/gpu/mediatek/gpufreq/v2_legacy/mtk_gpufreq_mt6761.ko":"mt6761"})
        mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6886.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6897.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6899.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6985.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6989.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6989_fpga.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6991.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6993.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpu_pdma/mtk_gpu_pdma_mt6991.ko")
        #mgk_64_platform_device_modules.pop("drivers/misc/mediatek/lpm/modules/debug/mt6886/mtk-lpm-dbg-mt6886.ko")
        #mgk_64_platform_device_modules.pop("drivers/misc/mediatek/lpm/modules/debug/mt6897/mtk-lpm-dbg-mt6897.ko")
        #mgk_64_platform_device_modules.pop("drivers/misc/mediatek/lpm/modules/debug/mt6983/mtk-lpm-dbg-mt6983.ko")
        #mgk_64_platform_device_modules.pop("drivers/misc/mediatek/lpm/modules/debug/mt6985/mtk-lpm-dbg-mt6985.ko")
        #mgk_64_platform_device_modules.pop("drivers/misc/mediatek/lpm/modules/debug/mt6989/mtk-lpm-dbg-mt6989.ko")
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/lpm/modules/debug/mt6991:mtk-lpm-dbg-mt6991".format(kernel_version))
        mgk_64_device_modules.remove("drivers/iommu/arm/arm-smmu-v3/arm_smmu_v3.ko")
        mgk_64_device_modules.remove("drivers/iommu/arm/arm-smmu-v3/mtk-smmuv3-pmu.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/iommu/arm/arm-smmu-v3:mtk-smmuv3-mpam-mon".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/iommu:mtk_smmu_qos".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/iommu:smmu_secure".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/iommu:iommu_secure".format(kernel_version))
        mgk_64_device_modules.remove("drivers/misc/mediatek/trusted_mem/trusted_mem.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/iommu/iommu_gz.ko")
        mgk_64_device_modules.remove("drivers/dma-buf/heaps/mtk_sec_heap.ko")
        mgk_64_device_modules.remove("drivers/tee/gud/610/TlcTui/t-base-tui.ko")

        mgk_64_device_modules.remove("drivers/misc/mediatek/slbc/mmsram.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/slbc:mtk_slbc".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/slbc:slbc_ipi".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/slbc:slbc_trace".format(kernel_version))
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/slbc/slbc_mt6886.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/slbc/slbc_mt6893.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/slbc/slbc_mt6895.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/slbc/slbc_mt6897.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/slbc/slbc_mt6899.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/slbc/slbc_mt6983.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/slbc/slbc_mt6985.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/slbc/slbc_mt6989.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/slbc/slbc_mt6991.ko")

        mgk_64_device_modules.append("drivers/misc/mediatek/thermal/thermal_monitor.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/hps_v3/mtk_cpuhp.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/power_gs_v1/mtk_power_gs_v1.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cm_mgr_legacy_v0/mtk_cm_mgr_v0.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cpuidle/mtk_cpuidle.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/mcdi/mcdi.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/spm/common_v0/MTK_INTERNAL_SPM.ko")

        mgk_64_device_modules.remove("drivers/memory/mediatek/emi.ko")
        mgk_64_device_modules.remove("drivers/memory/mediatek/emi-fake-eng.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/memory/mediatek:emi-mpu".format(kernel_version))
        mgk_64_device_modules.remove("drivers/memory/mediatek/emi-mpu-test.ko")
        mgk_64_device_modules.remove("drivers/memory/mediatek/emi-mpu-test-v2.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/memory/mediatek:emi-slb".format(kernel_version))
        mgk_64_device_modules.remove("drivers/memory/mediatek/smpu.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/ise_lpm/ise_lpm.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/ise_trusty/ise-trusty.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/ise_trusty/ise-trusty-ipc.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/ise_trusty/ise-trusty-log.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/ise_trusty/ise-trusty-virtio.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/lpm/governors/MHSP/lpm-gov-MHSP.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/lpm/modules/debug/v1/mtk-lpm-dbg-common-v1.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/lpm/modules/debug/v2/mtk-lpm-dbg-common-v2.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/lpm/modules/platform/v1/mtk-lpm-plat-v1.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/lpm/modules/platform/v2/mtk-lpm-plat-v2.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/lpm/mtk-lpm.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/bridge/mtk_mbraink_bridge.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/mtk_mbraink.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/memory/mediatek:slc-parity".format(kernel_version))
        mgk_64_device_modules.remove("drivers/memory/mediatek/smpu-hook-v1.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mminfra/mtk-mminfra-debug.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mminfra/mtk-mminfra-imax.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi_legacy_v1/emicen.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi_legacy_v1/emiisu.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi_legacy_v1/emimpu.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi_legacy_v1/emictrl.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi-dummy.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/smi/mtk-smi-bwc.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/videocodec/vcodec_kernel_common_driver.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/videocodec/vcodec_kernel_driver-v1.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/vcp/rv/vcp.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/vcp/rv/vcp_status.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/vdec_fmt/vdec-fmt.ko")
        mgk_64_device_modules.remove("drivers/media/platform/mtk-vcodec/mtk-vcodec-dec-v2.ko")
        mgk_64_device_modules.remove("drivers/media/platform/mtk-vcodec/mtk-vcodec-enc-v2.ko")
        mgk_64_device_modules.remove("drivers/media/platform/mtk-vcodec/mtk-vcodec-dec-v1.ko")
        mgk_64_device_modules.remove("drivers/media/platform/mtk-vcodec/mtk-vcodec-enc-v1.ko")
        mgk_64_device_modules.remove("drivers/media/platform/mtk-vcodec/mtk-vcodec-common.ko")
        mgk_64_device_modules.remove("drivers/media/platform/mtk-vcu/mtk-vcu.ko")
        mgk_64_device_modules.remove("drivers/soc/mediatek/mtk-mmdvfs-v3.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/qos/mtk_qos.ko")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/cpufreq_cus:cpu_freq")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/cpufreq_int:cpu_hwtest")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/mtkcam/scpsys/mtk-aov:mtk_aov")
        mgk_64_platform_device_modules.pop("drivers/clk/mediatek/clk-chk-mt6989.ko")
        mgk_64_platform_device_modules.pop("drivers/clk/mediatek/pd-chk-mt6989.ko")
        mgk_64_platform_device_modules.pop("drivers/clk/mediatek/clk-chk-mt6991.ko")
        mgk_64_platform_device_modules.pop("drivers/clk/mediatek/pd-chk-mt6991.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/modules/v6989/mtk_mbraink_v6989.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/modules/v6991/mtk_mbraink_v6991.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/modules/v6899/mtk_mbraink_v6899.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/apusys/apusys.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/apusys/apu_aov.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/apusys/power/apu_top.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/apusys/sapu/sapu.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/sensor/2.0/sensorhub/sensorhub.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/sspm/v3/sspm_v3.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/sspm/v1/sspm_v1.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/tinysys_scmi/tinysys-scmi.ko")

        mgk_64_device_modules.remove("drivers/misc/mediatek/imgsensor/src/isp6s/imgsensor_isp6s.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/imgsensor/src/isp3_m/imgsensor_isp3_m.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/cam_cal/src/custom/camera_eeprom.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cam_cal/src/isp3_m/camera_eeprom_isp3_m.ko")

        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v1/mtk-swpm-dbg-common-v1.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug/v1:mtk-swpm-isp-wrapper".format(kernel_version))
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
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6991/mtk-swpm-audio-dbg-v6991.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6991/mtk-swpm-dbg-v6991.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6991/mtk-swpm-cpu-dbg-v6991.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6991/mtk-swpm-mml-dbg-v6991.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6991/mtk-swpm-isp-dbg-v6991.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/mtk-swpm.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm:mtk-swpm-perf-arm-pmu".format(kernel_version))
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6897/mtk-swpm-audio-dbg-v6897.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6989/mtk-swpm-audio-dbg-v6989.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6989/mtk-swpm-core-dbg-v6989.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6989/mtk-swpm-cpu-dbg-v6989.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6989/mtk-swpm-dbg-v6989.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6989/mtk-swpm-disp-dbg-v6989.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6989/mtk-swpm-mem-dbg-v6989.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6989/mtk-swpm-mml-dbg-v6989.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6991/mtk-swpm-disp-dbg-v6991.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/pmsr:pmsr".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/pmsr/v2:pmsr_v2".format(kernel_version))
        mgk_64_device_modules.remove("drivers/misc/mediatek/pmsr/v3/pmsr_v3.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/cm_mgr/mtk_cm_ipi.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/cm_mgr/mtk_cm_mgr.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/cm_mgr/mtk_cm_mgr_mt6989.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/cm_mgr/mtk_cm_mgr_mt6886.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/cm_mgr/mtk_cm_mgr_mt6897.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/cm_mgr/mtk_cm_mgr_mt6983.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/cm_mgr/mtk_cm_mgr_mt6985.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/cm_mgr/mtk_cm_mgr_mt6991.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/ssc/debug/v1/mtk-ssc-dbg-v1.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/ssc/debug/v2/mtk-ssc-dbg-v2.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/ssc/mtk-ssc.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mcupm/v2/mcupm.ko")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/gpu:gpu")
        mgk_64_kleaf_modules.append("//vendor/mediatek/kernel_modules/gpu:gpu_mt6761")

        mgk_64_device_modules.remove("drivers/tee/teei/510/isee-ffa.ko")
        mgk_64_device_modules.append("drivers/tee/teei/515/isee.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/conn_md/conn_md_drv.ko")

        mgk_64_device_modules.remove("drivers/misc/mediatek/cameraisp/src/isp_6s/camera_isp.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cameraisp/src/isp_3/camera_isp_3_m.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cameraisp/src/isp_3/cam_qos_3.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cameraisp/fdvt/camera_fdvt_isp35.ko")

        mgk_64_device_modules.remove("sound/soc/codecs/snd-soc-mt6338.ko")
        mgk_64_device_modules.remove("sound/soc/codecs/snd-soc-mt6368.ko")
        mgk_64_device_modules.remove("sound/soc/mediatek/audio_dsp/mtk-soc-offload-common.ko")
        mgk_64_device_modules.remove("sound/soc/mediatek/audio_dsp/snd-soc-audiodsp-common.ko")
        mgk_64_device_modules.remove("sound/soc/mediatek/ultrasound/ultra_common/mtk-scp-ultra.ko")
        mgk_64_device_modules.remove("sound/soc/mediatek/ultrasound/ultra_scp/snd-soc-mtk-scp-ultra.ko")
        mgk_64_device_modules.remove("sound/soc/mediatek/vow/mtk-scp-vow.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/adsp/adsp.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/adsp/v1/adsp-v1.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/adsp/v2/adsp-v2.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/adsp/v3/adsp-v3.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/audio_ipi/audio_ipi.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/usb/usb_offload/usb_offload.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/vow/ver02/mtk-vow.ko")
        mgk_64_device_modules.remove("sound/soc/codecs/snd-soc-mt6359.ko")
        mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6886/mt6886-mt6368.ko")
        mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6886/snd-soc-mt6886-afe.ko")
        mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6897/mt6897-mt6368.ko")
        mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6897/snd-soc-mt6897-afe.ko")
        mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6983/mt6983-mt6338.ko")
        mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6983/snd-soc-mt6983-afe.ko")
        mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6985/mt6985-mt6338.ko")
        mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6985/snd-soc-mt6985-afe.ko")
        mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6885/snd-soc-mt6885-afe.ko")
        mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6885/mt6885-mt6359p.ko")

        mgk_64_device_modules.append("sound/soc/mediatek/codec/snd-mtk-soc-codec-6357.ko")
        mgk_64_device_modules.append("sound/soc/codecs/snd-soc-rt5509.ko")

        mgk_64_device_modules.remove("drivers/misc/mediatek/mdpm/mtk_mdpm.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/mdpm_v1/mtk_mdpm_v1.ko")
        mgk_64_platform_device_modules.update({"drivers/misc/mediatek/dcm/mt6761_dcm.ko":"mt6761"})

        mgk_64_device_modules.remove("drivers/misc/mediatek/widevine_drm/widevine_driver.ko")

        mgk_64_platform_device_modules.update({"sound/soc/mediatek/common_int/mtk-soc-sound.ko":"mt6761"})
        mgk_64_platform_device_modules.update({"sound/soc/mediatek/common_int/mtk-auddrv-gpio.ko":"mt6761"})
        mgk_64_platform_device_modules.update({"sound/soc/mediatek/common_int/mtk-soc-speaker-amp.ko":"mt6761"})
        mgk_64_platform_device_modules.update({"sound/soc/mediatek/common_int/mtk-soc-sound-cycle-dependent.ko":"mt6761"})
        mgk_64_platform_device_modules.update({"sound/soc/mediatek/common_int/mtk-soc-pcm-capture.ko":"mt6761"})
        mgk_64_platform_device_modules.update({"sound/soc/mediatek/common_int/mtk-soc-pcm-dl1.ko":"mt6761"})
        mgk_64_platform_device_modules.update({"sound/soc/mediatek/common_int/mtk-soc-pcm-routing.ko":"mt6761"})
        mgk_64_platform_device_modules.update({"sound/soc/mediatek/common_int/mtk-soc-pcm-capture2.ko":"mt6761"})
        mgk_64_platform_device_modules.update({"sound/soc/mediatek/common_int/mtk-soc-pcm-i2s2-adc2.ko":"mt6761"})
        mgk_64_platform_device_modules.update({"sound/soc/mediatek/common_int/mtk-soc-pcm-voice-usb-echoref.ko":"mt6761"})
        mgk_64_platform_device_modules.update({"sound/soc/mediatek/common_int/mtk-soc-pcm-dl1-i2s0Dl1.ko":"mt6761"})
        mgk_64_platform_device_modules.update({"sound/soc/mediatek/common_int/mtk-soc-pcm-i2s0-awb.ko":"mt6761"})
        mgk_64_platform_device_modules.update({"sound/soc/mediatek/common_int/mtk-soc-pcm-uldlloopback.ko":"mt6761"})
        mgk_64_platform_device_modules.update({"sound/soc/mediatek/common_int/mtk-soc-pcm-deep-buffer-dl.ko":"mt6761"})
        mgk_64_platform_device_modules.update({"sound/soc/mediatek/common_int/mtk-soc-pcm-mrgrx.ko":"mt6761"})
        mgk_64_platform_device_modules.update({"sound/soc/mediatek/common_int/mtk-soc-pcm-mrgrx-awb.ko":"mt6761"})
        mgk_64_platform_device_modules.update({"sound/soc/mediatek/common_int/mtk-soc-pcm-fm-i2s.ko":"mt6761"})
        mgk_64_platform_device_modules.update({"sound/soc/mediatek/common_int/mtk-soc-pcm-fm-i2s-awb.ko":"mt6761"})
        mgk_64_platform_device_modules.update({"sound/soc/mediatek/common_int/mtk-soc-pcm-dl1-awb.ko":"mt6761"})
        mgk_64_platform_device_modules.update({"sound/soc/mediatek/common_int/mtk-soc-pcm-dl1-bt.ko":"mt6761"})
        mgk_64_platform_device_modules.update({"sound/soc/mediatek/common_int/mtk-soc-pcm-bt-dai.ko":"mt6761"})
        mgk_64_platform_device_modules.update({"sound/soc/mediatek/common_int/mtk-soc-dai-stub.ko":"mt6761"})
        mgk_64_platform_device_modules.update({"sound/soc/mediatek/common_int/mtk-soc-dai-routing.ko":"mt6761"})
        mgk_64_platform_device_modules.update({"sound/soc/mediatek/common_int/mtk-soc-codec-dummy.ko":"mt6761"})
        mgk_64_platform_device_modules.update({"sound/soc/mediatek/common_int/mtk-soc-pcm-fmtx.ko":"mt6761"})
        mgk_64_platform_device_modules.update({"sound/soc/mediatek/common_int/mtk-soc-tdm-capture.ko":"mt6761"})
        mgk_64_platform_device_modules.update({"sound/soc/mediatek/common_int/mtk-soc-pcm-hp-impedance.ko":"mt6761"})
        mgk_64_platform_device_modules.update({"sound/soc/mediatek/common_int/mtk-soc-machine.ko":"mt6761"})
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/mtkcam/isp_pspm:isp_pspm")
        mgk_64_device_modules.append("drivers/misc/mediatek/scp/cm4/scp.ko")
        mgk_64_device_modules.remove("sound/soc/codecs/mt6338-accdet.ko")
        mgk_64_device_modules.remove("sound/soc/codecs/mt6681-accdet.ko")
        mgk_64_device_modules.remove("drivers/gpu/drm/mediatek/mediatek_v2/mtk_aod_scp.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/pkvm_tmem/pkvm_tmem.ko")
        mgk_64_device_modules.append("drivers/mmc/host/mtk-mmc-swcqhci.ko")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/mtkcam/sched:c2ps")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/mtkcam/sched/c2ps_ioctl:c2ps_perf_ioctl")
        mgk_64_common_eng_modules.remove("drivers/perf/arm_dsu_pmu.ko")
        mgk_64_common_userdebug_modules.remove("drivers/perf/arm_dsu_pmu.ko")
        mgk_64_common_user_modules.remove("drivers/perf/arm_dsu_pmu.ko")


    if "mt6768_overlay_ref.config" in DEFCONFIG_OVERLAYS:
        mgk_64_device_modules.append("drivers/misc/mediatek/flashlight/flashlights-ocp81375.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/typec/mux/usb_dp_selector.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/typec/mux:fusb304".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/typec/mux:mux_switch".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/typec/mux:ps5169".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/typec/mux:ps5170".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/typec/mux:ptn36241g".format(kernel_version))

        mgk_64_device_modules.append("drivers/power/supply/mtk_chg_det.ko")
        mgk_64_device_modules.remove("drivers/power/supply/mtk_hvbpc.ko")
        mgk_64_device_modules.remove("drivers/power/supply/mtk_pep.ko")
        mgk_64_device_modules.remove("drivers/power/supply/mtk_pep20.ko")
        mgk_64_device_modules.remove("drivers/power/supply/mtk_pep40.ko")
        mgk_64_device_modules.remove("drivers/power/supply/mtk_pep45.ko")
        mgk_64_device_modules.remove("drivers/power/supply/mtk_pep50.ko")
        mgk_64_device_modules.remove("drivers/power/supply/mtk_pep50p.ko")
        mgk_64_device_modules.remove("drivers/power/supply/mtk_2p_charger.ko")
        mgk_64_device_modules.append("sound/soc/mediatek/mt6768/mt6768-mt6358-ref.ko")

        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/typec/tcpc:pd_dbg_info".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/typec/tcpc:rt_pd_manager".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/typec/tcpc:tcpc_class".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/typec/tcpc:tcpc_mt6360".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/typec/tcpc:tcpc_mt6370".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/typec/tcpc:tcpc_mt6375".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/typec/tcpc:tcpc_mt6379".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/typec/tcpc:tcpc_rt1711h".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/typec/tcpc:tcpci_late_sync".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/power/supply/ufcs:ufcs_class".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/power/supply/ufcs:ufcs_mt6379".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/power/supply:mt6360_charger".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/power/supply:mt6360_pmu_chg".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/power/supply:mt6370-charger".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/power/supply:mt6379-chg".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/power/supply:mtk_ufcs_adapter".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/power/supply:mtk_chg_type_det".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/power/supply:mtk_pd_adapter".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/power/supply:mtk_pd_charging".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/power/supply:rt9759".format(kernel_version))
        mgk_64_kleaf_device_modules.append("//kernel_device_modules-{}/drivers/power/supply:sgm41516d".format(kernel_version))

    if "mt6893_overlay.config" in DEFCONFIG_OVERLAYS:
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/gpu:gpu")
        mgk_64_kleaf_modules.append("//vendor/mediatek/kernel_modules/gpu:gpu_mt6893")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/mtkcam/sched:c2ps")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/mtkcam/sched/c2ps_ioctl:c2ps_perf_ioctl")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/mtkcam/isp_pspm:isp_pspm")

        mgk_64_platform_device_modules.update({"drivers/gpu/mediatek/gpufreq/v2_legacy/mtk_gpufreq_mt6893.ko":"mt6893"})
        mgk_64_kleaf_platform_modules.update({"//kernel_device_modules-{}/drivers/pinctrl/mediatek:pinctrl-mt6893".format(kernel_version): "mt6893"})
        mgk_64_platform_device_modules.update({"drivers/misc/mediatek/cm_mgr_legacy_v1/mtk_cm_mgr_mt6893.ko":"mt6893"})

        mgk_64_common_eng_modules.remove("drivers/firmware/arm_ffa/ffa-module.ko")
        mgk_64_common_userdebug_modules.remove("drivers/firmware/arm_ffa/ffa-module.ko")
        mgk_64_common_user_modules.remove("drivers/firmware/arm_ffa/ffa-module.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/pkvm_tmem/pkvm_tmem.ko")
        mgk_64_device_modules.append("drivers/tee/gud/600/MobiCoreDriver/mcDrvModule.ko")
        mgk_64_device_modules.append("drivers/tee/gud/600/MobiCoreDriver/mcDrvModule-ffa.ko")
        mgk_64_device_modules.append("drivers/tee/gud/600/TlcTui/t-base-tui.ko")

        mgk_64_common_eng_modules.append("drivers/mfd/mt6360-core.ko")
        mgk_64_common_userdebug_modules.append("drivers/mfd/mt6360-core.ko")
        mgk_64_common_user_modules.append("drivers/mfd/mt6360-core.ko")

        mgk_64_device_modules.append("drivers/misc/mediatek/dcm/mt6885_dcm.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/iommu/iommu_gz.ko")

        mgk_64_device_modules.append("drivers/misc/mediatek/cameraisp/wpe/isp_6s/camera_wpe_isp6s.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cameraisp/fdvt/camera_fdvt_isp51.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cameraisp/dip/isp_6s/camera_dip_isp6s.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cameraisp/rsc/camera_rsc_isp6s.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cameraisp/dpe/camera_dpe_isp60.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cameraisp/mfb/camera_mfb_isp6s.ko")

        mgk_64_device_modules.append("drivers/misc/mediatek/smi/mtk-smi-bwc.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cpuidle/mtk_cpuidle.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cpuhotplug/mtk_cpuhp.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/upower/Upower.ko")
        mgk_64_device_modules.append("drivers/soc/mediatek/devmpu/devmpu.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/eemgpu/mtk_eem.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/leakage_table_v2/mediatek_static_power.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/sspm/v2/sspm.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/mcupm/v1/mcupm.ko")
        mgk_64_device_modules.append("drivers/gpu/mediatek/gpufreq/v2_legacy/mtk_gpufreq_wrapper_legacy.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cpufreq_v2/src/CPU_DVFS.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/thermal/thermal_monitor.ko")


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
        mgk_64_device_modules.remove("drivers/trusty/trusty-core.ko")
        mgk_64_device_modules.remove("drivers/trusty/trusty-ipc.ko")
        mgk_64_device_modules.remove("drivers/trusty/trusty-log.ko")
        mgk_64_device_modules.remove("drivers/trusty/trusty-test.ko")
        mgk_64_device_modules.remove("drivers/trusty/trusty-virtio.ko")

        mgk_64_platform_device_modules.pop("drivers/gpu/drm/mediatek/mml/mtk-mml-mt6878.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/drm/mediatek/mml/mtk-mml-mt6886.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/drm/mediatek/mml/mtk-mml-mt6897.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/drm/mediatek/mml/mtk-mml-mt6899.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/drm/mediatek/mml/mtk-mml-mt6983.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/drm/mediatek/mml/mtk-mml-mt6985.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/drm/mediatek/mml/mtk-mml-mt6989.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/drm/mediatek/mml/mtk-mml-mt6991.ko")

        mgk_64_device_modules.remove("drivers/gpu/drm/panel/mediatek-cust-panel-sample.ko")
        mgk_64_device_modules.remove("drivers/gpu/drm/panel/mediatek-drm-gateic.ko")
        mgk_64_device_modules.remove("drivers/gpu/drm/panel/mediatek-drm-panel-drv.ko")

        mgk_64_device_modules.remove("drivers/gpu/mediatek/gpueb/mtk_gpueb.ko")
        mgk_64_device_modules.remove("drivers/gpu/mediatek/gpueb/mtk_ghpm.ko")
        mgk_64_device_modules.remove("drivers/gpu/mediatek/gpueb/mtk_ghpm_swwa.ko")

        mgk_64_device_modules.remove("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_wrapper.ko")

        mgk_64_device_modules.remove("drivers/iommu/arm/arm-smmu-v3/arm_smmu_v3.ko")
        mgk_64_device_modules.remove("drivers/iommu/arm/arm-smmu-v3/mtk-smmuv3-pmu.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/iommu/arm/arm-smmu-v3:mtk-smmuv3-mpam-mon".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/iommu:mtk_smmu_qos".format(kernel_version))


        mgk_64_device_modules.remove("drivers/memory/mediatek/emi.ko")
        mgk_64_device_modules.remove("drivers/memory/mediatek/emi-fake-eng.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/memory/mediatek:emi-mpu".format(kernel_version))
        mgk_64_device_modules.remove("drivers/memory/mediatek/emi-mpu-test.ko")
        mgk_64_device_modules.remove("drivers/memory/mediatek/emi-mpu-test-v2.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/memory/mediatek:emi-slb".format(kernel_version))
        mgk_64_device_modules.remove("drivers/memory/mediatek/smpu.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/memory/mediatek:slc-parity".format(kernel_version))
        mgk_64_device_modules.remove("drivers/memory/mediatek/smpu-hook-v1.ko")

        mgk_64_device_modules.remove("drivers/misc/mediatek/memory-amms/memory-amms.ko")

        mgk_64_device_modules.remove("drivers/misc/mediatek/adsp/adsp.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/adsp/v1/adsp-v1.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/adsp/v2/adsp-v2.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/adsp/v3/adsp-v3.ko")

        mgk_64_device_modules.remove("drivers/misc/mediatek/mminfra/mtk-mminfra-debug.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mminfra/mtk-mminfra-imax.ko")
        mgk_64_device_modules.remove("drivers/gpu/drm/panel/panel-alpha-jdi-nt36672e-vdo-120hz-hfp.ko")
        mgk_64_device_modules.remove("drivers/gpu/drm/panel/panel-alpha-jdi-nt36672e-vdo-120hz.ko")
        mgk_64_device_modules.remove("drivers/gpu/drm/panel/panel-alpha-jdi-nt36672e-vdo-120hz-threshold.ko")
        mgk_64_device_modules.remove("drivers/gpu/drm/panel/panel-alpha-jdi-nt36672e-vdo-144hz-hfp.ko")
        mgk_64_device_modules.remove("drivers/gpu/drm/panel/panel-alpha-jdi-nt36672e-vdo-144hz.ko")
        mgk_64_device_modules.remove("drivers/gpu/drm/panel/panel-alpha-jdi-nt36672e-vdo-60hz.ko")
        mgk_64_device_modules.remove("drivers/gpu/drm/panel/panel-nt37801-cmd-120hz.ko")
        mgk_64_device_modules.remove("drivers/gpu/drm/panel/panel-nt37801-cmd-fhd.ko")
        mgk_64_device_modules.remove("drivers/gpu/drm/panel/panel-nt37801-cmd-ltpo.ko")
        mgk_64_device_modules.remove("drivers/gpu/drm/panel/panel-nt37801-cmd-fhd-plus.ko")
        mgk_64_device_modules.remove("drivers/gpu/drm/panel/panel-nt37801-cmd-spr.ko")
        mgk_64_device_modules.remove("drivers/gpu/drm/panel/panel-boe-ts127qfmll1dkp0.ko")

        mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6886.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6897.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6899.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6985.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6989.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6989_fpga.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6991.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6993.ko")

        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/cm_mgr/mtk_cm_mgr_mt6989.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/cm_mgr/mtk_cm_mgr_mt6886.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/cm_mgr/mtk_cm_mgr_mt6897.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/cm_mgr/mtk_cm_mgr_mt6983.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/cm_mgr/mtk_cm_mgr_mt6985.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/cm_mgr/mtk_cm_mgr_mt6991.ko")

        #mgk_64_platform_device_modules.pop("drivers/misc/mediatek/lpm/modules/debug/mt6886/mtk-lpm-dbg-mt6886.ko")
        #mgk_64_platform_device_modules.pop("drivers/misc/mediatek/lpm/modules/debug/mt6897/mtk-lpm-dbg-mt6897.ko")
        #mgk_64_platform_device_modules.pop("drivers/misc/mediatek/lpm/modules/debug/mt6983/mtk-lpm-dbg-mt6983.ko")
        #mgk_64_platform_device_modules.pop("drivers/misc/mediatek/lpm/modules/debug/mt6985/mtk-lpm-dbg-mt6985.ko")
        #mgk_64_platform_device_modules.pop("drivers/misc/mediatek/lpm/modules/debug/mt6989/mtk-lpm-dbg-mt6989.ko")
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/lpm/modules/debug/mt6991:mtk-lpm-dbg-mt6991".format(kernel_version))

        mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6985/mt6985-mt6338.ko")
        mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6985/snd-soc-mt6985-afe.ko")

        mgk_64_platform_device_modules.pop("drivers/clk/mediatek/clk-chk-mt6989.ko")
        mgk_64_platform_device_modules.pop("drivers/clk/mediatek/pd-chk-mt6989.ko")
        mgk_64_platform_device_modules.pop("drivers/clk/mediatek/clk-chk-mt6991.ko")
        mgk_64_platform_device_modules.pop("drivers/clk/mediatek/pd-chk-mt6991.ko")

        mgk_64_device_modules.remove("drivers/misc/mediatek/vcp/rv/vcp.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/vcp/rv/vcp_status.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/vdec_fmt/vdec-fmt.ko")
        mgk_64_device_modules.remove("drivers/media/platform/mtk-vcodec/mtk-vcodec-dec-v1.ko")
        mgk_64_device_modules.remove("drivers/media/platform/mtk-vcodec/mtk-vcodec-enc-v1.ko")
        mgk_64_device_modules.remove("drivers/soc/mediatek/mtk-mmdvfs-v3.ko")
        mgk_64_device_modules.remove("sound/soc/codecs/mt6681-accdet.ko")

        mgk_64_device_modules.append("drivers/misc/mediatek/sensor/2.0/mtk_nanohub/nanohub.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/sensor/2.0/sensorhub/sensorhub.ko")

        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/pmsr:pmsr".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/pmsr/v2:pmsr_v2".format(kernel_version))
        mgk_64_device_modules.remove("drivers/misc/mediatek/pmsr/v3/pmsr_v3.ko")

        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v1/mtk-swpm-dbg-common-v1.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug/v1:mtk-swpm-isp-wrapper".format(kernel_version))
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6886/mtk-swpm-core-dbg-v6886.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6886/mtk-swpm-cpu-dbg-v6886.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6886/mtk-swpm-dbg-v6886.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6886/mtk-swpm-mem-dbg-v6886.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6897/mtk-swpm-audio-dbg-v6897.ko")
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
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6989/mtk-swpm-audio-dbg-v6989.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6989/mtk-swpm-core-dbg-v6989.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6989/mtk-swpm-cpu-dbg-v6989.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6989/mtk-swpm-dbg-v6989.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6989/mtk-swpm-disp-dbg-v6989.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6989/mtk-swpm-mem-dbg-v6989.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6989/mtk-swpm-mml-dbg-v6989.ko")

        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/mtk-swpm.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm:mtk-swpm-perf-arm-pmu".format(kernel_version))
        mgk_64_device_modules.remove("drivers/misc/mediatek/tinysys_scmi/tinysys-scmi.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/trusted_mem/tmem_ffa.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/trusted_mem/ffa_v11.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6991/mtk-swpm-dbg-v6991.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/audio_ipi/audio_ipi.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/ppm_v3/mtk_ppm_v3.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/cm_mgr/mtk_cm_ipi.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/iommu:smmu_secure".format(kernel_version))

        mgk_64_device_modules.append("drivers/misc/mediatek/cm_mgr_legacy_v1/mtk_cm_mgr.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/ise_lpm/ise_lpm.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/ise_trusty/ise-trusty.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/ise_trusty/ise-trusty-ipc.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/ise_trusty/ise-trusty-log.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/ise_trusty/ise-trusty-virtio.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/lpm/governors/MHSP/lpm-gov-MHSP.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/lpm/modules/debug/v1/mtk-lpm-dbg-common-v1.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/lpm/modules/debug/v2/mtk-lpm-dbg-common-v2.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/lpm/modules/platform/v1/mtk-lpm-plat-v1.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/lpm/modules/platform/v2/mtk-lpm-plat-v2.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/lpm/mtk-lpm.ko")

        mgk_64_device_modules.remove("drivers/misc/mediatek/ssc/debug/v1/mtk-ssc-dbg-v1.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/ssc/debug/v2/mtk-ssc-dbg-v2.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/ssc/mtk-ssc.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/mtk_mbraink.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/sspm/v3/sspm_v3.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/usb/usb_offload/usb_offload.ko")
        mgk_64_device_modules.remove("drivers/tee/teei/510/isee-ffa.ko")
        mgk_64_device_modules.remove("sound/soc/mediatek/audio_dsp/mtk-soc-offload-common.ko")
        mgk_64_device_modules.remove("sound/soc/mediatek/audio_dsp/snd-soc-audiodsp-common.ko")
        mgk_64_device_modules.remove("sound/soc/mediatek/ultrasound/ultra_common/mtk-scp-ultra.ko")
        mgk_64_device_modules.remove("sound/soc/mediatek/ultrasound/ultra_scp/snd-soc-mtk-scp-ultra.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/performance/mtk_perf_ioctl_magt.ko")

        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi_legacy_v2/emi.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi_legacy_v2/emi-mpu.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi_legacy_v2/emi-mpu-test.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi-dummy.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi_legacy_v2/emi-mpu-hook-v1.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi_legacy_v2/emi-slb.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi_legacy_v2/emi-mpu-test-v2.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi_legacy_v2/emi-fake-eng.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi_legacy_v2/emi-mpu-v2.ko")

        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/modules/v6989/mtk_mbraink_v6989.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/modules/v6991/mtk_mbraink_v6991.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/modules/v6899/mtk_mbraink_v6899.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/bridge/mtk_mbraink_bridge.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6991/mtk-swpm-audio-dbg-v6991.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6991/mtk-swpm-cpu-dbg-v6991.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6991/mtk-swpm-disp-dbg-v6991.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6991/mtk-swpm-mml-dbg-v6991.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6991/mtk-swpm-isp-dbg-v6991.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpu_pdma/mtk_gpu_pdma_mt6991.ko")

        mgk_64_device_modules.remove("drivers/mailbox/mtk-mbox-mailbox.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/pgboost/pgboost.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/lpm_legacy/modules/platform/v1/mtk-lpm-plat-v1-legacy.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/lpm_legacy/modules/debug/k6893/mtk-lpm-dbg-mt6893-legacy.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/lpm_legacy/mtk-lpm-legacy.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/lpm_legacy/modules/debug/v1/mtk-lpm-dbg-common-v1-legacy.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/eem_v2/mediatek_eem.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/slbc:slbc_ipi".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/slbc:slbc_trace".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/regulator:mt6370-regulator".format(kernel_version))
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/slbc/slbc_mt6886.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/slbc/slbc_mt6895.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/slbc/slbc_mt6897.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/slbc/slbc_mt6899.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/slbc/slbc_mt6983.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/slbc/slbc_mt6985.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/slbc/slbc_mt6989.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/slbc/slbc_mt6991.ko")

    if "mt6765_overlay.config" in DEFCONFIG_OVERLAYS:
        mgk_64_platform_device_modules.update({"drivers/regulator/mt6357-regulator.ko":"mt6765"})
        mgk_64_kleaf_platform_modules.update({"//kernel_device_modules-{}/drivers/pinctrl/mediatek:pinctrl-mt6765".format(kernel_version): "mt6765"})
        mgk_64_platform_device_modules.update({"drivers/clk/mediatek/clk-mt6765.ko":"mt6765"})
        mgk_64_platform_device_modules.update({"drivers/clk/mediatek/clk-mt6765-pg.ko":"mt6765"})
        mgk_64_platform_device_modules.update({"drivers/clk/mediatek/clkchk-mt6765.ko":"mt6765"})
        mgk_64_platform_device_modules.update({"drivers/clk/mediatek/clk-fmeter-mt6765.ko":"mt6765"})
        mgk_64_platform_device_modules.update({"drivers/clk/mediatek/clkchk-mt6765.ko":"mt6765"})
        mgk_64_platform_device_modules.update({"drivers/clk/mediatek/clkdbg-mt6765.ko":"mt6765"})
        mgk_64_platform_device_modules.update({"drivers/clk/mediatek/mt6765_clkmgr.ko":"mt6765"})
        mgk_64_platform_device_modules.update({"drivers/misc/mediatek/qos/mtk_qos_legacy.ko":"mt6765"})
        mgk_64_platform_device_modules.update({"drivers/interconnect/mediatek/mmqos-mt6765.ko":"mt6765"})
        mgk_64_platform_device_modules.pop("drivers/clk/mediatek/clk-chk-mt6989.ko")
        mgk_64_platform_device_modules.pop("drivers/clk/mediatek/pd-chk-mt6989.ko")
        mgk_64_platform_device_modules.pop("drivers/clk/mediatek/clk-chk-mt6991.ko")
        mgk_64_platform_device_modules.pop("drivers/clk/mediatek/pd-chk-mt6991.ko")

        mgk_64_device_modules.remove("sound/soc/codecs/mt6681-accdet.ko")
        mgk_64_device_modules.remove("sound/soc/codecs/mt6359p-accdet.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/slbc/slbc_mt6886.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/slbc/slbc_mt6893.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/slbc/slbc_mt6895.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/slbc/slbc_mt6897.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/slbc/slbc_mt6899.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/slbc/slbc_mt6983.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/slbc/slbc_mt6985.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/slbc/slbc_mt6989.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/slbc/slbc_mt6991.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/slbc/mmsram.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/slbc:mtk_slbc".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/slbc:slbc_ipi".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/slbc:slbc_trace".format(kernel_version))

        mgk_64_device_modules.append("drivers/misc/mediatek/conn_md/conn_md_drv.ko")

        mgk_64_device_modules.append("drivers/tee/gud/600/MobiCoreDriver/mcDrvModule.ko")
        mgk_64_device_modules.append("drivers/tee/gud/600/MobiCoreDriver/mcDrvModule-ffa.ko")
        mgk_64_device_modules.append("drivers/tee/gud/600/TlcTui/t-base-tui.ko")
        mgk_64_device_modules.append("drivers/tee/teei/515/isee.ko")

        mgk_64_device_modules.remove("drivers/gpu/mediatek/gpueb/mtk_gpueb.ko")
        mgk_64_device_modules.remove("drivers/gpu/mediatek/gpueb/mtk_ghpm.ko")
        mgk_64_device_modules.remove("drivers/gpu/mediatek/gpueb/mtk_ghpm_swwa.ko")
        mgk_64_device_modules.remove("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_wrapper.ko")
        mgk_64_device_modules.remove("drivers/spmi/spmi-mtk-mpu.ko")
        mgk_64_device_modules.remove("drivers/spmi/spmi-mtk-pmif.ko")
        mgk_64_device_modules.append("drivers/gpu/mediatek/gpufreq/v2_legacy/mtk_gpufreq_wrapper_legacy.ko")
        mgk_64_platform_device_modules.update({"drivers/gpu/mediatek/gpufreq/v2_legacy/mtk_gpufreq_mt6765.ko":"mt6765"})
        mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6886.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6897.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6899.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6985.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6989.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6989_fpga.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6991.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6993.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpu_pdma/mtk_gpu_pdma_mt6991.ko")

        mgk_64_device_modules.remove("drivers/gpu/drm/panel/mediatek-cust-panel-sample.ko")
        mgk_64_device_modules.remove("drivers/gpu/drm/panel/mediatek-drm-gateic.ko")
        mgk_64_device_modules.remove("drivers/gpu/drm/panel/mediatek-drm-panel-drv.ko")
        mgk_64_device_modules.remove("drivers/gpu/drm/mediatek/dpc/dpc_v1/mtk_dpc_v1.ko")
        mgk_64_device_modules.remove("drivers/gpu/drm/mediatek/dpc/dpc_v1/mtk_vdisp_v1.ko")
        mgk_64_device_modules.remove("drivers/gpu/drm/mediatek/dpc/dpc_v2/mtk_dpc_v2.ko")
        mgk_64_device_modules.remove("drivers/gpu/drm/mediatek/dpc/dpc_v2/mtk_vdisp_v2.ko")
        mgk_64_device_modules.remove("drivers/gpu/drm/mediatek/dpc/dpc_v3/mtk_dpc_v3.ko")
        mgk_64_device_modules.remove("drivers/gpu/drm/mediatek/dpc/dpc_v3/mtk_vdisp_v3.ko")

        mgk_64_platform_device_modules.pop("drivers/gpu/drm/mediatek/mml/mtk-mml-mt6878.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/drm/mediatek/mml/mtk-mml-mt6886.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/drm/mediatek/mml/mtk-mml-mt6897.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/drm/mediatek/mml/mtk-mml-mt6899.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/drm/mediatek/mml/mtk-mml-mt6983.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/drm/mediatek/mml/mtk-mml-mt6985.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/drm/mediatek/mml/mtk-mml-mt6989.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/drm/mediatek/mml/mtk-mml-mt6991.ko")

        mgk_64_device_modules.append("drivers/misc/mediatek/upower/Upower.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/leakage_table_v2/mediatek_static_power.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/ppm_v3/mtk_ppm_v3.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cpufreq_v2/src/CPU_DVFS.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cpuidle/mtk_cpuidle.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/mcdi/mcdi.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/spm/common_v0/MTK_INTERNAL_SPM.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/eem_v2/mediatek_eem.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/eem_v2/mtk_picachu.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/power_gs_v1/mtk_power_gs_v1.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/mdpm_v1/mtk_mdpm_v1.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mdpm/mtk_mdpm.ko")

        mgk_64_device_modules.append("drivers/misc/mediatek/cm_mgr_legacy_v0/mtk_cm_mgr_v0.ko")

        mgk_64_device_modules.remove("drivers/iommu/arm/arm-smmu-v3/arm_smmu_v3.ko")
        mgk_64_device_modules.remove("drivers/iommu/arm/arm-smmu-v3/mtk-smmuv3-pmu.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/iommu/arm/arm-smmu-v3:mtk-smmuv3-mpam-mon".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/iommu:smmu_secure".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/iommu:mtk_smmu_qos".format(kernel_version))
        mgk_64_device_modules.remove("drivers/misc/mediatek/vcp/rv/vcp.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/vcp/rv/vcp_status.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/iommu/iommu_gz.ko")
        mgk_64_device_modules.append("//kernel_device_modules-{}/drivers/misc/mediatek/dvfsrc:dvfsrc-opp-mt6765".format(kernel_version))

        mgk_64_device_modules.append("drivers/misc/mediatek/hps_v3/mtk_cpuhp.ko")

        mgk_64_device_modules.append("drivers/misc/mediatek/thermal/thermal_monitor.ko")
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
        mgk_64_device_modules.remove("drivers/trusty/trusty-core.ko")
        mgk_64_device_modules.remove("drivers/trusty/trusty-ipc.ko")
        mgk_64_device_modules.remove("drivers/trusty/trusty-log.ko")
        mgk_64_device_modules.remove("drivers/trusty/trusty-test.ko")
        mgk_64_device_modules.remove("drivers/trusty/trusty-virtio.ko")

        mgk_64_device_modules.remove("drivers/memory/mediatek/emi.ko")
        mgk_64_device_modules.remove("drivers/memory/mediatek/emi-fake-eng.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/memory/mediatek:emi-mpu".format(kernel_version))
        mgk_64_device_modules.remove("drivers/memory/mediatek/emi-mpu-test.ko")
        mgk_64_device_modules.remove("drivers/memory/mediatek/emi-mpu-test-v2.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/memory/mediatek:emi-slb".format(kernel_version))
        mgk_64_device_modules.remove("drivers/memory/mediatek/smpu.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/memory/mediatek:slc-parity".format(kernel_version))
        mgk_64_device_modules.remove("drivers/memory/mediatek/smpu-hook-v1.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi_legacy_v1/emicen.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi_legacy_v1/emimpu.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi_legacy_v1/emiisu.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi_legacy_v1/emictrl.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi-dummy.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/smi/mtk-smi-bwc.ko")
        mgk_64_common_eng_modules.remove("drivers/firmware/arm_ffa/ffa-module.ko")
        mgk_64_common_userdebug_modules.remove("drivers/firmware/arm_ffa/ffa-module.ko")
        mgk_64_common_user_modules.remove("drivers/firmware/arm_ffa/ffa-module.ko")

        mgk_64_device_modules.remove("drivers/misc/mediatek/sda/cache-parity.ko")
        mgk_64_device_modules.remove("drivers/dma/mediatek/mtk-cqdma.ko")

        mgk_64_device_modules.remove("drivers/misc/mediatek/pkvm_tmem/pkvm_tmem.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/trusted_mem/tmem_ffa.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/trusted_mem/ffa_v11.ko")
        mgk_64_device_modules.remove("drivers/tee/teei/510/isee-ffa.ko")

        mgk_64_device_modules.append("drivers/misc/mediatek/videocodec/vcodec_kernel_common_driver.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/videocodec/vcodec_kernel_driver-v1.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/vdec_fmt/vdec-fmt.ko")
        mgk_64_device_modules.remove("drivers/media/platform/mtk-vcodec/mtk-vcodec-dec-v2.ko")
        mgk_64_device_modules.remove("drivers/media/platform/mtk-vcodec/mtk-vcodec-enc-v2.ko")

        mgk_64_device_modules.remove("drivers/soc/mediatek/mtk-mmdvfs-v3.ko")

        mgk_64_device_modules.remove("drivers/misc/mediatek/sspm/v3/sspm_v3.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/sspm/v1/sspm_v1.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/qos/mtk_qos_legacy.ko")
        mgk_64_platform_device_modules.update({"drivers/power/supply/mt6357-charger-type.ko":"mt6765"})
        mgk_64_platform_device_modules.update({"drivers/power/supply/rt9465.ko":"mt6765"})

        mgk_64_device_modules.remove("drivers/misc/mediatek/cm_mgr/mtk_cm_ipi.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/cm_mgr/mtk_cm_mgr.ko")

        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/modules/v6989/mtk_mbraink_v6989.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/modules/v6991/mtk_mbraink_v6991.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/modules/v6899/mtk_mbraink_v6899.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/mtk_mbraink.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/bridge/mtk_mbraink_bridge.ko")

        mgk_64_device_modules.remove("drivers/misc/mediatek/mminfra/mtk-mminfra-debug.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mminfra/mtk-mminfra-imax.ko")

        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/pmsr:pmsr".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/pmsr/v2:pmsr_v2".format(kernel_version))
        mgk_64_device_modules.remove("drivers/misc/mediatek/pmsr/v3/pmsr_v3.ko")

        mgk_64_device_modules.remove("drivers/misc/mediatek/qos/mtk_qos.ko")

        mgk_64_device_modules.remove("drivers/misc/mediatek/ssc/debug/v1/mtk-ssc-dbg-v1.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/ssc/debug/v2/mtk-ssc-dbg-v2.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/ssc/mtk-ssc.ko")

        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v1/mtk-swpm-dbg-common-v1.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug/v1:mtk-swpm-isp-wrapper".format(kernel_version))
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
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6991/mtk-swpm-dbg-v6991.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6991/mtk-swpm-cpu-dbg-v6991.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/mtk-swpm.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm:mtk-swpm-perf-arm-pmu".format(kernel_version))

        mgk_64_device_modules.remove("drivers/misc/mediatek/tinysys_scmi/tinysys-scmi.ko")

        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/cm_mgr/mtk_cm_mgr_mt6886.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/cm_mgr/mtk_cm_mgr_mt6897.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/cm_mgr/mtk_cm_mgr_mt6983.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/cm_mgr/mtk_cm_mgr_mt6985.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/cm_mgr/mtk_cm_mgr_mt6991.ko")

        #mgk_64_platform_device_modules.pop("drivers/misc/mediatek/lpm/modules/debug/mt6989/mtk-lpm-dbg-mt6989.ko")
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/lpm/modules/debug/mt6991:mtk-lpm-dbg-mt6991".format(kernel_version))

        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6897/mtk-swpm-audio-dbg-v6897.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6989/mtk-swpm-audio-dbg-v6989.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6989/mtk-swpm-core-dbg-v6989.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6989/mtk-swpm-cpu-dbg-v6989.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6989/mtk-swpm-dbg-v6989.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6989/mtk-swpm-disp-dbg-v6989.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6989/mtk-swpm-mem-dbg-v6989.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6989/mtk-swpm-mml-dbg-v6989.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6991/mtk-swpm-disp-dbg-v6991.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6991/mtk-swpm-audio-dbg-v6991.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6991/mtk-swpm-mml-dbg-v6991.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6991/mtk-swpm-isp-dbg-v6991.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/imgsensor/src/isp6s/imgsensor_isp6s.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/imgsensor/src/isp4_c/imgsensor_isp4_c.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/cam_cal/src/custom/camera_eeprom.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cam_cal/src/isp4_c/camera_eeprom_isp4_c.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cameraisp/dpe/camera_dpe_isp40.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/cm_mgr/mtk_cm_mgr_mt6989.ko")

        mgk_64_device_modules.remove("drivers/misc/mediatek/mcupm/v2/mcupm.ko")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/cpufreq_cus:cpu_freq")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/cpufreq_int:cpu_hwtest")
        mgk_64_device_modules.append("drivers/misc/mediatek/dcm/mt6765_dcm.ko")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/gpu:gpu")
        mgk_64_kleaf_modules.append("//vendor/mediatek/kernel_modules/gpu:gpu_mt6765")

        mgk_64_device_modules.remove("sound/soc/codecs/snd-soc-mt6338.ko")
        mgk_64_device_modules.remove("sound/soc/codecs/snd-soc-mt6368.ko")
        mgk_64_device_modules.remove("sound/soc/mediatek/audio_dsp/mtk-soc-offload-common.ko")
        mgk_64_device_modules.remove("sound/soc/mediatek/audio_dsp/snd-soc-audiodsp-common.ko")
        mgk_64_device_modules.remove("sound/soc/mediatek/ultrasound/ultra_common/mtk-scp-ultra.ko")
        mgk_64_device_modules.remove("sound/soc/mediatek/ultrasound/ultra_scp/snd-soc-mtk-scp-ultra.ko")
        mgk_64_device_modules.remove("sound/soc/mediatek/vow/mtk-scp-vow.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/adsp/adsp.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/adsp/v1/adsp-v1.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/adsp/v2/adsp-v2.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/adsp/v3/adsp-v3.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/audio_ipi/audio_ipi.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/usb/usb_offload/usb_offload.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/vow/ver02/mtk-vow.ko")
        mgk_64_device_modules.remove("sound/soc/codecs/snd-soc-mt6359.ko")
        mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6886/mt6886-mt6368.ko")
        mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6886/snd-soc-mt6886-afe.ko")
        mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6897/mt6897-mt6368.ko")
        mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6897/snd-soc-mt6897-afe.ko")
        mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6983/mt6983-mt6338.ko")
        mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6983/snd-soc-mt6983-afe.ko")
        mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6985/mt6985-mt6338.ko")
        mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6985/snd-soc-mt6985-afe.ko")
        mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6885/snd-soc-mt6885-afe.ko")
        mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6885/mt6885-mt6359p.ko")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/mtkcam/isp_pspm:isp_pspm")

        mgk_64_device_modules.append("drivers/misc/mediatek/scp/cm4/scp.ko")
        mgk_64_device_modules.remove("drivers/gpu/drm/mediatek/mediatek_v2/mtk_aod_scp.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/apusys/apusys.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/apusys/apu_aov.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/apusys/power/apu_top.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/apusys/sapu/sapu.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/sensor/2.0/sensorhub/sensorhub.ko")
        mgk_64_device_modules.remove("sound/soc/codecs/mt6338-accdet.ko")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/mtkcam/scpsys/mtk-aov:mtk_aov")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/mtkcam/sched:c2ps")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/mtkcam/sched/c2ps_ioctl:c2ps_perf_ioctl")

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

        mgk_64_device_modules.remove("drivers/misc/mediatek/cameraisp/src/isp_6s/camera_isp.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cameraisp/src/isp_4/camera_isp_4_c.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cameraisp/src/isp_4/cam_qos_4.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cameraisp/fdvt/camera_fdvt_isp40.ko")

        mgk_64_device_modules.append("sound/soc/mediatek/codec/snd-mtk-soc-codec-6357.ko")
        mgk_64_device_modules.append("sound/soc/codecs/snd-soc-rt5509.ko")

        mgk_64_platform_device_modules.update({"sound/soc/mediatek/common_int/mtk-soc-sound.ko":"mt6765"})
        mgk_64_platform_device_modules.update({"sound/soc/mediatek/common_int/mtk-auddrv-gpio.ko":"mt6765"})
        mgk_64_platform_device_modules.update({"sound/soc/mediatek/common_int/mtk-soc-speaker-amp.ko":"mt6765"})
        mgk_64_platform_device_modules.update({"sound/soc/mediatek/common_int/mtk-soc-sound-cycle-dependent.ko":"mt6765"})
        mgk_64_platform_device_modules.update({"sound/soc/mediatek/common_int/mtk-soc-pcm-capture.ko":"mt6765"})
        mgk_64_platform_device_modules.update({"sound/soc/mediatek/common_int/mtk-soc-pcm-dl1.ko":"mt6765"})
        mgk_64_platform_device_modules.update({"sound/soc/mediatek/common_int/mtk-soc-pcm-routing.ko":"mt6765"})
        mgk_64_platform_device_modules.update({"sound/soc/mediatek/common_int/mtk-soc-pcm-capture2.ko":"mt6765"})
        mgk_64_platform_device_modules.update({"sound/soc/mediatek/common_int/mtk-soc-pcm-i2s2-adc2.ko":"mt6765"})
        mgk_64_platform_device_modules.update({"sound/soc/mediatek/common_int/mtk-soc-pcm-voice-usb-echoref.ko":"mt6765"})
        mgk_64_platform_device_modules.update({"sound/soc/mediatek/common_int/mtk-soc-pcm-dl1-i2s0Dl1.ko":"mt6765"})
        mgk_64_platform_device_modules.update({"sound/soc/mediatek/common_int/mtk-soc-pcm-i2s0-awb.ko":"mt6765"})
        mgk_64_platform_device_modules.update({"sound/soc/mediatek/common_int/mtk-soc-pcm-uldlloopback.ko":"mt6765"})
        mgk_64_platform_device_modules.update({"sound/soc/mediatek/common_int/mtk-soc-pcm-deep-buffer-dl.ko":"mt6765"})
        mgk_64_platform_device_modules.update({"sound/soc/mediatek/common_int/mtk-soc-pcm-mrgrx.ko":"mt6765"})
        mgk_64_platform_device_modules.update({"sound/soc/mediatek/common_int/mtk-soc-pcm-mrgrx-awb.ko":"mt6765"})
        mgk_64_platform_device_modules.update({"sound/soc/mediatek/common_int/mtk-soc-pcm-fm-i2s.ko":"mt6765"})
        mgk_64_platform_device_modules.update({"sound/soc/mediatek/common_int/mtk-soc-pcm-fm-i2s-awb.ko":"mt6765"})
        mgk_64_platform_device_modules.update({"sound/soc/mediatek/common_int/mtk-soc-pcm-dl1-awb.ko":"mt6765"})
        mgk_64_platform_device_modules.update({"sound/soc/mediatek/common_int/mtk-soc-pcm-dl1-bt.ko":"mt6765"})
        mgk_64_platform_device_modules.update({"sound/soc/mediatek/common_int/mtk-soc-pcm-bt-dai.ko":"mt6765"})
        mgk_64_platform_device_modules.update({"sound/soc/mediatek/common_int/mtk-soc-dai-stub.ko":"mt6765"})
        mgk_64_platform_device_modules.update({"sound/soc/mediatek/common_int/mtk-soc-dai-routing.ko":"mt6765"})
        mgk_64_platform_device_modules.update({"sound/soc/mediatek/common_int/mtk-soc-codec-dummy.ko":"mt6765"})
        mgk_64_platform_device_modules.update({"sound/soc/mediatek/common_int/mtk-soc-pcm-fmtx.ko":"mt6765"})
        mgk_64_platform_device_modules.update({"sound/soc/mediatek/common_int/mtk-soc-tdm-capture.ko":"mt6765"})
        mgk_64_platform_device_modules.update({"sound/soc/mediatek/common_int/mtk-soc-pcm-hp-impedance.ko":"mt6765"})
        mgk_64_platform_device_modules.update({"sound/soc/mediatek/common_int/mtk-soc-machine.ko":"mt6765"})
        mgk_64_device_modules.remove("drivers/misc/mediatek/performance/mtk_perf_ioctl_magt.ko")
        mgk_64_common_eng_modules.remove("drivers/perf/arm_dsu_pmu.ko")
        mgk_64_common_userdebug_modules.remove("drivers/perf/arm_dsu_pmu.ko")
        mgk_64_common_user_modules.remove("drivers/perf/arm_dsu_pmu.ko")
        mgk_64_device_modules.append("drivers/mmc/host/mtk-mmc-swcqhci.ko")

    if "mt6833_overlay.config" in DEFCONFIG_OVERLAYS:
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/gpu:gpu")
        mgk_64_kleaf_modules.append("//vendor/mediatek/kernel_modules/gpu:gpu_mt6833")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/mtkcam/sched:c2ps")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/mtkcam/sched/c2ps_ioctl:c2ps_perf_ioctl")
        mgk_64_platform_device_modules.update({"drivers/gpu/mediatek/gpufreq/v2_legacy/mtk_gpufreq_mt6833.ko":"mt6833"})
        #mgk_64_device_modules.append("drivers/gpu/mediatek/gpufreq/v2_legacy/mtk_gpufreq_mt6833.ko")
        mgk_64_device_modules.remove("drivers/char/rpmb/rpmb.ko")
        mgk_64_device_modules.remove("drivers/char/rpmb/rpmb-mtk.ko")
        mgk_64_common_eng_modules.remove("drivers/firmware/arm_ffa/ffa-module.ko")
        mgk_64_common_userdebug_modules.remove("drivers/firmware/arm_ffa/ffa-module.ko")
        mgk_64_common_user_modules.remove("drivers/firmware/arm_ffa/ffa-module.ko")
        mgk_64_device_modules.append("drivers/tee/gud/600/MobiCoreDriver/mcDrvModule.ko")
        mgk_64_device_modules.append("drivers/tee/gud/600/MobiCoreDriver/mcDrvModule-ffa.ko")

        mgk_64_device_modules.append("drivers/tee/gud/600/TlcTui/t-base-tui.ko")
        mgk_64_device_modules.remove("drivers/iommu/arm/arm-smmu-v3/arm_smmu_v3.ko")
        mgk_64_device_modules.remove("drivers/iommu/arm/arm-smmu-v3/mtk-smmuv3-pmu.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/iommu/arm/arm-smmu-v3:mtk-smmuv3-mpam-mon".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/iommu:smmu_secure".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/iommu:mtk_smmu_qos".format(kernel_version))
        mgk_64_device_modules.append("drivers/misc/mediatek/iommu/iommu_gz.ko")
        mgk_64_device_modules.append("drivers/soc/mediatek/devmpu/devmpu.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/pkvm_tmem/pkvm_tmem.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/slbc/mmsram.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/slbc:mtk_slbc".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/slbc:slbc_ipi".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/slbc:slbc_trace".format(kernel_version))
        mgk_64_device_modules.append("drivers/misc/mediatek/conn_md/conn_md_drv.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/slbc/slbc_mt6886.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/slbc/slbc_mt6893.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/slbc/slbc_mt6895.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/slbc/slbc_mt6897.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/slbc/slbc_mt6899.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/slbc/slbc_mt6983.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/slbc/slbc_mt6985.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/slbc/slbc_mt6989.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/slbc/slbc_mt6991.ko")
        mgk_64_device_modules.append("drivers/clk/mediatek/clk-mt6833.ko")
        mgk_64_device_modules.append("drivers/clk/mediatek/clk-mt6833-audiosys.ko")
        mgk_64_device_modules.append("drivers/clk/mediatek/clk-mt6833-cam.ko")
        mgk_64_device_modules.append("drivers/clk/mediatek/clk-mt6833-mmsys.ko")
        mgk_64_device_modules.append("drivers/clk/mediatek/clk-mt6833-img.ko")
        mgk_64_device_modules.append("drivers/clk/mediatek/clk-mt6833-i2c.ko")
        mgk_64_device_modules.append("drivers/clk/mediatek/clk-mt6833-bus.ko")
        mgk_64_device_modules.append("drivers/clk/mediatek/clk-mt6833-vcodec.ko")
        mgk_64_device_modules.append("drivers/clk/mediatek/clk-mt6833-mfgcfg.ko")
        mgk_64_device_modules.append("drivers/clk/mediatek/clk-chk-mt6833.ko")
        mgk_64_device_modules.append("drivers/clk/mediatek/clk-dbg-mt6833.ko")
        mgk_64_device_modules.append("drivers/clk/mediatek/pd-chk-mt6833.ko")
        mgk_64_device_modules.append("drivers/clk/mediatek/clk-fmeter-mt6833.ko")
        mgk_64_kleaf_platform_modules.update({"//kernel_device_modules-{}/drivers/pinctrl/mediatek:pinctrl-mt6833".format(kernel_version): "mt6833"})
        mgk_64_device_modules.append("drivers/misc/mediatek/dcm/mt6833_dcm.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi_legacy_v2/emi-mpu-v2.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi_legacy_v2/emi-fake-eng.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi_legacy_v2/emi.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi_legacy_v2/emi-mpu.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi_legacy_v2/emi-mpu-hook-v1.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi-dummy.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi_legacy_v2/emi-slb.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi_legacy_v2/emi-mpu-test.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi_legacy_v2/emi-mpu-test-v2.ko")
        #mgk_64_device_modules.append("drivers/misc/mediatek/eemgpu/mtk_eem.ko")
        mgk_64_device_modules.append("drivers/gpu/mediatek/gpufreq/v2_legacy/mtk_gpufreq_wrapper_legacy.ko")
        mgk_64_device_modules.remove("drivers/gpu/mediatek/gpueb/mtk_gpueb.ko")
        mgk_64_device_modules.remove("drivers/gpu/mediatek/gpueb/mtk_ghpm.ko")
        mgk_64_device_modules.remove("drivers/gpu/mediatek/gpueb/mtk_ghpm_swwa.ko")
        mgk_64_device_modules.remove("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_wrapper.ko")
        mgk_64_device_modules.remove("drivers/memory/mediatek/emi.ko")
        mgk_64_device_modules.remove("drivers/memory/mediatek/emi-fake-eng.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/memory/mediatek:emi-mpu".format(kernel_version))
        mgk_64_device_modules.remove("drivers/memory/mediatek/emi-mpu-test.ko")
        mgk_64_device_modules.remove("drivers/memory/mediatek/emi-mpu-test-v2.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/memory/mediatek:emi-slb".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/memory/mediatek:slc-parity".format(kernel_version))
        mgk_64_device_modules.remove("drivers/memory/mediatek/smpu.ko")
        mgk_64_device_modules.remove("drivers/memory/mediatek/smpu-hook-v1.ko")
        mgk_64_platform_device_modules.pop("drivers/clk/mediatek/clk-mt6893-apu0.ko")
        mgk_64_platform_device_modules.pop("drivers/clk/mediatek/clk-mt6893-apu1.ko")
        mgk_64_platform_device_modules.pop("drivers/clk/mediatek/clk-mt6893-apu2.ko")
        mgk_64_platform_device_modules.pop("drivers/clk/mediatek/clk-mt6893-apuc.ko")
        mgk_64_platform_device_modules.pop("drivers/clk/mediatek/clk-mt6893-apum0.ko")
        mgk_64_platform_device_modules.pop("drivers/clk/mediatek/clk-mt6893-apum1.ko")
        mgk_64_platform_device_modules.pop("drivers/clk/mediatek/clk-mt6893-apuv.ko")
        mgk_64_platform_device_modules.pop("drivers/clk/mediatek/clk-mt6893-audsys.ko")
        mgk_64_platform_device_modules.pop("drivers/clk/mediatek/clk-mt6893-cam_m.ko")
        mgk_64_platform_device_modules.pop("drivers/clk/mediatek/clk-mt6893-cam_ra.ko")
        mgk_64_platform_device_modules.pop("drivers/clk/mediatek/clk-mt6893-cam_rb.ko")
        mgk_64_platform_device_modules.pop("drivers/clk/mediatek/clk-mt6893-cam_rc.ko")
        mgk_64_platform_device_modules.pop("drivers/clk/mediatek/clk-mt6893-imgsys1.ko")
        mgk_64_platform_device_modules.pop("drivers/clk/mediatek/clk-mt6893-imgsys2.ko")
        mgk_64_platform_device_modules.pop("drivers/clk/mediatek/clk-mt6893-impc.ko")
        mgk_64_platform_device_modules.pop("drivers/clk/mediatek/clk-mt6893-impe.ko")
        mgk_64_platform_device_modules.pop("drivers/clk/mediatek/clk-mt6893-impn.ko")
        mgk_64_platform_device_modules.pop("drivers/clk/mediatek/clk-mt6893-imps.ko")
        mgk_64_platform_device_modules.pop("drivers/clk/mediatek/clk-mt6893-ipe.ko")
        mgk_64_platform_device_modules.pop("drivers/clk/mediatek/clk-mt6893-mdp.ko")
        mgk_64_platform_device_modules.pop("drivers/clk/mediatek/clk-mt6893-mfgcfg.ko")
        mgk_64_platform_device_modules.pop("drivers/clk/mediatek/clk-mt6893-mm.ko")
        mgk_64_platform_device_modules.pop("drivers/clk/mediatek/clk-mt6893-scp_adsp.ko")
        mgk_64_platform_device_modules.pop("drivers/clk/mediatek/clk-mt6893-vde1.ko")
        mgk_64_platform_device_modules.pop("drivers/clk/mediatek/clk-mt6893-vde2.ko")
        mgk_64_platform_device_modules.pop("drivers/clk/mediatek/clk-mt6893-ven1.ko")
        mgk_64_platform_device_modules.pop("drivers/clk/mediatek/clk-mt6893-ven2.ko")
        mgk_64_platform_device_modules.pop("drivers/clk/mediatek/clk-mt6893.ko")
        mgk_64_platform_device_modules.pop("drivers/clk/mediatek/clk-chk-mt6893.ko")
        mgk_64_platform_device_modules.pop("drivers/clk/mediatek/clk-dbg-mt6893.ko")
        mgk_64_platform_device_modules.pop("drivers/clk/mediatek/clk-fmeter-mt6893.ko")
        mgk_64_platform_device_modules.pop("drivers/clk/mediatek/pd-chk-mt6893.ko")
        mgk_64_platform_device_modules.pop("drivers/clk/mediatek/clk-mt6893-pg.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6886.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6897.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6899.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6985.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6989.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6989_fpga.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6991.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6993.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpu_pdma/mtk_gpu_pdma_mt6991.ko")
        mgk_64_platform_device_modules.update({"drivers/interconnect/mediatek/mmqos-mt6833.ko":"mt6833"})
        mgk_64_device_modules.append("drivers/misc/mediatek/cameraisp/rsc/camera_rsc_isp6s.ko")

        mgk_64_device_modules.append("drivers/misc/mediatek/thermal/thermal_monitor.ko")
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
        mgk_64_device_modules.remove("drivers/misc/mediatek/mminfra/mtk-mminfra-debug.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mminfra/mtk-mminfra-imax.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/vcp/rv/vcp.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/vcp/rv/vcp_status.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/vdec_fmt/vdec-fmt.ko")
        mgk_64_device_modules.remove("drivers/media/platform/mtk-vcodec/mtk-vcodec-dec-v2.ko")
        mgk_64_device_modules.remove("drivers/media/platform/mtk-vcodec/mtk-vcodec-enc-v2.ko")
        mgk_64_device_modules.remove("drivers/soc/mediatek/mtk-mmdvfs-v3.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/regulator:mt6370-regulator".format(kernel_version))
        mgk_64_platform_device_modules.pop("drivers/clk/mediatek/clk-chk-mt6989.ko")
        mgk_64_platform_device_modules.pop("drivers/clk/mediatek/pd-chk-mt6989.ko")
        mgk_64_platform_device_modules.pop("drivers/clk/mediatek/clk-chk-mt6991.ko")
        mgk_64_platform_device_modules.pop("drivers/clk/mediatek/pd-chk-mt6991.ko")

        mgk_64_device_modules.append("drivers/misc/mediatek/sspm/v2/sspm.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/sspm/v3/sspm_v3.ko")
        #mgk_64_platform_device_modules.pop("drivers/misc/mediatek/lpm/modules/debug/mt6989/mtk-lpm-dbg-mt6989.ko")
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/lpm/modules/debug/mt6991:mtk-lpm-dbg-mt6991".format(kernel_version))
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/bridge/mtk_mbraink_bridge.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/mtk_mbraink.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/modules/v6989/mtk_mbraink_v6989.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/modules/v6991/mtk_mbraink_v6991.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/modules/v6899/mtk_mbraink_v6899.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/pmsr:pmsr".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/pmsr/v2:pmsr_v2".format(kernel_version))
        mgk_64_device_modules.remove("drivers/misc/mediatek/pmsr/v3/pmsr_v3.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug/v1:mtk-swpm-isp-wrapper".format(kernel_version))
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
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6991/mtk-swpm-dbg-v6991.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6991/mtk-swpm-cpu-dbg-v6991.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/mtk-swpm.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm:mtk-swpm-perf-arm-pmu".format(kernel_version))
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6897/mtk-swpm-audio-dbg-v6897.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6989/mtk-swpm-audio-dbg-v6989.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6989/mtk-swpm-core-dbg-v6989.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6989/mtk-swpm-cpu-dbg-v6989.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6989/mtk-swpm-dbg-v6989.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6989/mtk-swpm-disp-dbg-v6989.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6989/mtk-swpm-mem-dbg-v6989.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6989/mtk-swpm-mml-dbg-v6989.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6991/mtk-swpm-disp-dbg-v6991.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6991/mtk-swpm-audio-dbg-v6991.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6991/mtk-swpm-mml-dbg-v6991.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6991/mtk-swpm-isp-dbg-v6991.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/tinysys_scmi/tinysys-scmi.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/cm_mgr/mtk_cm_ipi.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/cm_mgr/mtk_cm_mgr.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/cm_mgr/mtk_cm_mgr_mt6886.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/cm_mgr/mtk_cm_mgr_mt6897.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/cm_mgr/mtk_cm_mgr_mt6983.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/cm_mgr/mtk_cm_mgr_mt6985.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/cm_mgr/mtk_cm_mgr_mt6989.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/cm_mgr/mtk_cm_mgr_mt6991.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/ssc/debug/v1/mtk-ssc-dbg-v1.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/ssc/debug/v2/mtk-ssc-dbg-v2.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/ssc/mtk-ssc.ko")

        mgk_64_platform_device_modules.pop("drivers/gpu/drm/mediatek/mml/mtk-mml-mt6878.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/drm/mediatek/mml/mtk-mml-mt6886.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/drm/mediatek/mml/mtk-mml-mt6897.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/drm/mediatek/mml/mtk-mml-mt6899.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/drm/mediatek/mml/mtk-mml-mt6983.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/drm/mediatek/mml/mtk-mml-mt6985.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/drm/mediatek/mml/mtk-mml-mt6989.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/drm/mediatek/mml/mtk-mml-mt6991.ko")

        mgk_64_device_modules.remove("drivers/misc/mediatek/trusted_mem/tmem_ffa.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/trusted_mem/ffa_v11.ko")
        mgk_64_device_modules.remove("drivers/tee/teei/510/isee-ffa.ko")
        mgk_64_device_modules.append("drivers/tee/teei/515/isee.ko")

        mgk_64_device_modules.remove("drivers/gpu/drm/mediatek/dpc/dpc_v1/mtk_dpc_v1.ko")
        mgk_64_device_modules.remove("drivers/gpu/drm/mediatek/dpc/dpc_v1/mtk_vdisp_v1.ko")
        mgk_64_device_modules.remove("drivers/gpu/drm/mediatek/dpc/dpc_v2/mtk_dpc_v2.ko")
        mgk_64_device_modules.remove("drivers/gpu/drm/mediatek/dpc/dpc_v2/mtk_vdisp_v2.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/performance/mtk_perf_ioctl_magt.ko")

        mgk_64_device_modules.append("drivers/misc/mediatek/cpufreq_v2/src/CPU_DVFS.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/upower/Upower.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/leakage_table_v2/mediatek_static_power.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/ppm_v3/mtk_ppm_v3.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cpuhotplug/mtk_cpuhp.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/eem_v2/mtk_picachu.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/eem_v2/mediatek_eem.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/lpm_legacy/modules/platform/v1/mtk-lpm-plat-v1-legacy.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/lpm_legacy/modules/debug/k6833/mtk-lpm-dbg-mt6833-legacy.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/lpm_legacy/mtk-lpm-legacy.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/lpm_legacy/modules/debug/v1/mtk-lpm-dbg-common-v1-legacy.ko")

        mgk_64_platform_device_modules.update({"drivers/misc/mediatek/cm_mgr_legacy_v1/mtk_cm_mgr_mt6833.ko":"mt6833"})
        mgk_64_device_modules.append("drivers/misc/mediatek/cm_mgr_legacy_v1/mtk_cm_mgr.ko")

        mgk_64_device_modules.append("drivers/misc/mediatek/spi_slave_drv/spi_slave.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cmdq/bridge/cmdq-bdg-mailbox.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cmdq/bridge/cmdq-bdg-test.ko")

        mgk_64_device_modules.append("sound/soc/codecs/snd-soc-rt5509.ko")
        mgk_64_device_modules.append("sound/soc/mediatek/mt6833/snd-soc-mt6833-afe.ko")
        mgk_64_device_modules.append("sound/soc/mediatek/mt6833/mt6833-mt6359p.ko")

        mgk_64_device_modules.remove("drivers/misc/mediatek/adsp/adsp.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/adsp/v1/adsp-v1.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/adsp/v2/adsp-v2.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/adsp/v3/adsp-v3.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/audio_ipi/audio_ipi.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/usb/usb_offload/usb_offload.ko")
        mgk_64_device_modules.remove("sound/soc/mediatek/audio_dsp/mtk-soc-offload-common.ko")
        mgk_64_device_modules.remove("sound/soc/mediatek/audio_dsp/snd-soc-audiodsp-common.ko")
        mgk_64_device_modules.remove("sound/soc/mediatek/ultrasound/ultra_common/mtk-scp-ultra.ko")
        mgk_64_device_modules.remove("sound/soc/mediatek/ultrasound/ultra_scp/snd-soc-mtk-scp-ultra.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/vow/ver02/mtk-vow.ko")
        mgk_64_device_modules.remove("sound/soc/mediatek/vow/mtk-scp-vow.ko")

        mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6985/mt6985-mt6338.ko")
        mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6985/snd-soc-mt6985-afe.ko")

        mgk_64_device_modules.append("drivers/misc/mediatek/cameraisp/dip/isp_6s/camera_dip_isp6s.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cameraisp/fdvt/camera_fdvt_isp51.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cameraisp/mfb/camera_mfb_isp6s.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cameraisp/dpe/camera_dpe_isp60.ko")

    if "mt6853_overlay.config" in DEFCONFIG_OVERLAYS:
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/gpu:gpu")
        mgk_64_kleaf_modules.append("//vendor/mediatek/kernel_modules/gpu:gpu_mt6853")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/mtkcam/sched:c2ps")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/mtkcam/sched/c2ps_ioctl:c2ps_perf_ioctl")
        mgk_64_platform_device_modules.update({"drivers/gpu/mediatek/gpufreq/v2_legacy/mtk_gpufreq_mt6853.ko":"mt6853"})
        #mgk_64_device_modules.append("drivers/gpu/mediatek/gpufreq/v2_legacy/mtk_gpufreq_mt6853.ko")
        mgk_64_kleaf_platform_modules.update({"//kernel_device_modules-{}/drivers/pinctrl/mediatek:pinctrl-mt6853".format(kernel_version): "mt6853"})
        mgk_64_device_modules.append("drivers/clk/mediatek/clk-mt6853.ko")
        mgk_64_device_modules.append("drivers/clk/mediatek/clk-mt6853-apu0.ko")
        mgk_64_device_modules.append("drivers/clk/mediatek/clk-mt6853-apu1.ko")
        mgk_64_device_modules.append("drivers/clk/mediatek/clk-mt6853-apuv.ko")
        mgk_64_device_modules.append("drivers/clk/mediatek/clk-mt6853-apuc.ko")
        mgk_64_device_modules.append("drivers/clk/mediatek/clk-mt6853-audsys.ko")
        mgk_64_device_modules.append("drivers/clk/mediatek/clk-mt6853-cam_m.ko")
        mgk_64_device_modules.append("drivers/clk/mediatek/clk-mt6853-cam_ra.ko")
        mgk_64_device_modules.append("drivers/clk/mediatek/clk-mt6853-cam_rb.ko")
        mgk_64_device_modules.append("drivers/clk/mediatek/clk-mt6853-imgsys1.ko")
        mgk_64_device_modules.append("drivers/clk/mediatek/clk-mt6853-imgsys2.ko")
        mgk_64_device_modules.append("drivers/clk/mediatek/clk-mt6853-impc.ko")
        mgk_64_device_modules.append("drivers/clk/mediatek/clk-mt6853-impe.ko")
        mgk_64_device_modules.append("drivers/clk/mediatek/clk-mt6853-impn.ko")
        mgk_64_device_modules.append("drivers/clk/mediatek/clk-mt6853-imps.ko")
        mgk_64_device_modules.append("drivers/clk/mediatek/clk-mt6853-impw.ko")
        mgk_64_device_modules.append("drivers/clk/mediatek/clk-mt6853-impws.ko")
        mgk_64_device_modules.append("drivers/clk/mediatek/clk-mt6853-ipe.ko")
        mgk_64_device_modules.append("drivers/clk/mediatek/clk-mt6853-mdp.ko")
        mgk_64_device_modules.append("drivers/clk/mediatek/clk-mt6853-mfg.ko")
        mgk_64_device_modules.append("drivers/clk/mediatek/clk-mt6853-mm.ko")
        mgk_64_device_modules.append("drivers/clk/mediatek/clk-mt6853-scp_par.ko")
        mgk_64_device_modules.append("drivers/clk/mediatek/clk-mt6853-vdec.ko")
        mgk_64_device_modules.append("drivers/clk/mediatek/clk-mt6853-venc.ko")
        mgk_64_device_modules.append("drivers/clk/mediatek/clk-chk-mt6853.ko")
        mgk_64_device_modules.append("drivers/clk/mediatek/clk-dbg-mt6853.ko")
        mgk_64_device_modules.append("drivers/interconnect/mediatek/mmqos-mt6853.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/eemgpu/mtk_eem.ko")

        mgk_64_platform_device_modules.pop("drivers/clk/mediatek/clk-chk-mt6989.ko")
        mgk_64_platform_device_modules.pop("drivers/clk/mediatek/pd-chk-mt6989.ko")
        mgk_64_platform_device_modules.pop("drivers/clk/mediatek/clk-chk-mt6991.ko")
        mgk_64_platform_device_modules.pop("drivers/clk/mediatek/pd-chk-mt6991.ko")

        mgk_64_common_eng_modules.remove("drivers/firmware/arm_ffa/ffa-module.ko")
        mgk_64_common_userdebug_modules.remove("drivers/firmware/arm_ffa/ffa-module.ko")
        mgk_64_common_user_modules.remove("drivers/firmware/arm_ffa/ffa-module.ko")

        mgk_64_device_modules.append("drivers/tee/teei/515/isee.ko")

        mgk_64_device_modules.append("drivers/tee/gud/600/MobiCoreDriver/mcDrvModule.ko")
        mgk_64_device_modules.append("drivers/tee/gud/600/MobiCoreDriver/mcDrvModule-ffa.ko")
        mgk_64_device_modules.append("drivers/tee/gud/600/TlcTui/t-base-tui.ko")

        mgk_64_device_modules.remove("drivers/misc/mediatek/trusted_mem/tmem_ffa.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/trusted_mem/ffa_v11.ko")
        mgk_64_device_modules.remove("drivers/tee/teei/510/isee-ffa.ko")

        mgk_64_device_modules.append("drivers/regulator/mt6362-regulator.ko")
        mgk_64_device_modules.append("drivers/leds/leds-mt6362.ko")
        mgk_64_device_modules.append("drivers/mfd/mt6362-core.ko")
        mgk_64_device_modules.remove("drivers/iommu/arm/arm-smmu-v3/arm_smmu_v3.ko")
        mgk_64_device_modules.remove("drivers/iommu/arm/arm-smmu-v3/mtk-smmuv3-pmu.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/iommu/arm/arm-smmu-v3:mtk-smmuv3-mpam-mon".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/iommu:smmu_secure".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/iommu:mtk_smmu_qos".format(kernel_version))

        mgk_64_device_modules.append("drivers/misc/mediatek/sensor/2.0/mtk_nanohub/nanohub.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/sensor/2.0/sensorhub/sensorhub.ko")

        mgk_64_device_modules.remove("drivers/misc/mediatek/vcp/rv/vcp.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/vcp/rv/vcp_status.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/vdec_fmt/vdec-fmt.ko")
        mgk_64_device_modules.remove("drivers/media/platform/mtk-vcodec/mtk-vcodec-dec-v2.ko")
        mgk_64_device_modules.remove("drivers/media/platform/mtk-vcodec/mtk-vcodec-enc-v2.ko")
        mgk_64_device_modules.remove("drivers/soc/mediatek/mtk-mmdvfs-v3.ko")
        mgk_64_device_modules.append("drivers/soc/mediatek/devmpu/devmpu.ko")

        mgk_64_device_modules.append("drivers/misc/mediatek/iommu/iommu_gz.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/pkvm_tmem/pkvm_tmem.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/sspm/v2/sspm.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/sspm/v3/sspm_v3.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/regulator:mt6370-regulator".format(kernel_version))
        #mgk_64_platform_device_modules.pop("drivers/misc/mediatek/lpm/modules/debug/mt6989/mtk-lpm-dbg-mt6989.ko")
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/lpm/modules/debug/mt6991:mtk-lpm-dbg-mt6991".format(kernel_version))
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/bridge/mtk_mbraink_bridge.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/mtk_mbraink.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/modules/v6989/mtk_mbraink_v6989.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/modules/v6991/mtk_mbraink_v6991.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/modules/v6899/mtk_mbraink_v6899.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/pmsr:pmsr".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/pmsr/v2:pmsr_v2".format(kernel_version))
        mgk_64_device_modules.remove("drivers/misc/mediatek/pmsr/v3/pmsr_v3.ko")

        mgk_64_device_modules.append("drivers/misc/mediatek/thermal/thermal_monitor.ko")
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

        mgk_64_platform_device_modules.pop("drivers/gpu/drm/mediatek/mml/mtk-mml-mt6878.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/drm/mediatek/mml/mtk-mml-mt6886.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/drm/mediatek/mml/mtk-mml-mt6897.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/drm/mediatek/mml/mtk-mml-mt6899.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/drm/mediatek/mml/mtk-mml-mt6983.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/drm/mediatek/mml/mtk-mml-mt6985.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/drm/mediatek/mml/mtk-mml-mt6989.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/drm/mediatek/mml/mtk-mml-mt6991.ko")

        mgk_64_device_modules.remove("drivers/gpu/drm/mediatek/dpc/dpc_v1/mtk_dpc_v1.ko")
        mgk_64_device_modules.remove("drivers/gpu/drm/mediatek/dpc/dpc_v1/mtk_vdisp_v1.ko")
        mgk_64_device_modules.remove("drivers/gpu/drm/mediatek/dpc/dpc_v2/mtk_dpc_v2.ko")
        mgk_64_device_modules.remove("drivers/gpu/drm/mediatek/dpc/dpc_v2/mtk_vdisp_v2.ko")

        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v1/mtk-swpm-dbg-common-v1.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug/v1:mtk-swpm-isp-wrapper".format(kernel_version))
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
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6991/mtk-swpm-dbg-v6991.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6991/mtk-swpm-cpu-dbg-v6991.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/mtk-swpm.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm:mtk-swpm-perf-arm-pmu".format(kernel_version))
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6897/mtk-swpm-audio-dbg-v6897.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6989/mtk-swpm-audio-dbg-v6989.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6989/mtk-swpm-core-dbg-v6989.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6989/mtk-swpm-cpu-dbg-v6989.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6989/mtk-swpm-dbg-v6989.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6989/mtk-swpm-disp-dbg-v6989.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6989/mtk-swpm-mem-dbg-v6989.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6989/mtk-swpm-mml-dbg-v6989.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6991/mtk-swpm-disp-dbg-v6991.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6991/mtk-swpm-audio-dbg-v6991.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6991/mtk-swpm-mml-dbg-v6991.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6991/mtk-swpm-isp-dbg-v6991.ko")

        mgk_64_device_modules.remove("drivers/misc/mediatek/imgsensor/src/isp6s/imgsensor_isp6s.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/imgsensor/src/isp6s_mou/imgsensor_isp6s_mou.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/cam_cal/src/custom/camera_eeprom.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cam_cal/src/isp6s_mou/camera_eeprom_isp6s_mou.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cameraisp/dip/isp_6s/camera_dip_isp6s.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cameraisp/mfb/camera_mfb_isp6s.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cameraisp/dpe/camera_dpe_isp60.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cameraisp/fdvt/camera_fdvt_isp51.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cameraisp/rsc/camera_rsc_isp6s.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cameraisp/wpe/isp_6s/camera_wpe_isp6s.ko")

        mgk_64_device_modules.remove("drivers/misc/mediatek/tinysys_scmi/tinysys-scmi.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/cm_mgr/mtk_cm_ipi.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/cm_mgr/mtk_cm_mgr.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/cm_mgr/mtk_cm_mgr_mt6886.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/cm_mgr/mtk_cm_mgr_mt6897.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/cm_mgr/mtk_cm_mgr_mt6983.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/cm_mgr/mtk_cm_mgr_mt6985.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/cm_mgr/mtk_cm_mgr_mt6989.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/cm_mgr/mtk_cm_mgr_mt6991.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mminfra/mtk-mminfra-debug.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mminfra/mtk-mminfra-imax.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/ssc/debug/v1/mtk-ssc-dbg-v1.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/ssc/debug/v2/mtk-ssc-dbg-v2.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/ssc/mtk-ssc.ko")
        mgk_64_device_modules.remove("drivers/memory/mediatek/emi.ko")
        mgk_64_device_modules.remove("drivers/memory/mediatek/emi-fake-eng.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/memory/mediatek:emi-mpu".format(kernel_version))
        mgk_64_device_modules.remove("drivers/memory/mediatek/emi-mpu-test.ko")
        mgk_64_device_modules.remove("drivers/memory/mediatek/emi-mpu-test-v2.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/memory/mediatek:emi-slb".format(kernel_version))
        mgk_64_device_modules.remove("drivers/memory/mediatek/smpu.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/memory/mediatek:slc-parity".format(kernel_version))
        mgk_64_device_modules.remove("drivers/memory/mediatek/smpu-hook-v1.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/slbc/mmsram.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/slbc:mtk_slbc".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/slbc:slbc_ipi".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/slbc:slbc_trace".format(kernel_version))
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/slbc/slbc_mt6886.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/slbc/slbc_mt6893.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/slbc/slbc_mt6895.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/slbc/slbc_mt6897.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/slbc/slbc_mt6899.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/slbc/slbc_mt6983.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/slbc/slbc_mt6985.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/slbc/slbc_mt6989.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/slbc/slbc_mt6991.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi_legacy_v2/emi.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi_legacy_v2/emi-mpu.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi_legacy_v2/emi-mpu-test.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi-dummy.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi_legacy_v2/emi-mpu-hook-v1.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi_legacy_v2/emi-slb.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi_legacy_v2/emi-mpu-test-v2.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi_legacy_v2/emi-fake-eng.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi_legacy_v2/emi-mpu-v2.ko")
        mgk_64_device_modules.append("drivers/gpu/mediatek/gpufreq/v2_legacy/mtk_gpufreq_wrapper_legacy.ko")
        mgk_64_device_modules.remove("drivers/gpu/mediatek/gpueb/mtk_gpueb.ko")
        mgk_64_device_modules.remove("drivers/gpu/mediatek/gpueb/mtk_ghpm.ko")
        mgk_64_device_modules.remove("drivers/gpu/mediatek/gpueb/mtk_ghpm_swwa.ko")
        mgk_64_device_modules.remove("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_wrapper.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6886.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6897.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6899.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6985.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6989.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6989_fpga.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6991.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6993.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpu_pdma/mtk_gpu_pdma_mt6991.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/performance/mtk_perf_ioctl_magt.ko")
        mgk_64_platform_device_modules.update({"drivers/misc/mediatek/cm_mgr_legacy_v1/mtk_cm_mgr_mt6853.ko":"mt6853"})
        mgk_64_device_modules.append("drivers/misc/mediatek/cm_mgr_legacy_v1/mtk_cm_mgr.ko")
        mgk_64_device_modules.append("sound/soc/mediatek/mt6853/snd-soc-mt6853-afe.ko")
        mgk_64_device_modules.append("sound/soc/mediatek/mt6853/mt6853-mt6359p.ko")
        mgk_64_device_modules.append("sound/soc/codecs/snd-soc-mt6660.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/adsp/adsp.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/adsp/v1/adsp-v1.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/adsp/v2/adsp-v2.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/adsp/v3/adsp-v3.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/audio_ipi/audio_ipi.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/usb/usb_offload/usb_offload.ko")
        mgk_64_device_modules.remove("sound/soc/mediatek/audio_dsp/mtk-soc-offload-common.ko")
        mgk_64_device_modules.remove("sound/soc/mediatek/audio_dsp/snd-soc-audiodsp-common.ko")
        mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6985/mt6985-mt6338.ko")
        mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6985/snd-soc-mt6985-afe.ko")
        mgk_64_device_modules.remove("sound/soc/mediatek/ultrasound/ultra_common/mtk-scp-ultra.ko")
        mgk_64_device_modules.remove("sound/soc/mediatek/ultrasound/ultra_scp/snd-soc-mtk-scp-ultra.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/vow/ver02/mtk-vow.ko")
        mgk_64_device_modules.remove("sound/soc/mediatek/vow/mtk-scp-vow.ko")

        mgk_64_device_modules.append("drivers/misc/mediatek/leakage_table_v2/mediatek_static_power.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/ppm_v3/mtk_ppm_v3.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cpuhotplug/mtk_cpuhp.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cpufreq_v2/src/CPU_DVFS.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cpufreq_v2/src/plat_k6853/mtk_cpufreq_utils.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/upower/Upower.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/lpm_legacy/modules/platform/v1/mtk-lpm-plat-v1-legacy.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/lpm_legacy/modules/debug/k6853/mtk-lpm-dbg-mt6853-legacy.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/lpm_legacy/mtk-lpm-legacy.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/lpm_legacy/modules/debug/v1/mtk-lpm-dbg-common-v1-legacy.ko")

    if "mt6781_overlay.config" in DEFCONFIG_OVERLAYS:
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/mtkcam/sched:c2ps")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/mtkcam/sched/c2ps_ioctl:c2ps_perf_ioctl")
        mgk_64_device_modules.append("drivers/regulator/mt6358-regulator.ko")
        mgk_64_device_modules.append("drivers/regulator/mtk-pmic-oc-debug.ko")
        mgk_64_device_modules.append("//kernel_device_modules-{}/drivers/misc/mediatek/dvfsrc:dvfsrc-opp-mt6781".format(kernel_version))
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/bridge/mtk_mbraink_bridge.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/mtk_mbraink.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/modules/v6989/mtk_mbraink_v6989.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/modules/v6991/mtk_mbraink_v6991.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/modules/v6899/mtk_mbraink_v6899.ko")
        mgk_64_device_modules.remove("drivers/spmi/spmi-mtk-mpu.ko")
        mgk_64_device_modules.remove("drivers/spmi/spmi-mtk-pmif.ko")
        mgk_64_device_modules.remove("drivers/gpu/drm/mediatek/dpc/dpc_v1/mtk_dpc_v1.ko")
        mgk_64_device_modules.remove("drivers/gpu/drm/mediatek/dpc/dpc_v1/mtk_vdisp_v1.ko")
        mgk_64_device_modules.remove("drivers/gpu/drm/mediatek/dpc/dpc_v2/mtk_dpc_v2.ko")
        mgk_64_device_modules.remove("drivers/gpu/drm/mediatek/dpc/dpc_v2/mtk_vdisp_v2.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/sspm/v2/sspm.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/sspm/v3/sspm_v3.ko")
        #mgk_64_platform_device_modules.pop("drivers/misc/mediatek/lpm/modules/debug/mt6989/mtk-lpm-dbg-mt6989.ko")
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/lpm/modules/debug/mt6991:mtk-lpm-dbg-mt6991".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/pmsr:pmsr".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/pmsr/v2:pmsr_v2".format(kernel_version))
        mgk_64_device_modules.remove("drivers/misc/mediatek/pmsr/v3/pmsr_v3.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v1/mtk-swpm-dbg-common-v1.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug/v1:mtk-swpm-isp-wrapper".format(kernel_version))
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
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6991/mtk-swpm-dbg-v6991.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6991/mtk-swpm-cpu-dbg-v6991.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/mtk-swpm.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm:mtk-swpm-perf-arm-pmu".format(kernel_version))
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6897/mtk-swpm-audio-dbg-v6897.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6989/mtk-swpm-audio-dbg-v6989.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6989/mtk-swpm-core-dbg-v6989.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6989/mtk-swpm-cpu-dbg-v6989.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6989/mtk-swpm-dbg-v6989.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6989/mtk-swpm-disp-dbg-v6989.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6989/mtk-swpm-mem-dbg-v6989.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6989/mtk-swpm-mml-dbg-v6989.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6991/mtk-swpm-disp-dbg-v6991.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6991/mtk-swpm-audio-dbg-v6991.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6991/mtk-swpm-mml-dbg-v6991.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6991/mtk-swpm-isp-dbg-v6991.ko")

        mgk_64_device_modules.remove("drivers/misc/mediatek/imgsensor/src/isp6s/imgsensor_isp6s.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/imgsensor/src/isp6s_lag/imgsensor_isp6s_lag.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/cam_cal/src/custom/camera_eeprom.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cam_cal/src/isp6s_lag/camera_eeprom_isp6s_lag.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cameraisp/fdvt/camera_fdvt_isp51_v1.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cameraisp/rsc/camera_rsc_isp6s_v1.ko")

        mgk_64_device_modules.remove("drivers/misc/mediatek/tinysys_scmi/tinysys-scmi.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/cm_mgr/mtk_cm_ipi.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/cm_mgr/mtk_cm_mgr.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/cm_mgr/mtk_cm_mgr_mt6886.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/cm_mgr/mtk_cm_mgr_mt6897.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/cm_mgr/mtk_cm_mgr_mt6983.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/cm_mgr/mtk_cm_mgr_mt6985.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/cm_mgr/mtk_cm_mgr_mt6989.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/cm_mgr/mtk_cm_mgr_mt6991.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mminfra/mtk-mminfra-debug.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mminfra/mtk-mminfra-imax.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/ssc/debug/v1/mtk-ssc-dbg-v1.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/ssc/debug/v2/mtk-ssc-dbg-v2.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/ssc/mtk-ssc.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/slbc/mmsram.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/slbc:mtk_slbc".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/slbc:slbc_ipi".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/slbc:slbc_trace".format(kernel_version))
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/slbc/slbc_mt6886.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/slbc/slbc_mt6893.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/slbc/slbc_mt6895.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/slbc/slbc_mt6897.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/slbc/slbc_mt6899.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/slbc/slbc_mt6983.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/slbc/slbc_mt6985.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/slbc/slbc_mt6989.ko")
        mgk_64_platform_device_modules.pop("drivers/misc/mediatek/slbc/slbc_mt6991.ko")
        mgk_64_platform_device_modules.update({"drivers/misc/mediatek/cm_mgr_legacy_v1/mtk_cm_mgr_mt6781.ko":"mt6781"})
        mgk_64_device_modules.append("drivers/misc/mediatek/cm_mgr_legacy_v1/mtk_cm_mgr.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cameraisp/dip/isp_6s/camera_dip_isp6s.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cameraisp/mfb/camera_mfb_isp6s.ko")

    if "mt8786p2_overlay.config" in DEFCONFIG_OVERLAYS:
        mgk_64_device_modules.append("drivers/video/backlight/sgm37604a.ko")

get_overlay_modules_list()
