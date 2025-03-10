load("//kernel_device_modules-6.12:kernel/kleaf/mgk_modules.bzl",

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

mgk_64_defconfig = "mgk_64_k612_defconfig"

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
    "//vendor/mediatek/kernel_modules/connectivity/fmradio:fmradio-connac2",
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
    "//vendor/mediatek/kernel_modules/connectivity/wlan/core/gen4m/build/connac1x/6789:wlan_drv_gen4m_6789",
    "//vendor/mediatek/kernel_modules/connectivity/wlan/core/gen4m/build/connac1x/6855:wlan_drv_gen4m_6855",
    "//vendor/mediatek/kernel_modules/connectivity/wlan/core/gen4m/build/connac3x/6991_6653:wlan_drv_gen4m_6991_6653",
    "//vendor/mediatek/kernel_modules/connectivity/wlan/core/gen4m/build/connac3x/6991_6653_dx5:wlan_drv_gen4m_6991_6653_dx5",
    "//vendor/mediatek/kernel_modules/connectivity/wlan/core/gen4m/build/connac3x/6991_6653_dx5_triband:wlan_drv_gen4m_6991_6653_dx5_triband",
    "//vendor/mediatek/kernel_modules/connectivity/wlan/core/gen4m/build/connac3x/6993_6653:wlan_drv_gen4m_6993_6653",
    "//vendor/mediatek/kernel_modules/connectivity/wlan/core/gen4m/build/connac3x/6993_6653_triband:wlan_drv_gen4m_6993_6653_triband",
    "//vendor/mediatek/kernel_modules/cpufreq_cus:cpufreq_cus",
    "//vendor/mediatek/kernel_modules/cpufreq_int:cpufreq_int",
    "//vendor/mediatek/kernel_modules/fpsgo_cus:fpsgo_cus",
    "//vendor/mediatek/kernel_modules/fpsgo_int:fpsgo_int",
    "//vendor/mediatek/kernel_modules/afs_common_utils:jank_detection_common_utils",
    "//vendor/mediatek/kernel_modules/afs_core_int:jank_detection_core_int",
    "//vendor/mediatek/kernel_modules/afs_core_cus:jank_detection_core_cus",
    "//vendor/mediatek/kernel_modules/hbt_driver_cus:hbt_cus",
    "//vendor/mediatek/kernel_modules/hbt_driver:hbt_int",
    "//vendor/mediatek/kernel_modules/met_drv_secure_v3:met_drv_secure_v3",
    "//vendor/mediatek/kernel_modules/met_drv_v3/met_api/met_backlight_api:met_backlight_api_int",
    "//vendor/mediatek/kernel_modules/met_drv_v3/met_api/met_emi_api:met_emi_api_int",
    "//vendor/mediatek/kernel_modules/met_drv_v3/met_api/met_gpu_adv_api:met_gpu_adv_api_int",
    "//vendor/mediatek/kernel_modules/met_drv_v3/met_api/met_gpu_api:met_gpu_api_int",
    "//vendor/mediatek/kernel_modules/met_drv_v3/met_api/met_gpueb_api:met_gpueb_api_int",
    "//vendor/mediatek/kernel_modules/met_drv_v3/met_api/met_ipi_api:met_ipi_api_int",
    "//vendor/mediatek/kernel_modules/met_drv_v3/met_api/met_mcupm_api:met_mcupm_api_int",
    "//vendor/mediatek/kernel_modules/met_drv_v3/met_api/met_scmi_api:met_scmi_api_int",
    "//vendor/mediatek/kernel_modules/met_drv_v3/met_api/met_sspm_api:met_sspm_api_int",
    "//vendor/mediatek/kernel_modules/met_drv_v3/met_api/met_vcore_api:met_vcore_api_int",
    "//vendor/mediatek/kernel_modules/met_drv_v3/met_api/met_backlight_api:met_backlight_api_cus",
    "//vendor/mediatek/kernel_modules/met_drv_v3/met_api/met_emi_api:met_emi_api_cus",
    "//vendor/mediatek/kernel_modules/met_drv_v3/met_api/met_gpu_adv_api:met_gpu_adv_api_cus",
    "//vendor/mediatek/kernel_modules/met_drv_v3/met_api/met_gpu_api:met_gpu_api_cus",
    "//vendor/mediatek/kernel_modules/met_drv_v3/met_api/met_gpueb_api:met_gpueb_api_cus",
    "//vendor/mediatek/kernel_modules/met_drv_v3/met_api/met_ipi_api:met_ipi_api_cus",
    "//vendor/mediatek/kernel_modules/met_drv_v3/met_api/met_mcupm_api:met_mcupm_api_cus",
    "//vendor/mediatek/kernel_modules/met_drv_v3/met_api/met_scmi_api:met_scmi_api_cus",
    "//vendor/mediatek/kernel_modules/met_drv_v3/met_api/met_sspm_api:met_sspm_api_cus",
    "//vendor/mediatek/kernel_modules/met_drv_v3/met_api/met_vcore_api:met_vcore_api_cus",
    "//vendor/mediatek/kernel_modules/met_drv_v3:met_drv_v3",
    #"//vendor/mediatek/kernel_modules/msync2_frd_cus/build:msync2_frd_cus",
    #"//vendor/mediatek/kernel_modules/msync2_frd_int:msync2_frd_int",
    "//vendor/mediatek/kernel_modules/mtk_input/FT3518:ft3518",
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
    "//vendor/mediatek/kernel_modules/mtkcam/camsys:mtk-cam-isp",
    "//vendor/mediatek/kernel_modules/mtkcam/camsys:mtk-cam-isp7s",
    "//vendor/mediatek/kernel_modules/mtkcam/camsys:mtk-cam-isp7sp",
    "//vendor/mediatek/kernel_modules/mtkcam/camsys:mtk-cam-isp8",
    "//vendor/mediatek/kernel_modules/mtkcam/camsys:mtk-cam-isp8s",
    "//vendor/mediatek/kernel_modules/mtkcam/camsys:mtk-cam-plat-util",
    "//vendor/mediatek/kernel_modules/mtkcam/camsys:cam-isp7_1-ut",
    "//vendor/mediatek/kernel_modules/mtkcam/camsys:cam-isp7s-ut",
    "//vendor/mediatek/kernel_modules/mtkcam/camsys:cam-isp7sp-ut",
    "//vendor/mediatek/kernel_modules/mtkcam/camsys:cam-isp8-ut",
    "//vendor/mediatek/kernel_modules/mtkcam/camsys:cam-isp8s-ut",
    "//vendor/mediatek/kernel_modules/mtkcam/mtk-pda:pda_drv_dummy",
    "//vendor/mediatek/kernel_modules/mtkcam/cam_cal/src_v4l2:mtk_cam_cal",
    "//vendor/mediatek/kernel_modules/mtkcam/ccusys:mtk_ccuv",
    "//vendor/mediatek/kernel_modules/mtkcam/imgsys/common:imgsys_common",
    # "//vendor/mediatek/kernel_modules/mtkcam/imgsys/imgsys/isp8s:imgsys_8s",
    "//vendor/mediatek/kernel_modules/mtkcam/imgsys/imgsys/isp8:imgsys_8",
    "//vendor/mediatek/kernel_modules/mtkcam/imgsys/imgsys/isp7sp:imgsys_7sp",
    "//vendor/mediatek/kernel_modules/mtkcam/imgsys/imgsys/isp7s:imgsys_7s",
    "//vendor/mediatek/kernel_modules/mtkcam/imgsys/imgsys/isp71:imgsys_71",
    "//vendor/mediatek/kernel_modules/mtkcam/imgsys/imgsys/cmdq/legacy:imgsys_cmdq",
    "//vendor/mediatek/kernel_modules/mtkcam/imgsys/imgsys/cmdq/isp8:imgsys_cmdq_isp8",
    "//vendor/mediatek/kernel_modules/mtkcam/imgsys/imgsys/cmdq/isp8s:imgsys_cmdq_isp8s",
    "//vendor/mediatek/kernel_modules/mtkcam/mtk-aie:mtk-aie",
    "//vendor/mediatek/kernel_modules/mtkcam/mtk-aie:mtk-aie-debug-7sp",
    "//vendor/mediatek/kernel_modules/mtkcam/mtk-aie:mtk-aie-debug-7sp-1",
    "//vendor/mediatek/kernel_modules/mtkcam/mtk-aie:mtk-aie-debug-7sp-2",
    "//vendor/mediatek/kernel_modules/mtkcam/mtk-mae:mtk_mae",
    "//vendor/mediatek/kernel_modules/mtkcam/mtk-mae:mtk_mae_isp8",
    "//vendor/mediatek/kernel_modules/mtkcam/mtk-hcp/legacy:mtk-hcp",
    "//vendor/mediatek/kernel_modules/mtkcam/mtk-hcp/isp8:mtk-hcp-isp8",
    "//vendor/mediatek/kernel_modules/mtkcam/mtk-hcp/isp8s:mtk-hcp-isp8s",
    "//vendor/mediatek/kernel_modules/mtkcam/mtk-ipesys-me:mtk-ipesys-me",
    "//vendor/mediatek/kernel_modules/mtkcam/sched:c2ps",
    "//vendor/mediatek/kernel_modules/mtkcam/sched:c2ps_perf_ioctl",
    "//vendor/mediatek/kernel_modules/mtkcam/scpsys/mtk-aov:mtk_aov",
    "//vendor/mediatek/kernel_modules/mtkcam/isp_pspm:isp_pspm",
    "//vendor/mediatek/kernel_modules/sched_cus:eas_ext_cus",
    "//vendor/mediatek/kernel_modules/sched_cus:cpuqos_ext_cus",
    "//vendor/mediatek/kernel_modules/sched_cus:mtk_em_cus",
    #"//vendor/mediatek/kernel_modules/sched_int:sched_int",
    "//vendor/mediatek/kernel_modules/sched_int:cpuqos_dbg_int",
    "//vendor/mediatek/kernel_modules/sched_int:eas_ext_int",
    "//vendor/mediatek/kernel_modules/sched_int:cci_dbg_int",
    "//vendor/mediatek/kernel_modules/sched_int:cci_dbg_cus_int",
    "//vendor/mediatek/kernel_modules/sched_int:cpuqos_ext_int",
    "//vendor/mediatek/kernel_modules/sched_int:eas_dbg_int",
    "//vendor/mediatek/kernel_modules/sched_int:mtk_sched_test_int",
    "//vendor/mediatek/kernel_modules/sched_int:mtk_em_int",
    "//vendor/mediatek/kernel_modules/mtkcam/img_frm_sync:mtk-img-frm-sync",
    "//vendor/mediatek/kernel_modules/task_turbo_cus:task_turbo_cus",
    "//vendor/mediatek/kernel_modules/task_turbo_int:task_turbo_int",
    "//vendor/mediatek/kernel_modules/mtkcam/imgsensor/src-v4l2/imgsensor-glue:imgsensor-glue",
    "//vendor/mediatek/kernel_modules/mtkcam/imgsensor/src-isp8/imgsensor-glue:imgsensor-glue_isp8",
    "//vendor/mediatek/kernel_modules/mtkcam/imgsensor/src_spm-isp8/imgsensor-glue:imgsensor-glue_spm_isp8",
    "//vendor/mediatek/kernel_modules/mtkcam/imgsensor/src-isp8s/imgsensor-glue:imgsensor-glue_isp8s",
    "//vendor/mediatek/kernel_modules/mtkcam/imgsensor/src-v4l2:imgsensor",
    "//vendor/mediatek/kernel_modules/mtkcam/imgsensor/src-isp8:imgsensor_isp8",
    "//vendor/mediatek/kernel_modules/mtkcam/imgsensor/src_spm-isp8:imgsensor_spm_isp8",
    "//vendor/mediatek/kernel_modules/mtkcam/imgsensor/src-isp8s:imgsensor_isp8s",
    "//vendor/mediatek/kernel_modules/perf_common_cus:perf_common_cus",
    "//vendor/mediatek/kernel_modules/perf_common_int:perf_common_int",
    "//vendor/mediatek/kernel_modules/game_cus:game_cus",
    "//vendor/mediatek/kernel_modules/game_int:game_int",
]

mgk_64_kleaf_eng_modules = [
    "//vendor/mediatek/tests/kernel/ktf_testcase:ktf_testcase",
    "//vendor/mediatek/tests/kernel/ktf_testcase/ccmni:ktf_ccmni_ut",
    "//vendor/mediatek/tests/kernel/ktf_testcase/efuse/efuse_ut:ktf_efuse_ut",
    "//vendor/mediatek/tests/kernel/ktf_testcase/example:ktf_hello",
    "//vendor/mediatek/tests/kernel/ktf_testcase/mmc:ktf_mmc_ut",
    "//vendor/mediatek/tests/kernel/ktf_testcase/mkp:ktf_mkp_ut",
    "//vendor/mediatek/tests/kernel/ktf_testcase/mkp/mkp_ait:ktf_mkp_ait",
    "//vendor/mediatek/tests/kernel/ktf_testcase/mkp/mkp_it:ktf_mkp_it",
    "//vendor/mediatek/tests/kernel/ktf_testcase/selftest:ktf_selftest",
    "//vendor/mediatek/tests/kernel/ktf_testcase/apusys/ipifuzz_apusys:ktf_apusys_ipifuzz",
    "//vendor/mediatek/tests/ktf/kernel:ktf_ddk",
    "//vendor/mediatek/tests/kernel/ktf_testcase/atf_fuzzer:atf_fuzzer",
    "//vendor/mediatek/tests/kernel/ktf_testcase/accdet/accdet_ait:accdet_ait",
    "//vendor/mediatek/tests/kernel/ktf_testcase/accdet/accdet_ait:accdet_ait_fuzz",
    "//vendor/mediatek/tests/kernel/ktf_testcase/dma_buf/dma_buf_ait:ktf_dma_buf_ait",
    "//vendor/mediatek/tests/kernel/ktf_testcase/dma_buf:ktf_dma_buf",
    "//vendor/mediatek/tests/kernel/ktf_testcase/emi_slc:ktf_emi_slc_ut",
    "//vendor/mediatek/tests/kernel/ktf_testcase/flashlight:ktf_flashlight",
    "//vendor/mediatek/tests/kernel/ktf_testcase/i2c/i2c_ait:ktf_i2c_ait",
    "//vendor/mediatek/tests/kernel/ktf_testcase/i2c:ktf_i2c",
    "//vendor/mediatek/tests/kernel/ktf_testcase/keyboard:ktf_keyboard",
    "//vendor/mediatek/tests/kernel/ktf_testcase/pinctrl:ktf_pinctrl_it",
    "//vendor/mediatek/tests/kernel/ktf_testcase/touch:ktf_touch",
    "//vendor/mediatek/tests/kernel/ktf_testcase/uart:ktf_uart",
    "//vendor/mediatek/tests/kernel/ktf_testcase/typec:ktf_i2c_suspend",
    "//vendor/mediatek/tests/kernel/ktf_testcase/i3c:ktf_i3c",
    "//vendor/mediatek/tests/kernel/ktf_testcase/iommu:ktf_iommu",
    "//vendor/mediatek/tests/kernel/ktf_testcase/iommu/iommu_ait:ktf_iommu_ait",
    "//vendor/mediatek/tests/kernel/ktf_testcase/leds_mtk:ktf_leds_mtk_test",
    "//vendor/mediatek/tests/kernel/ktf_testcase/log_store:ktf_log_store",
    "//vendor/mediatek/tests/kernel/ktf_testcase/pw_charger:ktf_pw_charger",
    "//vendor/mediatek/tests/kernel/ktf_testcase/pw_gauge:ktf_pw_gauge_it",
    "//vendor/mediatek/tests/kernel/ktf_testcase/pmqos:ktf_pmqos",
    "//vendor/mediatek/tests/kernel/ktf_testcase/sched:ktf_sched_ut",
    "//vendor/mediatek/tests/kernel/ktf_testcase/selinux:ktf_selinux",
    "//vendor/mediatek/tests/kernel/ktf_testcase/sensorhub/sensorhub_ait:ktf_sensorhub_ait",
    "//vendor/mediatek/tests/kernel/ktf_testcase/sensorhub:ktf_test_af_z",
    "//vendor/mediatek/tests/kernel/ktf_testcase/sensorhub:ktf_test_app",
    "//vendor/mediatek/tests/kernel/ktf_testcase/sensorhub:ktf_sensorhub_UT",
    "//vendor/mediatek/tests/kernel/ktf_testcase/sensorhub:ktf_sensorhub_IT",
    "//vendor/mediatek/tests/kernel/ktf_testcase/slbc:ktf_slbc_it",
    "//vendor/mediatek/tests/kernel/ktf_testcase/smi:ktf_smi",
    "//vendor/mediatek/tests/kernel/ktf_testcase/smi/smi_ait:ktf_smi_ait",
    "//vendor/mediatek/tests/kernel/ktf_testcase/mmqos:ktf_mmqos",
    #"//vendor/mediatek/tests/kernel/ktf_testcase/cmdq_gce/cmdq_gce_ait:ktf_cmdq_ait",
    "//vendor/mediatek/tests/kernel/ktf_testcase/mml:ktf_mml_ait",
    "//vendor/mediatek/tests/kernel/ktf_testcase/mtprintk:ktf_mtprintk",
    "//vendor/mediatek/tests/kernel/ktf_testcase/trusted_mem/trusted_mem_2307:2307",
    "//vendor/mediatek/tests/kernel/ktf_testcase/trusted_mem/trusted_mem_ait:ktf_trusted_mem_ait",
    "//vendor/mediatek/tests/kernel/ktf_testcase/atflog:ktf_atflog",
    "//vendor/mediatek/tests/kernel/ktf_testcase/spi:ktf_spi",
    "//vendor/mediatek/tests/kernel/ktf_testcase/emmc:ktf_emmc",
    "//vendor/mediatek/tests/kernel/ktf_testcase/masp:ktf_masp",
    "//vendor/mediatek/tests/kernel/ktf_testcase/ufs:ktf_ufs",
    #"//vendor/mediatek/tests/kernel/ktf_testcase/cmdq_gce_wbgai:ktf_cmdq_wbgai",
    "//vendor/mediatek/tests/kernel/ktf_testcase/hybrid:ktf_hybrid_demo",
    "//vendor/mediatek/tests/kernel/ktf_testcase/usb_device_class_ut:ktf_usb_device_class_ut",
    "//vendor/mediatek/tests/kernel/ktf_testcase/usb_type_c_ut:ktf_usb_type_c_ut",
    "//vendor/mediatek/tests/kernel/ktf_testcase/adsp:ktf_adsp_ut",
    "//vendor/mediatek/tests/kernel/ktf_testcase/adsp:ktf_adsp_ipifuzz",
    "//vendor/mediatek/tests/kernel/ktf_testcase/scp:ktf_scp",
    "//vendor/mediatek/tests/kernel/ktf_testcase/vcp:ktf_vcp",
    "//vendor/mediatek/tests/kernel/ktf_testcase/smmu:ktf_smmu",
    "//vendor/mediatek/tests/kernel/ktf_testcase/smmu/smmu_ait:ktf_smmu_ait",
    "//vendor/mediatek/tests/kernel/ktf_testcase/sspm:ktf_sspm",
    "//vendor/mediatek/tests/kernel/ktf_testcase/sspm/sspm_ait:ktf_sspm_ait",
    "//vendor/mediatek/tests/kernel/ktf_testcase/swpm:mtk_ktf_swpm",
    "//vendor/mediatek/tests/kernel/ktf_testcase/fhctl/fhctl_ut:ktf_fhctl_ut",
    "//vendor/mediatek/tests/kernel/ktf_testcase/cpu_dvfs_ut:ktf_cpu_dvfs_ut",
    "//vendor/mediatek/tests/kernel/ktf_testcase/irq_monitor:ktf_irq_monitor",
    "//vendor/mediatek/tests/kernel/ktf_testcase/power_throttling:ktf_power_throttling",
    "//vendor/mediatek/tests/kernel/ktf_testcase/pbm_mdpm:ktf_pbm_mdpm",
    #"//vendor/mediatek/tests/kernel/ktf_testcase/vcodec/vcodec_ait:ktf_vcodec_ait3",
    #"//vendor/mediatek/tests/kernel/ktf_testcase/vcodec/vcodec_ait:ktf_vcodec_ait2",
    #"//vendor/mediatek/tests/kernel/ktf_testcase/vcodec/vcodec_ait:ktf_vcodec_ait",
    "//vendor/mediatek/tests/kernel/ktf_testcase/mmdvfs:ktf_mmdvfs",
    "//vendor/mediatek/tests/kernel/ktf_testcase/mminfra:ktf_mminfra",
    "//vendor/mediatek/tests/kernel/ktf_testcase/ipifuzz_autogen/adsp:ktf_adsp_ipifuzz_autogen",
    "//vendor/mediatek/tests/kernel/ktf_testcase/ipifuzz_autogen/gpueb:ktf_gpueb_ipifuzz_autogen",
    "//vendor/mediatek/tests/kernel/ktf_testcase/mminfra:ktf_mminfra_wbgai",
    "//vendor/mediatek/tests/kernel/ktf_testcase/display_mobile_wbgai:ktf_display_mobile_wbgai",
    "//vendor/mediatek/tests/kernel/ktf_testcase/display_pq_wbgai:ktf_display_pq_wbgai",
    "//vendor/mediatek/tests/kernel/ktf_testcase/ipifuzz_autogen/vcp:ktf_vcp_ipifuzz_autogen",
    "//vendor/mediatek/tests/kernel/ktf_testcase/vcp/ipifuzz_vcp:ktf_vcp_ipifuzz",
    "//vendor/mediatek/tests/kernel/ktf_testcase/scp/ipifuzz_scp:ktf_sap_ipifuzz",
    "//vendor/mediatek/tests/kernel/ktf_testcase/scp/ipifuzz_scp:ktf_scp_ipifuzz",
    "//vendor/mediatek/tests/kernel/ktf_testcase/ipifuzz_autogen/scp:ktf_scp_ipifuzz_autogen",
    "//vendor/mediatek/tests/kernel/ktf_testcase/thermal:ktf_thermal",
    "//vendor/mediatek/tests/kernel/ktf_testcase/gpu_ext:ktf_gpu_ext_it",
    "//vendor/mediatek/tests/kernel/ktf_testcase/core_ctl:ktf_mtk_core_ctl",
]

mgk_64_kleaf_userdebug_modules = [
    "//vendor/mediatek/tests/kernel/ktf_testcase:ktf_testcase",
    "//vendor/mediatek/tests/kernel/ktf_testcase/ccmni:ktf_ccmni_ut",
    "//vendor/mediatek/tests/kernel/ktf_testcase/efuse/efuse_ut:ktf_efuse_ut",
    "//vendor/mediatek/tests/kernel/ktf_testcase/example:ktf_hello",
    "//vendor/mediatek/tests/kernel/ktf_testcase/mmc:ktf_mmc_ut",
    "//vendor/mediatek/tests/kernel/ktf_testcase/mkp:ktf_mkp_ut",
    "//vendor/mediatek/tests/kernel/ktf_testcase/mkp/mkp_ait:ktf_mkp_ait",
    "//vendor/mediatek/tests/kernel/ktf_testcase/mkp/mkp_it:ktf_mkp_it",
    "//vendor/mediatek/tests/kernel/ktf_testcase/selftest:ktf_selftest",
    "//vendor/mediatek/tests/kernel/ktf_testcase/apusys/ipifuzz_apusys:ktf_apusys_ipifuzz",
    "//vendor/mediatek/tests/ktf/kernel:ktf_ddk",
    "//vendor/mediatek/tests/kernel/ktf_testcase/atf_fuzzer:atf_fuzzer",
    "//vendor/mediatek/tests/kernel/ktf_testcase/accdet/accdet_ait:accdet_ait",
    "//vendor/mediatek/tests/kernel/ktf_testcase/accdet/accdet_ait:accdet_ait_fuzz",
    "//vendor/mediatek/tests/kernel/ktf_testcase/dma_buf/dma_buf_ait:ktf_dma_buf_ait",
    "//vendor/mediatek/tests/kernel/ktf_testcase/dma_buf:ktf_dma_buf",
    "//vendor/mediatek/tests/kernel/ktf_testcase/emi_slc:ktf_emi_slc_ut",
    "//vendor/mediatek/tests/kernel/ktf_testcase/flashlight:ktf_flashlight",
    "//vendor/mediatek/tests/kernel/ktf_testcase/i2c/i2c_ait:ktf_i2c_ait",
    "//vendor/mediatek/tests/kernel/ktf_testcase/i2c:ktf_i2c",
    "//vendor/mediatek/tests/kernel/ktf_testcase/keyboard:ktf_keyboard",
    "//vendor/mediatek/tests/kernel/ktf_testcase/pinctrl:ktf_pinctrl_it",
    "//vendor/mediatek/tests/kernel/ktf_testcase/touch:ktf_touch",
    "//vendor/mediatek/tests/kernel/ktf_testcase/uart:ktf_uart",
    "//vendor/mediatek/tests/kernel/ktf_testcase/typec:ktf_i2c_suspend",
    "//vendor/mediatek/tests/kernel/ktf_testcase/i3c:ktf_i3c",
    "//vendor/mediatek/tests/kernel/ktf_testcase/iommu:ktf_iommu",
    "//vendor/mediatek/tests/kernel/ktf_testcase/iommu/iommu_ait:ktf_iommu_ait",
    "//vendor/mediatek/tests/kernel/ktf_testcase/leds_mtk:ktf_leds_mtk_test",
    "//vendor/mediatek/tests/kernel/ktf_testcase/log_store:ktf_log_store",
    "//vendor/mediatek/tests/kernel/ktf_testcase/pw_charger:ktf_pw_charger",
    "//vendor/mediatek/tests/kernel/ktf_testcase/pw_gauge:ktf_pw_gauge_it",
    "//vendor/mediatek/tests/kernel/ktf_testcase/pmqos:ktf_pmqos",
    "//vendor/mediatek/tests/kernel/ktf_testcase/sched:ktf_sched_ut",
    "//vendor/mediatek/tests/kernel/ktf_testcase/selinux:ktf_selinux",
    "//vendor/mediatek/tests/kernel/ktf_testcase/sensorhub/sensorhub_ait:ktf_sensorhub_ait",
    "//vendor/mediatek/tests/kernel/ktf_testcase/sensorhub:ktf_test_af_z",
    "//vendor/mediatek/tests/kernel/ktf_testcase/sensorhub:ktf_test_app",
    "//vendor/mediatek/tests/kernel/ktf_testcase/sensorhub:ktf_sensorhub_UT",
    "//vendor/mediatek/tests/kernel/ktf_testcase/sensorhub:ktf_sensorhub_IT",
    "//vendor/mediatek/tests/kernel/ktf_testcase/slbc:ktf_slbc_it",
    "//vendor/mediatek/tests/kernel/ktf_testcase/smi:ktf_smi",
    "//vendor/mediatek/tests/kernel/ktf_testcase/smi/smi_ait:ktf_smi_ait",
    "//vendor/mediatek/tests/kernel/ktf_testcase/mmqos:ktf_mmqos",
    #"//vendor/mediatek/tests/kernel/ktf_testcase/cmdq_gce/cmdq_gce_ait:ktf_cmdq_ait",
    "//vendor/mediatek/tests/kernel/ktf_testcase/mml:ktf_mml_ait",
    "//vendor/mediatek/tests/kernel/ktf_testcase/mtprintk:ktf_mtprintk",
    "//vendor/mediatek/tests/kernel/ktf_testcase/trusted_mem/trusted_mem_2307:2307",
    "//vendor/mediatek/tests/kernel/ktf_testcase/trusted_mem/trusted_mem_ait:ktf_trusted_mem_ait",
    "//vendor/mediatek/tests/kernel/ktf_testcase/atflog:ktf_atflog",
    "//vendor/mediatek/tests/kernel/ktf_testcase/spi:ktf_spi",
    "//vendor/mediatek/tests/kernel/ktf_testcase/emmc:ktf_emmc",
    "//vendor/mediatek/tests/kernel/ktf_testcase/masp:ktf_masp",
    "//vendor/mediatek/tests/kernel/ktf_testcase/ufs:ktf_ufs",
    #"//vendor/mediatek/tests/kernel/ktf_testcase/cmdq_gce_wbgai:ktf_cmdq_wbgai",
    "//vendor/mediatek/tests/kernel/ktf_testcase/hybrid:ktf_hybrid_demo",
    "//vendor/mediatek/tests/kernel/ktf_testcase/usb_device_class_ut:ktf_usb_device_class_ut",
    "//vendor/mediatek/tests/kernel/ktf_testcase/usb_type_c_ut:ktf_usb_type_c_ut",
    "//vendor/mediatek/tests/kernel/ktf_testcase/adsp:ktf_adsp_ut",
    "//vendor/mediatek/tests/kernel/ktf_testcase/adsp:ktf_adsp_ipifuzz",
    "//vendor/mediatek/tests/kernel/ktf_testcase/scp:ktf_scp",
    "//vendor/mediatek/tests/kernel/ktf_testcase/vcp:ktf_vcp",
    "//vendor/mediatek/tests/kernel/ktf_testcase/smmu:ktf_smmu",
    "//vendor/mediatek/tests/kernel/ktf_testcase/smmu/smmu_ait:ktf_smmu_ait",
    "//vendor/mediatek/tests/kernel/ktf_testcase/sspm:ktf_sspm",
    "//vendor/mediatek/tests/kernel/ktf_testcase/sspm/sspm_ait:ktf_sspm_ait",
    "//vendor/mediatek/tests/kernel/ktf_testcase/swpm:mtk_ktf_swpm",
    "//vendor/mediatek/tests/kernel/ktf_testcase/fhctl/fhctl_ut:ktf_fhctl_ut",
    "//vendor/mediatek/tests/kernel/ktf_testcase/cpu_dvfs_ut:ktf_cpu_dvfs_ut",
    "//vendor/mediatek/tests/kernel/ktf_testcase/irq_monitor:ktf_irq_monitor",
    "//vendor/mediatek/tests/kernel/ktf_testcase/power_throttling:ktf_power_throttling",
    "//vendor/mediatek/tests/kernel/ktf_testcase/pbm_mdpm:ktf_pbm_mdpm",
    #"//vendor/mediatek/tests/kernel/ktf_testcase/vcodec/vcodec_ait:ktf_vcodec_ait3",
    #"//vendor/mediatek/tests/kernel/ktf_testcase/vcodec/vcodec_ait:ktf_vcodec_ait2",
    #"//vendor/mediatek/tests/kernel/ktf_testcase/vcodec/vcodec_ait:ktf_vcodec_ait",
    "//vendor/mediatek/tests/kernel/ktf_testcase/mmdvfs:ktf_mmdvfs",
    "//vendor/mediatek/tests/kernel/ktf_testcase/mminfra:ktf_mminfra",
    "//vendor/mediatek/tests/kernel/ktf_testcase/ipifuzz_autogen/adsp:ktf_adsp_ipifuzz_autogen",
    "//vendor/mediatek/tests/kernel/ktf_testcase/ipifuzz_autogen/gpueb:ktf_gpueb_ipifuzz_autogen",
    "//vendor/mediatek/tests/kernel/ktf_testcase/mminfra:ktf_mminfra_wbgai",
    "//vendor/mediatek/tests/kernel/ktf_testcase/display_mobile_wbgai:ktf_display_mobile_wbgai",
    "//vendor/mediatek/tests/kernel/ktf_testcase/display_pq_wbgai:ktf_display_pq_wbgai",
    "//vendor/mediatek/tests/kernel/ktf_testcase/ipifuzz_autogen/vcp:ktf_vcp_ipifuzz_autogen",
    "//vendor/mediatek/tests/kernel/ktf_testcase/vcp/ipifuzz_vcp:ktf_vcp_ipifuzz",
    "//vendor/mediatek/tests/kernel/ktf_testcase/scp/ipifuzz_scp:ktf_sap_ipifuzz",
    "//vendor/mediatek/tests/kernel/ktf_testcase/scp/ipifuzz_scp:ktf_scp_ipifuzz",
    "//vendor/mediatek/tests/kernel/ktf_testcase/ipifuzz_autogen/scp:ktf_scp_ipifuzz_autogen",
    "//vendor/mediatek/tests/kernel/ktf_testcase/thermal:ktf_thermal",
    "//vendor/mediatek/tests/kernel/ktf_testcase/gpu_ext:ktf_gpu_ext_it",
    "//vendor/mediatek/tests/kernel/ktf_testcase/core_ctl:ktf_mtk_core_ctl",
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
    "//kernel_device_modules-{}/drivers/char/hw_random:ddk_srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/char/rpmb:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/cpufreq:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/devfreq:ddk_makefiles".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/cameraisp/dpe:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/dma/mediatek:ddk_src".format(kernel_version),
    "//kernel_device_modules-{}/drivers/pwm:ddk_src".format(kernel_version),
    "//kernel_device_modules-{}/drivers/tty/serial/8250:ddk_src".format(kernel_version),
    "//kernel_device_modules-{}/drivers/i2c/busses:ddk_src".format(kernel_version),
    "//kernel_device_modules-{}/drivers/i3c/master:ddk_src".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/i3c_i2c_wrap:ddk_src".format(kernel_version),
    "//kernel_device_modules-{}/drivers/dma-buf/heaps:ddk_src".format(kernel_version),
    "//kernel_device_modules-{}/drivers/edac/mediatek:makefiles".format(kernel_version),
    "//kernel_device_modules-{}/drivers/iommu:ddk_src".format(kernel_version),
    "//kernel_device_modules-{}/drivers/iommu/arm/arm-smmu-v3:ddk_src".format(kernel_version),
    "//kernel_device_modules-{}/drivers/clk/mediatek:ddk_makefile".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/aee/mrdump:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/drm/panel:ddk_makefile".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mdp:ddk_makefile".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/drm/mediatek/mediatek_v2:ddk_makefile".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/drm/mediatek/mml:ddk_makefile".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/drm/mediatek/mediatek_v2/v1:ddk_makefile".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/mediatek/ged:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/mediatek/gpu_iommu:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/mediatek/gpueb:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/mediatek/gpufreq:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/mediatek/gpufreq/v2:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/mediatek/gpufreq/v2_legacy:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/mediatek/gpu_pdma:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/mediatek/gpu_bm:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/mediatek/mt-plat:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/iio/adc:ddk_src".format(kernel_version),
    "//kernel_device_modules-{}/drivers/interconnect/mediatek:makefiles".format(kernel_version),
    "//kernel_device_modules-{}/drivers/leds:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/leds/flash:ddk_src".format(kernel_version),
    "//kernel_device_modules-{}/drivers/nvmem:ddk_makefiles".format(kernel_version),
    "//kernel_device_modules-{}/drivers/mfd:ddk_src".format(kernel_version),
    "//kernel_device_modules-{}/drivers/memory/mediatek:makefiles".format(kernel_version),
    "//kernel_device_modules-{}/drivers/tee/teeperf:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/media/platform/mtk-vcodec:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/media/platform/mtk-vcu:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/adsp:srcs".format(kernel_version),
	"//kernel_device_modules-{}/drivers/misc/mediatek/apusys/power:srcs".format(kernel_version),
	"//kernel_device_modules-{}/drivers/misc/mediatek/apusys:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/apusys/aov:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/audio_ipi:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/btif:ddk_makefiles".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/blocktag:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/ccci_util:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/ccmni:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/cache-auditor:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/clkbuf:ddk_src".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/conn_md:ddk_makefiles".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/connectivity:ddk_makefiles".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/conn_scp:ddk_makefiles".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/dvfsrc:ddk_makefiles".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/eccci:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/eccci/fsm:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/eccci/hif:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/extcon:ddk_srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/flashlight:ddk_makefile".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/flashlight/v4l2:ddk_makefile".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/geniezone:ddk_makefile".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/hwccf:ddk_makefile".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/ise_lpm:ddk_makefile".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/log_store:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/masp:ddk_srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mdpm:ddk_makefile".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/monitor_hang:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/pbm:ddk_makefile".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/cg_ppt:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/pcie:ddk_makefile".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/perf_common:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/pmic_protect:ddk_src".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/power_throttling:ddk_makefile".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/rps:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/sensor/2.0/core:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/sensor/2.0/sensorhub:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/task_turbo:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/trusted_mem:ddk_makefile".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/aee/aed:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/iommu:ddk_src".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mmdebug:ddk_makefile".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mmdvfs:ddk_makefile".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mmp/src:ddk_makefile".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mtprintk:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/sda:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/pmsr:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/pmsr/v2:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/pmsr/v3:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/pmsr/v4:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/slbc:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/subpmic:ddk_src".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/swpm:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug/v1:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug/v1/subsys:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug/v6991/subsys:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug/v6993/subsys:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/swpm_legacy_v2:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/swpm_legacy_v2/modules/debug/v1:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/swpm_legacy_v2/modules/debug:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/typec/tcpc:ddk_src".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/typec/mux:ddk_src".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/uarthub:ddk_makefiles".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/usb/usb_rndis:ddk_src".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/usb/usb_logger:ddk_makefile".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/usb/usb_sram:ddk_makefile".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/usb/usb_offload:ddk_makefile".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/usb/usb_boost:ddk_makefile".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/usb/usb_xhci:ddk_makefile".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/usb/usb20:ddk_makefile".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/vmm_spm:ddk_src".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/gate_ic:ddk_makefile".format(kernel_version),
    "//kernel_device_modules-{}/drivers/mmc/host:ddk_srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/pci/controller:ddk_makefile".format(kernel_version),
    "//kernel_device_modules-{}/drivers/phy/mediatek:ddk_makefile".format(kernel_version),
    "//kernel_device_modules-{}/drivers/power/supply/ufcs:ddk_src".format(kernel_version),
    "//kernel_device_modules-{}/drivers/power/supply:ddk_src".format(kernel_version),
    "//kernel_device_modules-{}/drivers/regulator:ddk_src".format(kernel_version),
    "//kernel_device_modules-{}/drivers/rtc:ddk_makefile".format(kernel_version),
    "//kernel_device_modules-{}/drivers/soc/mediatek:makefiles".format(kernel_version),
    "//kernel_device_modules-{}/drivers/soc/mediatek/mmdvfs:makefiles".format(kernel_version),
    "//kernel_device_modules-{}/drivers/spmi:ddk_src".format(kernel_version),
    "//kernel_device_modules-{}/drivers/tee/gud:ddk_makefile".format(kernel_version),
    "//kernel_device_modules-{}/drivers/thermal/mediatek:ddk_makefile".format(kernel_version),
    "//kernel_device_modules-{}/drivers/usb/mtu3:ddk_srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/cci_lite:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/cpufreq_lite:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/performance:ddk_src".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mtprof:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/tee/teei/520:srcs".format(kernel_version),
    "//kernel_device_modules-{}/sound/soc/mediatek/audio_dsp:srcs".format(kernel_version),
    "//kernel_device_modules-{}/sound/soc/mediatek/common:srcs".format(kernel_version),
    "//kernel_device_modules-{}/sound/soc/codecs:srcs".format(kernel_version),
    "//kernel_device_modules-{}/sound/soc/codecs/richtek:srcs".format(kernel_version),
    "//kernel_device_modules-{}/sound/soc/codecs/tfa98xx:srcs".format(kernel_version),
    "//kernel_device_modules-{}/sound/soc/mediatek/mt6855:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mminfra:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/vmm:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/camera_mem:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/cam_timesync:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/ccu/src/isp4:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/cameraisp/src/isp_6s:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/cameraisp/pda:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/cameraisp/mfb/isp_6s:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/tinysys_scmi:ddk_srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/mailbox:ddk_srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/ise_trusty:ddk_srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/lpm:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/lpm/modules/debug:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mkp:ddk_makefile".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mmstat:ddk_makefile".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/scp/include:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/scp/rv:ddk_makefile".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/sspm/v1:ddk_makefile".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/sspm/v3:ddk_makefile".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/smap:ddk_makefiles".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/vcp/include:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/vcp/rv:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/vcp/rv_v2:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/vow/ver02:ddk_makefile".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mcupm:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mtk-interconnect:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/qos:ddk_src".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/widevine_drm:ddk_srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/remoteproc:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/spi:ddk_src".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mme:ddk_src".format(kernel_version),
    "//kernel_device_modules-{}/drivers/memory:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/soc/mediatek/devapc:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/memory-amms:ddk_makefile".format(kernel_version),
    "//kernel_device_modules-{}/sound/soc/mediatek/ultrasound/ultra_common:srcs".format(kernel_version),
    "//kernel_device_modules-{}/sound/soc/mediatek/ultrasound/ultra_scp:srcs".format(kernel_version),
    "//kernel_device_modules-{}/sound/soc/mediatek/vow:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mtk_zram:ddk_makefile".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:ddk_makefile".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:ddk_src".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr_legacy_v1:ddk_makefile".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr_legacy_v1:ddk_src".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/smi:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/cameraisp/rsc:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/ufs:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/hw_sem:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/cmdq/mailbox:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/cameraisp/fdvt/5.1:srcs".format(kernel_version),
    "//vendor/mediatek/kernel_modules/mtkcam/ccusys:srcs",
    "//kernel_device_modules-{}/drivers/misc/mediatek/mbraink/perf:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/performance/powerhal_cpu_ctrl:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mbraink/bridge:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mdp:ddk_src".format(kernel_version),
    "//kernel_device_modules-{}/drivers/input/touchscreen/GT9895:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/imgsensor:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/imgsensor/src:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/cam_cal:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/cam_cal/src:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mddp:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/apusys/sapu:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/sched:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/cameraisp/dip/isp_6s:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/lpm_legacy:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/lpm_legacy/modules/debug:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/cameraisp/wpe/isp_6s:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/performance/sbe:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/ssc:srcs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/performance/fpsgo_v8:ddk_srcs".format(kernel_version),
]

mgk_64_kleaf_device_modules_kconfigs = [
    # keep sorted
    "//kernel_device_modules-{}/drivers/char/hw_random:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/char/rpmb:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/clocksource:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/cpufreq:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/devfreq:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/dma/mediatek:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/tty/serial/8250:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/i2c/busses:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/i3c/master:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/i3c_i2c_wrap:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/dma-buf/heaps:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/pwm:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/edac/mediatek:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/iommu:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/iommu/arm/arm-smmu-v3:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/clk/mediatek:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/drm/panel:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/drm/mediatek/mediatek_v2:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/drm/mediatek/mml:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/drm/mediatek/mediatek_v2/v1:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/mediatek/ged:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/mediatek/hal:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/iio/adc:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/input/keyboard:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/interconnect/mediatek:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/leds:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/leds/flash:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/nvmem:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/memory/mediatek:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/tee/teeperf:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/media/platform/mtk-vcodec:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/mfd:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/adsp:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/aee/hangdet:ddk_kconfigs".format(kernel_version),
	"//kernel_device_modules-{}/drivers/misc/mediatek/apusys/power:ddk_kconfigs".format(kernel_version),
	"//kernel_device_modules-{}/drivers/misc/mediatek/apusys:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/audio_ipi:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/blocktag:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/btif:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/ccci_util:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/cci_lite:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/ccmni:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/ccu/src/isp6s:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/cache-auditor:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/clkbuf:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/conn_md:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/connectivity:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/conn_scp:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/cpufreq_v1:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/cpufreq_lite:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/dcm:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/dvfsrc:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/eccci:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/extcon:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/flashlight:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/flashlight/v4l2:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/geniezone:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/geniezone/gz-trusty:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/hwccf:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/iommu:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/irq_monitor:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/irtx:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/jpeg:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/vdec_fmt:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/locking:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/log_store:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/masp:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mmdebug:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mmp/src:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mm_monitor:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/monitor_hang:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mtprintk:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/ips:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/ise_lpm:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/ktchbst:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mdpm:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/pbm:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/cg_ppt:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/pcie:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/pidmap:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/perf_common:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/pmic_protect:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/pmsr:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/pmsr/v2:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/pmsr/v3:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/pmsr/v4:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/power_throttling:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/pwm:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/rps:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/sda:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/selinux_warning:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/slbc:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/smap:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/subpmic:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/swpm:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/swpm_legacy_v2:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/sensor/2.0/sensorhub:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/task_turbo:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/trusted_mem:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/typec/tcpc:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/typec/mux:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/uarthub:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/usb/c2k_usb:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/usb/usb_meta:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/usb/usb_rndis:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/usb/usb_logger:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/usb/usb_sram:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/usb/usb_offload:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/usb/usb_boost:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/usb/usb_xhci:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/usb/usb20:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/vmm_spm:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/vow/ver02:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/wla:wla_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/gate_ic:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/mmc/host:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/pci/controller:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/phy/mediatek:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/pinctrl/mediatek:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/power/supply/ufcs:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/power/supply:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/regulator:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/rtc:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/remoteproc:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/reset:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/rpmsg:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/soc/mediatek:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/soc/mediatek/mmdvfs:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/soc/mediatek/devmpu:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/spmi:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/thermal/mediatek:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/trusty:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/usb/mtu3:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/performance:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mcupm:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mdp:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/camera_mem:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/cam_timesync:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/ccu/src/isp4:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/cameraisp/src/isp_6s:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/cameraisp/pda:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/cameraisp/mfb/isp_6s:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/watchdog:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mtprof:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/tee/teei/520:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/sound/soc/mediatek/common:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/sound/soc/codecs:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/sound/soc/codecs/richtek:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/sound/soc/codecs/tfa98xx:ddk_kconfigs".format(kernel_version),
	"//kernel_device_modules-{}/sound/soc/mediatek/mt6855:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mminfra:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/vmm:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/tinysys_scmi:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/mailbox:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/lpm_legacy:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/ise_trusty:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/lpm:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/lpm/governors:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mkp:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mmstat:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/tee/gud:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/tee/gud/700/TlcTui:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/sspm/v1:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mtk-interconnect:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/qos:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/spi:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mme:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/memory:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/widevine_drm:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/soc/mediatek/devapc:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/memory-amms:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/sound/soc/mediatek/ultrasound/ultra_common:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/sound/soc/mediatek/ultrasound/ultra_scp:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/sound/soc/mediatek/vow:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mtk_zram:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/cameraisp/rsc:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/cameraisp/dpe:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr_legacy_v1:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/smi:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/ufs:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/hw_sem:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/cmdq/mailbox:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/cameraisp/fdvt/5.1:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mbraink/perf:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mbraink/bridge:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/drm/mediatek/dpc/dpc_v3:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/drm/mediatek/dpc/dpc_v2:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/drm/mediatek/dpc/dpc_v1:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/drm/mediatek/dpc:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/imgsensor:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/imgsensor/src:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/cam_cal:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mddp:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/apusys/sapu:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/sched:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/cameraisp/dip/isp_6s:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/cameraisp/wpe/isp_6s:ddk_kconfigs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/ssc:ddk_kconfigs".format(kernel_version),
]

mgk_64_kleaf_device_modules = [
    # keep sorted
    "//kernel_device_modules-{}/drivers/char/hw_random:sec-rng".format(kernel_version),
    "//kernel_device_modules-{}/drivers/char/rpmb:rpmb".format(kernel_version),
    "//kernel_device_modules-{}/drivers/char/rpmb:rpmb-mtk".format(kernel_version),
    "//kernel_device_modules-{}/drivers/clocksource:timer-mediatek".format(kernel_version),
    "//kernel_device_modules-{}/drivers/cpufreq:mediatek-cpufreq-hw".format(kernel_version),
    "//kernel_device_modules-{}/drivers/devfreq:mtk-dvfsrc-devfreq".format(kernel_version),
    "//kernel_device_modules-{}/drivers/dma/mediatek:mtk-cqdma".format(kernel_version),
    "//kernel_device_modules-{}/drivers/dma/mediatek:mtk-uart-apdma".format(kernel_version),
    "//kernel_device_modules-{}/drivers/tty/serial/8250:8250_mtk".format(kernel_version),
    "//kernel_device_modules-{}/drivers/i2c/busses:i2c-mt65xx".format(kernel_version),
    "//kernel_device_modules-{}/drivers/i3c/master:mtk-i3c-master-mt69xx".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/i3c_i2c_wrap:mtk-i3c-i2c-wrap".format(kernel_version),
    "//kernel_device_modules-{}/drivers/dma-buf/heaps:mtk_heap_debug".format(kernel_version),
    "//kernel_device_modules-{}/drivers/dma-buf/heaps:mtk_heap_refill".format(kernel_version),
    "//kernel_device_modules-{}/drivers/dma-buf/heaps:mtk_sec_heap".format(kernel_version),
    "//kernel_device_modules-{}/drivers/dma-buf/heaps:system_heap".format(kernel_version),
    "//kernel_device_modules-{}/drivers/pwm:pwm-mtk-disp".format(kernel_version),
    "//kernel_device_modules-{}/drivers/edac/mediatek:mtk_edac_slc".format(kernel_version),
    "//kernel_device_modules-{}/drivers/iommu:mtk_iommu".format(kernel_version),
    "//kernel_device_modules-{}/drivers/iommu/arm/arm-smmu-v3:arm_smmu_v3".format(kernel_version),
    "//kernel_device_modules-{}/drivers/iommu/arm/arm-smmu-v3:mtk-smmuv3-mpam-mon".format(kernel_version),
    "//kernel_device_modules-{}/drivers/iommu/arm/arm-smmu-v3:mtk-smmuv3-pmu".format(kernel_version),
    "//kernel_device_modules-{}/drivers/clk/mediatek:clk-common".format(kernel_version),
    "//kernel_device_modules-{}/drivers/clk/mediatek:clk-disable-unused".format(kernel_version),
    "//kernel_device_modules-{}/drivers/clk/mediatek:fhctl".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/vmm_spm:mtk_vmm_spm".format(kernel_version),
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
    "//kernel_device_modules-{}/drivers/gpu/drm/panel:panel-nt36672a-rt4801-vdo".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/drm/panel:panel-alpha-jdi-nt36672e-cphy-vdo".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/drm/panel:panel-alpha-jdi-nt36672e-vdo-120hz-frameratev5".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/drm/panel:panel-alpha-jdi-nt36672e-vdo-144hz".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/drm/panel:panel-nt35695b-auo-cmd".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/drm/panel:panel-nt37707-c2v-arp".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/drm/panel:panel-rm692h5-alpha-cmd-spr".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/drm/panel:panel-rm692h5-alpha-cmd-spr-mipichg".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/drm/panel:panel-rm692h5-cmd".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/drm/panel:panel-tianma-nt36672e-vdo-120hz-vfp".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/drm/panel:panel-tianma-nt36672e-vdo-120hz-vfp-6382".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/drm/panel:panel-tianma-r66451-cmd-120hz".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/drm/panel:panel-tianma-r66451-cmd-120hz-wa".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/drm/panel:panel-truly-nt35595-cmd".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/drm/panel:panel-truly-td4330-vdo".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/drm/panel:panel-samsung-op-cmd".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/drm/panel:panel-samsung-op-cmd-msync2".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/drm/panel:panel-alpha-samsung-s6e3hae-wqhd-dphy-cmd-120hz".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/drm/panel:panel-alpha-jdi-nt36672e-vdo-120hz".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/drm/panel:panel-alpha-jdi-nt36672e-vdo-120hz-hfp".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/drm/panel:panel-alpha-jdi-nt36672e-vdo-120hz-threshold".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/drm/panel:panel-alpha-jdi-nt36672e-vdo-144hz-hfp".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/drm/panel:panel-alpha-jdi-nt36672e-vdo-60hz".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/drm/panel:panel-nt37801-cmd-120hz".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/drm/panel:panel-nt37801-cmd-fhd".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/drm/panel:panel-nt37801-cmd-fhd-plus".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/drm/panel:panel-nt37801-cmd-fhd-10bit".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/drm/panel:panel-nt37801-cmd-ltpo".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/drm/panel:panel-nt37801-cmd-spr".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/drm/panel:panel-truly-td4330-cmd".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/drm/panel:panel-samsung-s68fc01-vdo-aod".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/drm/mediatek/mediatek_v2:mtk_disp_notify".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/drm/mediatek/mediatek_v2:mtk_panel_ext".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/drm/mediatek/mediatek_v2:mtk_sync".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/drm/mediatek/mediatek_v2:mediatek-drm".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/drm/mediatek/mediatek_v2:mtk_disp_sec".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/drm/mediatek/mediatek_v2:mtk_aod_scp".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/drm/mediatek/mml:mtk-mml".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/drm/mediatek/dpc/dpc_v3:mtk_dpc_v3".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/drm/mediatek/dpc/dpc_v3:mtk_vdisp_v3".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/drm/mediatek/dpc/dpc_v2:mtk_dpc_v2".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/drm/mediatek/dpc/dpc_v2:mtk_vdisp_v2".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/drm/mediatek/dpc/dpc_v1:mtk_dpc_v1".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/drm/mediatek/dpc/dpc_v1:mtk_vdisp_v1".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/mediatek/hal:mtk_gpu_hal".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/mediatek/ged:ged".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/mediatek/gpueb:mtk_gpueb".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/mediatek/gpufreq/v2:mtk_gpufreq_wrapper".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/mediatek/gpu_bm:mtk_gpu_qos".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/mediatek/gpu_bm:mtk_gpu_qos_bringup".format(kernel_version),
    "//kernel_device_modules-{}/drivers/iio/adc:mt635x-auxadc".format(kernel_version),
    "//kernel_device_modules-{}/drivers/iio/adc:mt6338-auxadc".format(kernel_version),
    "//kernel_device_modules-{}/drivers/iio/adc:mt6577_auxadc".format(kernel_version),
    "//kernel_device_modules-{}/drivers/iio/adc:mt6375-adc".format(kernel_version),
    "//kernel_device_modules-{}/drivers/iio/adc:mt6375-auxadc".format(kernel_version),
    "//kernel_device_modules-{}/drivers/iio/adc:mt6379-adc".format(kernel_version),
    "//kernel_device_modules-{}/drivers/iio/adc:mtk-spmi-pmic-adc".format(kernel_version),
    "//kernel_device_modules-{}/drivers/iio/adc:rt9490-adc".format(kernel_version),
    "//kernel_device_modules-{}/drivers/iio/adc:mt6681-auxadc".format(kernel_version),
    "//kernel_device_modules-{}/drivers/input/keyboard:mtk-kpd".format(kernel_version),
    "//kernel_device_modules-{}/drivers/input/touchscreen/BoTai_Multi_Touch/BoTai_touch_one:botai_touch_one".format(kernel_version),
    "//kernel_device_modules-{}/drivers/input/touchscreen/BoTai_Multi_Touch/BoTai_touch_two:botai_touch_two".format(kernel_version),
    "//kernel_device_modules-{}/drivers/input/touchscreen/GT1151:gt1151".format(kernel_version),
    "//kernel_device_modules-{}/drivers/input/touchscreen/GT9886:gt9886".format(kernel_version),
    "//kernel_device_modules-{}/drivers/input/touchscreen/GT9895:gt9895".format(kernel_version),
    "//kernel_device_modules-{}/drivers/input/touchscreen/GT9896S:gt9896s".format(kernel_version),
    "//kernel_device_modules-{}/drivers/input/touchscreen/GT9966:gt9966".format(kernel_version),
    "//kernel_device_modules-{}/drivers/input/touchscreen/gt9xx:gt9xx_touch".format(kernel_version),
    "//kernel_device_modules-{}/drivers/input/touchscreen/NT36532:nt36532".format(kernel_version),
    "//kernel_device_modules-{}/drivers/input/touchscreen/tui_common:tui-common".format(kernel_version),
    "//kernel_device_modules-{}/drivers/input/touchscreen/ts_scp:ts_scp_common".format(kernel_version),
    "//kernel_device_modules-{}/drivers/input/keyboard:mtk-pmic-keys".format(kernel_version),
    "//kernel_device_modules-{}/drivers/input/touchscreen/NT36672C_I2C:nt36672c_i2c".format(kernel_version),
    "//kernel_device_modules-{}/drivers/interconnect/mediatek:mtk-emi".format(kernel_version),
    "//kernel_device_modules-{}/drivers/interconnect/mediatek:mtk-emibus-icc".format(kernel_version),
    "//kernel_device_modules-{}/drivers/leds/flash:leds-mt6379".format(kernel_version),
    "//kernel_device_modules-{}/drivers/leds:leds-mtk".format(kernel_version),
    "//kernel_device_modules-{}/drivers/leds:leds-mtk-pwm".format(kernel_version),
    "//kernel_device_modules-{}/drivers/leds:leds-mtk-disp".format(kernel_version),
    "//kernel_device_modules-{}/drivers/leds:regulator-vibrator".format(kernel_version),
    "//kernel_device_modules-{}/drivers/nvmem:nvmem-mt635x-efuse".format(kernel_version),
    "//kernel_device_modules-{}/drivers/nvmem:nvmem-mt6338-efuse".format(kernel_version),
    "//kernel_device_modules-{}/drivers/nvmem:nvmem-mt6688-rtccon".format(kernel_version),
    "//kernel_device_modules-{}/drivers/nvmem:nvmem_mtk-devinfo".format(kernel_version),
    "//kernel_device_modules-{}/drivers/nvmem:nvmem-mt6681-efuse".format(kernel_version),
    "//kernel_device_modules-{}/drivers/media/platform/mtk-jpeg:mtk-jpeg".format(kernel_version),
    "//kernel_device_modules-{}/drivers/media/platform/mtk-vcodec:mtk-vcodec-common".format(kernel_version),
    "//kernel_device_modules-{}/drivers/media/platform/mtk-vcodec:mtk-vcodec-dec-v1".format(kernel_version),
    "//kernel_device_modules-{}/drivers/media/platform/mtk-vcodec:mtk-vcodec-dec-v2".format(kernel_version),
    "//kernel_device_modules-{}/drivers/media/platform/mtk-vcu:mtk-vcu".format(kernel_version),
    "//kernel_device_modules-{}/drivers/memory/mediatek:emi".format(kernel_version),
    "//kernel_device_modules-{}/drivers/memory/mediatek:emi-fake-eng".format(kernel_version),
    "//kernel_device_modules-{}/drivers/memory/mediatek:emi-fake-eng-v2".format(kernel_version),
    "//kernel_device_modules-{}/drivers/memory/mediatek:emi-mpu".format(kernel_version),
    "//kernel_device_modules-{}/drivers/memory/mediatek:emi-mpu-test".format(kernel_version),
    "//kernel_device_modules-{}/drivers/memory/mediatek:smpu".format(kernel_version),
    "//kernel_device_modules-{}/drivers/memory/mediatek:smpu-hook-v1".format(kernel_version),
    "//kernel_device_modules-{}/drivers/memory/mediatek:emi-mpu-test-v2".format(kernel_version),
    "//kernel_device_modules-{}/drivers/memory/mediatek:emi-slb".format(kernel_version),
    "//kernel_device_modules-{}/drivers/memory/mediatek:slc-parity".format(kernel_version),
    "//kernel_device_modules-{}/drivers/mfd:mt6375".format(kernel_version),
    "//kernel_device_modules-{}/drivers/mfd:mt6379i".format(kernel_version),
    "//kernel_device_modules-{}/drivers/mfd:mt6379s".format(kernel_version),
    "//kernel_device_modules-{}/drivers/mfd:mt6397".format(kernel_version),
    "//kernel_device_modules-{}/drivers/mfd:mt63xx-debug".format(kernel_version),
    "//kernel_device_modules-{}/drivers/mfd:mtk-spmi-pmic".format(kernel_version),
    "//kernel_device_modules-{}/drivers/mfd:mtk-spmi-pmic-debug".format(kernel_version),
    "//kernel_device_modules-{}/drivers/mfd:mt6685-core".format(kernel_version),
    "//kernel_device_modules-{}/drivers/mfd:mt6687-core".format(kernel_version),
    "//kernel_device_modules-{}/drivers/mfd:mt6681-core".format(kernel_version),
    "//kernel_device_modules-{}/drivers/mfd:mt6338-core".format(kernel_version),
    "//kernel_device_modules-{}/drivers/mfd:mt6685-audclk".format(kernel_version),
    "//kernel_device_modules-{}/drivers/mfd:rt9490".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/adsp:adsp".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/adsp/v1:adsp-v1".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/adsp/v2:adsp-v2".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/adsp/v3:adsp-v3".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/aee/hangdet:aee_hangdet".format(kernel_version),
	"//kernel_device_modules-{}/drivers/misc/mediatek/apusys/power:apu_top".format(kernel_version),
	"//kernel_device_modules-{}/drivers/misc/mediatek/apusys:apusys".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/apusys/aov:apu_aov".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/audio_ipi:audio_ipi".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/atf:atf_logger".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/blocktag:blocktag".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/btif:btif_drv".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/ccci_util:ccci_util_lib".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/cci_lite:ccidvfs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/ccmni:ccmni".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/ccu/src/isp6s:ccu_isp6s".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/cache-auditor:cpuqos_v3".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/clkbuf:clkbuf".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/connectivity:connadp".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/conn_scp:connscp".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/cpufreq_lite:cpudvfs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/dcm:mtk_dcm".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/dvfsrc:mtk-dvfsrc-helper".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/eccci:ccci_auxadc".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/eccci:ccci_md_all".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/eccci/fsm:ccci_fsm_scp".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/eccci/hif:ccci_ccif".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/eccci/hif:ccci_cldma".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/eccci/hif:ccci_dpmaif".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/extcon:extcon-mtk-usb".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/flashlight:mtk-composite".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/flashlight:flashlight".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/flashlight/v4l2:lm3643".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/flashlight/v4l2:lm3644".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/geniezone:gz_main".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/geniezone:gz_tz_system".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/geniezone/gz-trusty:gz-trusty".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/hwccf:hwccf".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/ise_lpm:ise_lpm".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/ise_lpm:ise_lpm_v2".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/ips:mtk-ips-helper".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/ktchbst:ktchbst".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/log_store:log_store".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/masp:sec".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mdpm:mtk_mdpm".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mmp/src:mmprofile".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/monitor_hang:monitor_hang".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mtprintk:mtk_printk_ctrl".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/pbm:mtk_pbm".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/pbm:mtk_peak_power_budget".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/cg_ppt:mtk_cg_peak_power_throttling".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/pcie:mtk_pcie_smt".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/pidmap:pidmap".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/perf_common:mtk_perf_common".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/power_throttling:mtk_apu_power_throttling".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/power_throttling:mtk_md_power_throttling".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/power_throttling:mtk_battery_oc_throttling".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/power_throttling:mtk_bp_thl".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/power_throttling:mtk_cpu_power_throttling".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/power_throttling:mtk_dynamic_loading_throttling".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/power_throttling:mtk_gpu_power_throttling".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/power_throttling:mtk_low_battery_throttling".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/power_throttling:pmic_dual_lbat_service".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/power_throttling:pmic_lbat_service".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/power_throttling:pmic_lvsys_notify".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/rps:rps_perf".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/sda/btm:bus_tracer_interface".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/sda/btm/v1:bus_tracer_v1".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/sda:cache-parity".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/sda:dbg_error_flag".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/sda:dbgtop-drm".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/sda:bus-parity".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/sda:irq-dbg".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/sda:systracker".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/sda:gic-ram-parity".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/sda:last_bus".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/slbc:mmsram".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/slbc:mtk_slbc".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/slbc:slbc_ipi".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/slbc:slbc_trace".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/smap:mtk-smap-debug".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/smap:smap-mt6991".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/smap:smap-mt6993".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/subpmic:extdev_io_class".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/subpmic:subpmic-dbg".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/sensor/2.0/core:hf_manager".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/sensor/2.0/sensorhub:sensorhub".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/task_turbo:task_turbo".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/trusted_mem:ffa".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/trusted_mem:trusted_mem".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/aee/aed:aee_aed".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/aee/aed:aee_rs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/iommu:iommu_debug".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/iommu:iommu_engine".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/iommu:iommu_secure".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/iommu:iommu_test".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/iommu:iommu_gz".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/iommu:mtk_iommu_util".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/iommu:mtk_smmu_qos".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/iommu:smmu_secure".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/irtx:mtk_irtx_pwm".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/jpeg:jpeg-driver".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/lens/vcm/v4l2/ak7375c:ak7375c".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/lens/vcm/v4l2/ak7377a:ak7377a".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/lens/vcm/v4l2/bu64253gwz:bu64253gwz".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/lens/vcm/v4l2/dw9718:dw9718".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/lens/vcm/v4l2/dw9800v:dw9800v".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/lens/vcm/v4l2/dw9800w:dw9800w".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/lens/vcm/v4l2/gt9764:gt9764".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/lens/vcm/v4l2/gt9772a:gt9772a".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/lens/vcm/v4l2/gt9772b:gt9772b".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/lens/vcm/v4l2/lc898229:lc898229".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/lens/vcm/v4l2/main_vcm:main_vcm".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/lens/vcm/v4l2/main2_vcm:main2_vcm".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/lens/vcm/v4l2/main3_vcm:main3_vcm".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/lens/vcm/v4l2/main4_vcm:main4_vcm".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/lens/vcm/v4l2/sub_vcm:sub_vcm".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/lens/vcm/v4l2/media:camera_af_media".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/lens/vcm/proprietary/main:mainaf".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/lens/vcm/proprietary/main2:main2af".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/lens/vcm/proprietary/main3:main3af".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/lens/vcm/proprietary/sub:subaf".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/lens/vcm/proprietary/sub2:sub2af".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/lens/tof:stmvl53l4".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/lens/tof:vl53l0".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/lens/ois/bu63169:bu63169".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/lens/ois/dw9781c:dw9781c".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/lens/ois/dw9781d:dw9781d".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/cameraisp/pda:camera_pda".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mmdebug:mtk-mmdebug-vcp-stub".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mmdebug:mtk-mmdebug-vcp".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mmdvfs:mtk-mmdvfs-ftrace".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mmdvfs:mtk-mmdvfs-debug".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mmdvfs:mtk-mmdvfs-debug-v3".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mmdvfs:mtk-mmdvfs-debug-v5".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mdp:mdp_drv_dummy".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mdp:cmdq_helper_inf".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/pmsr:pmsr".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/pmsr/v2:pmsr_v2".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/pmsr/v3:pmsr_v3".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/pmsr/v4:pmsr_v4".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/pwm:mtk-pwm".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/qos:mtk_qos".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/scp/rv:scp".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/sspm/v3:sspm_v3".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/typec/tcpc:pd_dbg_info".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/typec/tcpc:rt_pd_manager".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/typec/tcpc:tcpci_late_sync".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/typec/tcpc:tcpc_class".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/typec/tcpc:tcpc_mt6375".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/typec/tcpc:tcpc_mt6379".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/typec/tcpc:tcpc_rt1711h".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/typec/tcpc:tcpc_rt1718s".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/typec/mux:fusb304".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/typec/mux:mux_switch".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/typec/mux:ps5169".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/typec/mux:ps5170".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/typec/mux:ptn36241g".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/typec/mux:usb_dp_selector".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/uarthub:uarthub_drv".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/usb/c2k_usb:c2k_usb".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/usb/c2k_usb:c2k_usb_f_via_atc".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/usb/c2k_usb:c2k_usb_f_via_ets".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/usb/c2k_usb:c2k_usb_f_via_modem".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/usb/usb_meta:usb_meta".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/usb/usb_rndis:mtk_u_ether".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/usb/usb_rndis:mtk_usb_f_rndis".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/usb/usb_logger:usb_logger".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/usb/usb_sram:usb_sram".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/usb/usb_offload:usb_offload".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/usb/usb_boost:usb_boost".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/usb/usb_boost:musb_boost".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/usb/usb_xhci:xhci-mtk-hcd-v2".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/usb/usb20:musb_hdrc".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/usb/usb20/musb_main:musb_main".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/swpm:mtk-swpm-perf-arm-pmu".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/swpm:mtk-swpm".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug/v1:mtk-swpm-dbg-common-v1".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug/v1/subsys:mtk-swpm-isp-wrapper".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug/v6991/subsys:mtk-swpm-audio-dbg-v6991".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug/v6991/subsys:mtk-swpm-disp-dbg-v6991".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug/v6991/subsys:mtk-swpm-mml-dbg-v6991".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug/v6993/subsys:mtk-swpm-disp-dbg-v6993".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug/v6993/subsys:mtk-swpm-mml-dbg-v6993".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug:mtk-swpm-dbg-v6993".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug:mtk-swpm-dbg-v6991".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug/v6991/subsys:mtk-swpm-isp-dbg-v6991".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug/v6993/subsys:mtk-swpm-isp-dbg-v6993".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/vcp/rv:vcp_status".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/vcp/rv_v2:vcp_status_v2".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/vcp/rv:vcp".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/vcp/rv_v2:vcp_v2".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/vdec_fmt:vdec-fmt".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/vow/ver02:mtk-vow".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/wla:wla-v1".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/wla:wla-v1-dbg".format(kernel_version),
    "//kernel_device_modules-{}/drivers/mmc/host:mtk-mmc-dbg".format(kernel_version),
    "//kernel_device_modules-{}/drivers/mmc/host:cqhci".format(kernel_version),
    "//kernel_device_modules-{}/drivers/mmc/host:mtk-sd".format(kernel_version),
    "//kernel_device_modules-{}/drivers/mmc/host:mtk-mmc".format(kernel_version),
    "//kernel_device_modules-{}/drivers/pci/controller:pcie-mediatek-gen3".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/wla:wla-v2".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/wla:wla-v2-dbg".format(kernel_version),
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
    "//kernel_device_modules-{}/drivers/power/supply:mt6375-battery".format(kernel_version),
    "//kernel_device_modules-{}/drivers/power/supply:mt6375-charger".format(kernel_version),
    "//kernel_device_modules-{}/drivers/power/supply:mt6379-battery".format(kernel_version),
    "//kernel_device_modules-{}/drivers/power/supply:mt6379-chg".format(kernel_version),
    "//kernel_device_modules-{}/drivers/power/supply:mtk_2p_charger".format(kernel_version),
    "//kernel_device_modules-{}/drivers/power/supply:mtk_battery_manager".format(kernel_version),
    "//kernel_device_modules-{}/drivers/power/supply:mtk_charger_algorithm_class".format(kernel_version),
    "//kernel_device_modules-{}/drivers/power/supply:mtk_charger_framework".format(kernel_version),
    "//kernel_device_modules-{}/drivers/power/supply:mtk_chg_type_det".format(kernel_version),
    "//kernel_device_modules-{}/drivers/power/supply:mtk_hvbpc".format(kernel_version),
    "//kernel_device_modules-{}/drivers/power/supply:mtk_pd_adapter".format(kernel_version),
    "//kernel_device_modules-{}/drivers/power/supply:mtk_pd_charging".format(kernel_version),
    "//kernel_device_modules-{}/drivers/power/supply:mtk_pep".format(kernel_version),
    "//kernel_device_modules-{}/drivers/power/supply:mtk_pep20".format(kernel_version),
    "//kernel_device_modules-{}/drivers/power/supply:mtk_pep40".format(kernel_version),
    "//kernel_device_modules-{}/drivers/power/supply:mtk_pep45".format(kernel_version),
    "//kernel_device_modules-{}/drivers/power/supply:mtk_pep50".format(kernel_version),
    "//kernel_device_modules-{}/drivers/power/supply:mtk_pep50p".format(kernel_version),
    "//kernel_device_modules-{}/drivers/power/supply:mtk_ufcs_adapter".format(kernel_version),
    "//kernel_device_modules-{}/drivers/power/supply:rt9490-charger".format(kernel_version),
    "//kernel_device_modules-{}/drivers/power/supply:rt9758-charger".format(kernel_version),
    "//kernel_device_modules-{}/drivers/power/supply:rt9759".format(kernel_version),
    "//kernel_device_modules-{}/drivers/regulator:mt6315-regulator".format(kernel_version),
    "//kernel_device_modules-{}/drivers/regulator:mt6316-regulator".format(kernel_version),
    "//kernel_device_modules-{}/drivers/regulator:mt6359p-regulator".format(kernel_version),
    "//kernel_device_modules-{}/drivers/regulator:mt6358-regulator".format(kernel_version),
    "//kernel_device_modules-{}/drivers/regulator:mt6363-regulator".format(kernel_version),
    "//kernel_device_modules-{}/drivers/regulator:mt6368-regulator".format(kernel_version),
    "//kernel_device_modules-{}/drivers/regulator:mt6369-regulator".format(kernel_version),
    "//kernel_device_modules-{}/drivers/regulator:mt6373-regulator".format(kernel_version),
    "//kernel_device_modules-{}/drivers/regulator:mt6379-regulator".format(kernel_version),
    "//kernel_device_modules-{}/drivers/regulator:mt6661-regulator".format(kernel_version),
    "//kernel_device_modules-{}/drivers/regulator:mt6667-regulator".format(kernel_version),
    "//kernel_device_modules-{}/drivers/regulator:mt6681-regulator".format(kernel_version),
    "//kernel_device_modules-{}/drivers/regulator:mt6688".format(kernel_version),
    "//kernel_device_modules-{}/drivers/regulator:mtk-dvfsrc-regulator".format(kernel_version),
    "//kernel_device_modules-{}/drivers/regulator:mtk-extbuck-debug".format(kernel_version),
    "//kernel_device_modules-{}/drivers/regulator:rt4803".format(kernel_version),
    "//kernel_device_modules-{}/drivers/regulator:rt5133-regulator".format(kernel_version),
    "//kernel_device_modules-{}/drivers/regulator:rt6160-regulator".format(kernel_version),
    "//kernel_device_modules-{}/drivers/regulator:mtk-vmm-isp71-regulator".format(kernel_version),
    "//kernel_device_modules-{}/drivers/rtc:rtc-mt6397".format(kernel_version),
    "//kernel_device_modules-{}/drivers/rtc:rtc-mt6685".format(kernel_version),
    "//kernel_device_modules-{}/drivers/remoteproc:mtk_ccu".format(kernel_version),
    "//kernel_device_modules-{}/drivers/remoteproc:mtk_ccu_mssv".format(kernel_version),
    "//kernel_device_modules-{}/drivers/reset:reset-ti-syscon".format(kernel_version),
    "//kernel_device_modules-{}/drivers/rpmsg:mtk_rpmsg_mbox".format(kernel_version),
    "//kernel_device_modules-{}/drivers/soc/mediatek:mtk-dvfsrc".format(kernel_version),
    "//kernel_device_modules-{}/drivers/soc/mediatek:mtk-dvfsrc-start".format(kernel_version),
    "//kernel_device_modules-{}/drivers/soc/mediatek:mtk-pm-domain-disable-unused".format(kernel_version),
    "//kernel_device_modules-{}/drivers/soc/mediatek:mtk-scpsys".format(kernel_version),
    "//kernel_device_modules-{}/drivers/soc/mediatek:mtk-mbox".format(kernel_version),
    "//kernel_device_modules-{}/drivers/soc/mediatek:mtk_tinysys_ipi".format(kernel_version),
    "//kernel_device_modules-{}/drivers/soc/mediatek:mtk-pmic-wrap".format(kernel_version),
    "//kernel_device_modules-{}/drivers/soc/mediatek:mtk-mmdvfs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/soc/mediatek:mtk-mmdvfs-v3".format(kernel_version),
    "//kernel_device_modules-{}/drivers/soc/mediatek/mmdvfs:mtk-mmdvfs-v5".format(kernel_version),
    "//kernel_device_modules-{}/drivers/spmi:spmi-mtk-mpu".format(kernel_version),
    "//kernel_device_modules-{}/drivers/spmi:spmi-mtk-pmif".format(kernel_version),
    "//kernel_device_modules-{}/drivers/spmi:spmi-mtk-gip".format(kernel_version),
    "//kernel_device_modules-{}/drivers/tee/teeperf:teeperf".format(kernel_version),
    "//kernel_device_modules-{}/drivers/tee/gud:mcDrvModule".format(kernel_version),
    "//kernel_device_modules-{}/drivers/tee/gud:mcDrvModule-ffa".format(kernel_version),
    "//kernel_device_modules-{}/drivers/tee/gud/700/TlcTui:t-base-tui".format(kernel_version),
    "//kernel_device_modules-{}/drivers/thermal/mediatek:backlight_cooling".format(kernel_version),
    "//kernel_device_modules-{}/drivers/thermal/mediatek:board_temp".format(kernel_version),
    "//kernel_device_modules-{}/drivers/thermal/mediatek:charger_cooling".format(kernel_version),
	"//kernel_device_modules-{}/drivers/thermal/mediatek:md_cooling_all".format(kernel_version),
    "//kernel_device_modules-{}/drivers/thermal/mediatek:pmic_temp".format(kernel_version),
    "//kernel_device_modules-{}/drivers/thermal/mediatek:soc_temp_lvts".format(kernel_version),
    "//kernel_device_modules-{}/drivers/thermal/mediatek:thermal_interface".format(kernel_version),
    "//kernel_device_modules-{}/drivers/thermal/mediatek:thermal_trace".format(kernel_version),
    "//kernel_device_modules-{}/drivers/thermal/mediatek:vtskin_temp".format(kernel_version),
    "//kernel_device_modules-{}/drivers/thermal/mediatek:wifi_cooling".format(kernel_version),
    "//kernel_device_modules-{}/drivers/trusty:trusty-core".format(kernel_version),
    "//kernel_device_modules-{}/drivers/trusty:trusty-ipc".format(kernel_version),
    "//kernel_device_modules-{}/drivers/trusty:trusty-log".format(kernel_version),
    "//kernel_device_modules-{}/drivers/trusty:trusty-test".format(kernel_version),
    "//kernel_device_modules-{}/drivers/trusty:trusty-virtio".format(kernel_version),
    "//kernel_device_modules-{}/drivers/usb/mtu3:mtu3".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/performance:mtk_ioctl_touch_boost".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/performance:mtk_ioctl_powerhal".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/performance/touch_boost:touch_boost".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/performance/powerhal_cpu_ctrl:powerhal_cpu_ctrl".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/cameraisp/src/isp_6s:camera_isp".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/cameraisp/src/isp_6s:cam_qos".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/camera_mem:camera_mem".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/cameraisp/mfb/isp_6s:camera_mfb_isp6s".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/cam_timesync:archcounter_timesync".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/ccu/src/isp4:ccu_isp4".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/performance:mtk_perf_ioctl_magt".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/performance:mtk_perf_ioctl".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/performance:mtk_pelt_hint".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/performance:mtk_sbe_ioctl".format(kernel_version),
    "//kernel_device_modules-{}/drivers/watchdog:mtk_wdt".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mtprof:bootprof".format(kernel_version),
    "//kernel_device_modules-{}/drivers/tee/teei/520:isee".format(kernel_version),
    "//kernel_device_modules-{}/drivers/tee/teei/520:isee-ffa".format(kernel_version),
    "//kernel_device_modules-{}/sound/soc/mediatek/audio_dsp:snd-soc-audiodsp-common".format(kernel_version),
    "//kernel_device_modules-{}/sound/soc/mediatek/common:mtk-btcvsd".format(kernel_version),
    "//kernel_device_modules-{}/sound/soc/mediatek/common:mtk-afe-external".format(kernel_version),
    "//kernel_device_modules-{}/sound/soc/mediatek/common:mtk-sp-spk-amp".format(kernel_version),
    "//kernel_device_modules-{}/sound/soc/mediatek/common:snd-soc-mtk-common".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mminfra:mm-fake-engine".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mminfra:mtk-mminfra-util".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mminfra:mtk-mminfra-debug".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mminfra:mtk-mminfra-imax".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/tinysys_scmi:tinysys-scmi".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/vmm:mtk-vmm-notifier".format(kernel_version),
    "//kernel_device_modules-{}/drivers/mailbox:mtk-ise-mailbox".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/ise_trusty:ise-trusty".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/ise_trusty:ise-trusty-log".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/ise_trusty:ise-trusty-ipc".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/ise_trusty:ise-trusty-virtio".format(kernel_version),
    "//kernel_device_modules-{}/drivers/mailbox:mtk-mbox-mailbox".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/lpm:mtk-lpm".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/lpm/governors:lpm-gov-MHSP".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/lpm/modules/debug:mtk-lpm-dbg-common-v2".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/lpm/modules/platform/v2:mtk-lpm-plat-v2".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mkp:mkp".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mmstat:trace_mmstat".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mtk-interconnect:mtk-icc-core".format(kernel_version),
    "//kernel_device_modules-{}/sound/soc/codecs:mt6358-accdet".format(kernel_version),
    "//kernel_device_modules-{}/sound/soc/codecs:mt6368-accdet".format(kernel_version),
    "//kernel_device_modules-{}/sound/soc/codecs:mt6369-accdet".format(kernel_version),
    "//kernel_device_modules-{}/sound/soc/codecs:snd-soc-mt6366".format(kernel_version),
    "//kernel_device_modules-{}/sound/soc/codecs:snd-soc-mt6368".format(kernel_version),
    "//kernel_device_modules-{}/sound/soc/codecs:snd-soc-mt6369".format(kernel_version),
    "//kernel_device_modules-{}/sound/soc/codecs:snd-soc-mt6681".format(kernel_version),
    "//kernel_device_modules-{}/sound/soc/codecs:mt6681-accdet".format(kernel_version),
    "//kernel_device_modules-{}/sound/soc/codecs/richtek:snd-soc-rt5512".format(kernel_version),
    "//kernel_device_modules-{}/sound/soc/codecs/richtek:richtek_spm_cls".format(kernel_version),
    "//kernel_device_modules-{}/sound/soc/codecs/tfa98xx:snd-soc-tfa98xx".format(kernel_version),
    "//kernel_device_modules-{}/drivers/spi:spi-mt65xx".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mme:mme".format(kernel_version),
    "//kernel_device_modules-{}/drivers/memory:mtk-smi".format(kernel_version),
    "//kernel_device_modules-{}/drivers/soc/mediatek/devapc:device-apc-common-legacy".format(kernel_version),
    "//kernel_device_modules-{}/drivers/soc/mediatek/devapc:device-apc-common".format(kernel_version),
    "//kernel_device_modules-{}/drivers/soc/mediatek/devmpu:devmpu".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/widevine_drm:widevine_driver".format(kernel_version),
    "//kernel_device_modules-{}/sound/soc/mediatek/ultrasound/ultra_common:mtk-scp-ultra".format(kernel_version),
    "//kernel_device_modules-{}/sound/soc/mediatek/ultrasound/ultra_scp:snd-soc-mtk-scp-ultra".format(kernel_version),
    "//kernel_device_modules-{}/sound/soc/mediatek/vow:mtk-scp-vow".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mtk_zram:mtk_hwz".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mtk_zram:mtk_zram".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/performance:load_track".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/performance:uload_ind".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/pmic_protect:mtk-pmic-oc-debug".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:mtk_cm_mgr".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:mtk_cm_ipi".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/smi:mtk-smi-dbg".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/cameraisp/rsc:camera_rsc_isp6s".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/cameraisp/dpe:camera_dpe_isp70".format(kernel_version),
    "//kernel_device_modules-{}/drivers/ufs:ufs-mediatek-dbg".format(kernel_version),
    "//kernel_device_modules-{}/drivers/ufs:ufs-mediatek-mod".format(kernel_version),
    "//kernel_device_modules-{}/drivers/ufs:ufs-mediatek-mod-ise".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/hw_sem:mtk-hw-semaphore".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/cmdq/mailbox:mtk-cmdq-drv-ext".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/cameraisp/fdvt/5.1:camera_fdvt_isp51".format(kernel_version),
    "//kernel_device_modules-{}/sound/soc/mediatek/audio_dsp:mtk-soc-offload-common".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mbraink/bridge:mtk_mbraink_bridge".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mbraink/perf:mtk_mbraink_perf".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mm_monitor:mtk-mm-monitor-controller".format(kernel_version),
    "//kernel_device_modules-{}/drivers/interconnect/mediatek:mmqos-common".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mmqos:mmqos_wrapper".format(kernel_version),
    "//kernel_device_modules-{}/drivers/gpu/drm/mediatek/mediatek_v2/v1:mediatek_drm_v1".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/imgsensor/src:imgsensor_isp6s".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/cam_cal/src:camera_eeprom".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/cmdq/mailbox:cmdq-sec-drv".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/mddp:mddp".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/apusys/sapu:sapu".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/sched:cpufreq_sugov_ext".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/cmdq/mailbox:cmdq-test".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/cameraisp/dip/isp_6s:camera_dip_isp6s".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/cameraisp/wpe/isp_6s:camera_wpe_isp6s".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/sched:scheduler".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/performance/sbe:mtk_sbe".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/performance/fpsgo_v8:mtk_fpsgo_v8".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/sched:mtk_core_ctl".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/ssc:mtk-ssc".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/ssc/debug/v1:mtk-ssc-dbg-v1".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/ssc/debug/v2:mtk-ssc-dbg-v2".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/task_turbo:vip_engine".format(kernel_version),
]

mgk_64_kleaf_platform_modules = {
    # keep sorted
    "//kernel_device_modules-{}/drivers/clk/mediatek:clk-bringup".format(kernel_version): "mt6781 mt6789 mt6833 mt6855 mt6895 mt6897 mt6886 mt6893 mt6983 mt6989 mt8192 mt8188 mt6899 mt6993",
    "//kernel_device_modules-{}/drivers/clk/mediatek:clk-mt6993".format(kernel_version): "mt6993",
    "//kernel_device_modules-{}/drivers/clk/mediatek:clk-mt6993-vcodec".format(kernel_version): "mt6993",
    "//kernel_device_modules-{}/drivers/clk/mediatek:clk-mt6993-adsp".format(kernel_version): "mt6993",
    "//kernel_device_modules-{}/drivers/clk/mediatek:clk-mt6993-mmsys".format(kernel_version): "mt6993",
    "//kernel_device_modules-{}/drivers/clk/mediatek:clk-mt6993-img".format(kernel_version): "mt6993",
    "//kernel_device_modules-{}/drivers/clk/mediatek:clk-mt6993-peri".format(kernel_version): "mt6993",
    "//kernel_device_modules-{}/drivers/clk/mediatek:clk-mt6993-ifrao".format(kernel_version): "mt6993",
    "//kernel_device_modules-{}/drivers/clk/mediatek:clk-mt6993-cam".format(kernel_version): "mt6993",
    "//kernel_device_modules-{}/drivers/clk/mediatek:clk-mt6993-pd".format(kernel_version): "mt6993",
    "//kernel_device_modules-{}/drivers/clk/mediatek:clk-chk-mt6993".format(kernel_version): "mt6993",
    "//kernel_device_modules-{}/drivers/clk/mediatek:clk-dbg-mt6993".format(kernel_version): "mt6993",
    "//kernel_device_modules-{}/drivers/clk/mediatek:clk-fmeter-mt6993".format(kernel_version): "mt6993",
    "//kernel_device_modules-{}/drivers/clk/mediatek:pd-chk-mt6993".format(kernel_version): "mt6993",
    "//kernel_device_modules-{}/drivers/gpu/mediatek/gpu_iommu:mtk_gpu_iommu_mt6991".format(kernel_version): "mt6991",
    "//kernel_device_modules-{}/drivers/gpu/mediatek/gpu_iommu:mtk_gpu_iommu_mt6993".format(kernel_version): "mt6993",
    "//kernel_device_modules-{}/drivers/gpu/mediatek/gpueb:mtk_ghpm_mt6991".format(kernel_version): "mt6991",
    "//kernel_device_modules-{}/drivers/gpu/mediatek/gpueb:mtk_ghpm_mt6993".format(kernel_version): "mt6993",
    "//kernel_device_modules-{}/drivers/gpu/mediatek/gpu_pdma:mtk_gpu_pdma_mt6991".format(kernel_version): "mt6991",
    "//kernel_device_modules-{}/drivers/gpu/mediatek/gpu_pdma:mtk_gpu_pdma_mt6993".format(kernel_version): "mt6993",
    "//kernel_device_modules-{}/drivers/gpu/mediatek/gpufreq/v2:mtk_gpufreq_mt6991".format(kernel_version): "mt6991",
    "//kernel_device_modules-{}/drivers/gpu/mediatek/gpufreq/v2:mtk_gpufreq_mt6993".format(kernel_version): "mt6993",
    "//kernel_device_modules-{}/drivers/gpu/mediatek/gpufreq/v2:mtk_gpufreq_mt6855".format(kernel_version): "mt6855",
    "//kernel_device_modules-{}/drivers/gpu/mediatek/gpufreq/v2:mtk_gpufreq_mt6895".format(kernel_version): "mt6895",
    "//kernel_device_modules-{}/drivers/gpu/mediatek/ged:mtk_ged_mt6991".format(kernel_version): "mt6991",
    "//kernel_device_modules-{}/drivers/gpu/mediatek/ged:mtk_ged_mt6993".format(kernel_version): "mt6993",
    "//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:mtk_cm_mgr_mt6991".format(kernel_version): "mt6991",
    "//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:mtk_cm_mgr_mt6993".format(kernel_version): "mt6993",
    "//kernel_device_modules-{}/drivers/misc/mediatek/dcm:mt6789_dcm".format(kernel_version): "mt6789",
    "//kernel_device_modules-{}/drivers/misc/mediatek/dcm:mt6855_dcm".format(kernel_version): "mt6855",
    "//kernel_device_modules-{}/drivers/misc/mediatek/dcm:mt6895_dcm".format(kernel_version): "mt6895",
    "//kernel_device_modules-{}/drivers/misc/mediatek/dcm:mt6991_dcm".format(kernel_version): "mt6991",
    "//kernel_device_modules-{}/drivers/misc/mediatek/dcm:mt6993_dcm".format(kernel_version): "mt6993",
    "//kernel_device_modules-{}/drivers/misc/mediatek/lpm/modules/debug/mt6991:mtk-lpm-dbg-mt6991".format(kernel_version): "mt6991",
    "//kernel_device_modules-{}/drivers/misc/mediatek/lpm/modules/debug/v6993:mtk-lpm-dbg-v6993".format(kernel_version): "mt6993",
    "//kernel_device_modules-{}/drivers/misc/mediatek/slbc:slbc_mt6895".format(kernel_version): "mt6895",
    "//kernel_device_modules-{}/drivers/misc/mediatek/slbc:slbc_mt6991".format(kernel_version): "mt6991",
    "//kernel_device_modules-{}/drivers/misc/mediatek/slbc:slbc_mt6993".format(kernel_version): "mt6993",
    "//kernel_device_modules-{}/drivers/misc/mediatek/vmm:mtk-vmm-notifier-mt6991".format(kernel_version): "mt6991",
    "//kernel_device_modules-{}/drivers/misc/mediatek/vmm:mtk-vmm-notifier-mt6993".format(kernel_version): "mt6993",
    "//kernel_device_modules-{}/drivers/misc/mediatek/vmm_spm:mtk_vmm_spm_mt6989".format(kernel_version): "mt6989",
    "//kernel_device_modules-{}/drivers/misc/mediatek/mcupm/v3:mcupm".format(kernel_version): "mt6993",
    "//kernel_device_modules-{}/drivers/misc/mediatek/mcupm/v2:mcupm".format(kernel_version): "mt6991 mt6789 mt6895 mt6855",
    "//kernel_device_modules-{}/drivers/pinctrl/mediatek:pinctrl-mt6789".format(kernel_version): "mt6789",
    "//kernel_device_modules-{}/drivers/pinctrl/mediatek:pinctrl-mt6855".format(kernel_version): "mt6855",
    "//kernel_device_modules-{}/drivers/pinctrl/mediatek:pinctrl-mt6886".format(kernel_version): "mt6886",
    "//kernel_device_modules-{}/drivers/pinctrl/mediatek:pinctrl-mt6895".format(kernel_version): "mt6895",
    "//kernel_device_modules-{}/drivers/pinctrl/mediatek:pinctrl-mt6897".format(kernel_version): "mt6897",
    "//kernel_device_modules-{}/drivers/pinctrl/mediatek:pinctrl-mt6899".format(kernel_version): "mt6899",
    "//kernel_device_modules-{}/drivers/pinctrl/mediatek:pinctrl-mt6983".format(kernel_version): "mt6983",
    "//kernel_device_modules-{}/drivers/pinctrl/mediatek:pinctrl-mt6985".format(kernel_version): "mt6985",
    "//kernel_device_modules-{}/drivers/pinctrl/mediatek:pinctrl-mt6989".format(kernel_version): "mt6989",
    "//kernel_device_modules-{}/drivers/pinctrl/mediatek:pinctrl-mt6991".format(kernel_version): "mt6991",
    "//kernel_device_modules-{}/drivers/pinctrl/mediatek:pinctrl-mt6993".format(kernel_version): "mt6993",
    "//kernel_device_modules-{}/drivers/soc/mediatek:mtk-scpsys-bringup".format(kernel_version): "mt6789 mt6833 mt6855 mt6895 mt6886 mt6983 mt6989 mt8192 mt8188 ",
    #"//kernel_device_modules-{}/drivers/soc/mediatek:mtk-scpsys-mt6761".format(kernel_version): "mt6761",
    "//kernel_device_modules-{}/drivers/soc/mediatek:mtk-scpsys-mt6789".format(kernel_version): "mt6789",
    "//kernel_device_modules-{}/drivers/soc/mediatek:mtk-scpsys-mt6833".format(kernel_version): "mt6833",
    #"//kernel_device_modules-{}/drivers/soc/mediatek:mtk-scpsys-mt6877".format(kernel_version): "mt6877",
    "//kernel_device_modules-{}/drivers/soc/mediatek:mtk-scpsys-mt6886".format(kernel_version): "mt6886",
    "//kernel_device_modules-{}/drivers/soc/mediatek:mtk-scpsys-mt6895".format(kernel_version): "mt6895",
    "//kernel_device_modules-{}/drivers/soc/mediatek:mtk-scpsys-mt6983".format(kernel_version): "mt6983",
    #"//kernel_device_modules-{}/drivers/soc/mediatek:mtk-scpsys-mt6985".format(kernel_version): "mt6985",
    "//kernel_device_modules-{}/drivers/soc/mediatek:mtk-scpsys-mt6991-spm".format(kernel_version): "mt6991",
    "//kernel_device_modules-{}/drivers/soc/mediatek:mtk-scpsys-mt6991-mmpc".format(kernel_version): "mt6991",
    "//kernel_device_modules-{}/drivers/soc/mediatek:mtk-scpsys-mt6989".format(kernel_version): "mt6989",
    "//kernel_device_modules-{}/drivers/soc/mediatek:mtk-scpsys-mt6855".format(kernel_version): "mt6855",
    "//kernel_device_modules-{}/drivers/soc/mediatek/mmdvfs:mmdvfs-mt6993".format(kernel_version): "mt6993",
    "//kernel_device_modules-{}/drivers/misc/mediatek/memory-amms:memory-amms".format(kernel_version): "mt6761 mt6765 mt6768 mt6781 mt6833 mt6853 mt6877",
    "//kernel_device_modules-{}/drivers/misc/mediatek/mm_monitor:mm-monitor-platform-mt6789".format(kernel_version): "mt6789",
    "//kernel_device_modules-{}/drivers/misc/mediatek/mm_monitor:mm-monitor-platform-mt6855".format(kernel_version): "mt6855",
    "//kernel_device_modules-{}/drivers/misc/mediatek/mm_monitor:mm-monitor-platform-mt6895".format(kernel_version): "mt6895",
    "//kernel_device_modules-{}/drivers/misc/mediatek/mm_monitor:mm-monitor-platform-mt6991".format(kernel_version): "mt6991",
    "//kernel_device_modules-{}/drivers/misc/mediatek/mm_monitor:mm-monitor-platform-mt6993".format(kernel_version): "mt6993",
    "//kernel_device_modules-{}/drivers/interconnect/mediatek:mmqos-mt6789".format(kernel_version): "mt6789",
    "//kernel_device_modules-{}/drivers/interconnect/mediatek:mmqos-mt6991".format(kernel_version): "mt6991",
    "//kernel_device_modules-{}/drivers/interconnect/mediatek:mmqos-mt6993".format(kernel_version): "mt6993",
    "//kernel_device_modules-{}/drivers/interconnect/mediatek:mmqos-mt6855".format(kernel_version): "mt6855",
    "//kernel_device_modules-{}/drivers/interconnect/mediatek:mmqos-mt6895".format(kernel_version): "mt6895",
    "//kernel_device_modules-{}/drivers/clk/mediatek:clk-chk-mt6789".format(kernel_version): "mt6789",
    "//kernel_device_modules-{}/drivers/clk/mediatek:clk-dbg-mt6789".format(kernel_version): "mt6789",
    "//kernel_device_modules-{}/drivers/clk/mediatek:clk-fmeter-mt6789".format(kernel_version): "mt6789",
    "//kernel_device_modules-{}/drivers/clk/mediatek:clk-mt6789".format(kernel_version): "mt6789",
    "//kernel_device_modules-{}/drivers/clk/mediatek:clk-mt6789-audiosys".format(kernel_version): "mt6789",
    "//kernel_device_modules-{}/drivers/clk/mediatek:clk-mt6789-cam".format(kernel_version): "mt6789",
    "//kernel_device_modules-{}/drivers/clk/mediatek:clk-mt6789-mmsys".format(kernel_version): "mt6789",
    "//kernel_device_modules-{}/drivers/clk/mediatek:clk-mt6789-img".format(kernel_version): "mt6789",
    "//kernel_device_modules-{}/drivers/clk/mediatek:clk-mt6789-i2c".format(kernel_version): "mt6789",
    "//kernel_device_modules-{}/drivers/clk/mediatek:clk-mt6789-bus".format(kernel_version): "mt6789",
    "//kernel_device_modules-{}/drivers/clk/mediatek:clk-mt6789-vcodec".format(kernel_version): "mt6789",
    "//kernel_device_modules-{}/drivers/clk/mediatek:clk-mt6789-disp_dsc".format(kernel_version): "mt6789",
    "//kernel_device_modules-{}/drivers/clk/mediatek:clk-mt6789-mfgcfg".format(kernel_version): "mt6789",
    "//kernel_device_modules-{}/drivers/clk/mediatek:clk-mt6789-impen".format(kernel_version): "mt6789",
    "//kernel_device_modules-{}/drivers/clk/mediatek:pd-chk-mt6789".format(kernel_version): "mt6789",
    #"//kernel_device_modules-{}/drivers/clk/mediatek:clk-mt6985".format(kernel_version): "mt6985",
    #"//kernel_device_modules-{}/drivers/clk/mediatek:clk-mt6985-adsp".format(kernel_version): "mt6985",
    #"//kernel_device_modules-{}/drivers/clk/mediatek:clk-mt6985-bus".format(kernel_version): "mt6985",
    #"//kernel_device_modules-{}/drivers/clk/mediatek:clk-mt6985-cam".format(kernel_version): "mt6985",
    #"//kernel_device_modules-{}/drivers/clk/mediatek:clk-mt6985-ccu".format(kernel_version): "mt6985",
    #"//kernel_device_modules-{}/drivers/clk/mediatek:clk-mt6985-img".format(kernel_version): "mt6985",
    #"//kernel_device_modules-{}/drivers/clk/mediatek:clk-mt6985-mdpsys".format(kernel_version): "mt6985",
    #"//kernel_device_modules-{}/drivers/clk/mediatek:clk-mt6985-mmsys".format(kernel_version): "mt6985",
    #"//kernel_device_modules-{}/drivers/clk/mediatek:clk-mt6985-peri".format(kernel_version): "mt6985",
    #"//kernel_device_modules-{}/drivers/clk/mediatek:clk-mt6985-vcodec".format(kernel_version): "mt6985",
    #"//kernel_device_modules-{}/drivers/clk/mediatek:clk-mt6985-vlp".format(kernel_version): "mt6985",
    #"//kernel_device_modules-{}/drivers/clk/mediatek:clk-chk-mt6985".format(kernel_version): "mt6985",
    #"//kernel_device_modules-{}/drivers/clk/mediatek:clk-dbg-mt6985".format(kernel_version): "mt6985",
    #"//kernel_device_modules-{}/drivers/clk/mediatek:clk-fmeter-mt6985".format(kernel_version): "mt6985",
    #"//kernel_device_modules-{}/drivers/clk/mediatek:pd-chk-mt6985".format(kernel_version): "mt6985",
    "//kernel_device_modules-{}/drivers/clk/mediatek:clk-mt6991".format(kernel_version): "mt6991",
    "//kernel_device_modules-{}/drivers/clk/mediatek:clk-mt6991-adsp".format(kernel_version): "mt6991",
    "//kernel_device_modules-{}/drivers/clk/mediatek:clk-mt6991-bus".format(kernel_version): "mt6991",
    "//kernel_device_modules-{}/drivers/clk/mediatek:clk-mt6991-cam".format(kernel_version): "mt6991",
    "//kernel_device_modules-{}/drivers/clk/mediatek:clk-mt6991-img".format(kernel_version): "mt6991",
    "//kernel_device_modules-{}/drivers/clk/mediatek:clk-mt6991-mdpsys".format(kernel_version): "mt6991",
    "//kernel_device_modules-{}/drivers/clk/mediatek:clk-mt6991-mmsys".format(kernel_version): "mt6991",
    "//kernel_device_modules-{}/drivers/clk/mediatek:clk-mt6991-pd".format(kernel_version): "mt6991",
    "//kernel_device_modules-{}/drivers/clk/mediatek:clk-mt6991-peri".format(kernel_version): "mt6991",
    "//kernel_device_modules-{}/drivers/clk/mediatek:clk-mt6991-vcodec".format(kernel_version): "mt6991",
    "//kernel_device_modules-{}/drivers/clk/mediatek:clk-chk-mt6991".format(kernel_version): "mt6991",
    "//kernel_device_modules-{}/drivers/clk/mediatek:clk-dbg-mt6991".format(kernel_version): "mt6991",
    "//kernel_device_modules-{}/drivers/clk/mediatek:clk-fmeter-mt6991".format(kernel_version): "mt6991",
    "//kernel_device_modules-{}/drivers/clk/mediatek:pd-chk-mt6991".format(kernel_version): "mt6991",
    "//kernel_device_modules-{}/drivers/clk/mediatek:clk-mt6855".format(kernel_version): "mt6855",
    "//kernel_device_modules-{}/drivers/clk/mediatek:clk-dbg-mt6855".format(kernel_version): "mt6855",
    "//kernel_device_modules-{}/drivers/clk/mediatek:pd-chk-mt6855".format(kernel_version): "mt6855",
    "//kernel_device_modules-{}/drivers/clk/mediatek:clk-fmeter-mt6855".format(kernel_version): "mt6855",
    "//kernel_device_modules-{}/drivers/clk/mediatek:clk-chk-mt6855".format(kernel_version): "mt6855",
    "//kernel_device_modules-{}/drivers/clk/mediatek:clk-mt6855-vcodec".format(kernel_version): "mt6855",
    "//kernel_device_modules-{}/drivers/clk/mediatek:clk-mt6855-adsp".format(kernel_version): "mt6855",
    "//kernel_device_modules-{}/drivers/clk/mediatek:clk-mt6855-cam_m".format(kernel_version): "mt6855",
    "//kernel_device_modules-{}/drivers/clk/mediatek:clk-mt6855-cam".format(kernel_version): "mt6855",
    "//kernel_device_modules-{}/drivers/clk/mediatek:clk-mt6855-mmsys".format(kernel_version): "mt6855",
    "//kernel_device_modules-{}/drivers/clk/mediatek:clk-mt6855-imgsys1".format(kernel_version): "mt6855",
    "//kernel_device_modules-{}/drivers/clk/mediatek:clk-mt6855-imgsys2".format(kernel_version): "mt6855",
    "//kernel_device_modules-{}/drivers/clk/mediatek:clk-mt6855-peri".format(kernel_version): "mt6855",
    "//kernel_device_modules-{}/drivers/clk/mediatek:clk-mt6855-bus".format(kernel_version): "mt6855",
    "//kernel_device_modules-{}/drivers/clk/mediatek:clk-mt6855-img".format(kernel_version): "mt6855",
    "//kernel_device_modules-{}/drivers/clk/mediatek:clk-mt6855-mdp1".format(kernel_version): "mt6855",
    "//kernel_device_modules-{}/drivers/clk/mediatek:clk-mt6855-mdp".format(kernel_version): "mt6855",


    "//kernel_device_modules-{}/drivers/clk/mediatek:clk-mt6895".format(kernel_version): "mt6895",
    "//kernel_device_modules-{}/drivers/clk/mediatek:clk-fmeter-mt6895".format(kernel_version): "mt6895",
    "//kernel_device_modules-{}/drivers/clk/mediatek:clk-chk-mt6895".format(kernel_version): "mt6895",
    "//kernel_device_modules-{}/drivers/clk/mediatek:clk-dbg-mt6895".format(kernel_version): "mt6895",
    "//kernel_device_modules-{}/drivers/clk/mediatek:pd-chk-mt6895".format(kernel_version): "mt6895",
    "//kernel_device_modules-{}/drivers/clk/mediatek:clk-mt6895-adsp".format(kernel_version): "mt6895",
    "//kernel_device_modules-{}/drivers/clk/mediatek:clk-mt6895-cam".format(kernel_version): "mt6895",
    "//kernel_device_modules-{}/drivers/clk/mediatek:clk-mt6895-img".format(kernel_version): "mt6895",
    "//kernel_device_modules-{}/drivers/clk/mediatek:clk-mt6895-mmsys".format(kernel_version): "mt6895",
    "//kernel_device_modules-{}/drivers/clk/mediatek:clk-mt6895-peri".format(kernel_version): "mt6895",
    "//kernel_device_modules-{}/drivers/clk/mediatek:clk-mt6895-bus".format(kernel_version): "mt6895",
    "//kernel_device_modules-{}/drivers/clk/mediatek:clk-mt6895-mdpsys".format(kernel_version): "mt6895",
    "//kernel_device_modules-{}/drivers/clk/mediatek:clk-mt6895-vcodec".format(kernel_version): "mt6895",
    "//kernel_device_modules-{}/drivers/clk/mediatek:clk-mt6895-ccu".format(kernel_version): "mt6895",

    "//kernel_device_modules-{}/drivers/soc/mediatek/devapc:device-apc-mt6768".format(kernel_version): "mt6768",
    "//kernel_device_modules-{}/drivers/soc/mediatek/devapc:device-apc-mt6789".format(kernel_version): "mt6789",
    "//kernel_device_modules-{}/drivers/soc/mediatek/devapc:device-apc-mt6895".format(kernel_version): "mt6895",
    "//kernel_device_modules-{}/drivers/soc/mediatek/devapc:device-apc-mt6991".format(kernel_version): "mt6991",
    "//kernel_device_modules-{}/drivers/soc/mediatek/devapc:device-apc-mt6993".format(kernel_version): "mt6993",
    "//kernel_device_modules-{}/drivers/misc/mediatek/cameraisp/pda:pda_drv_mt6855".format(kernel_version): "mt6855",
    "//kernel_device_modules-{}/drivers/misc/mediatek/cameraisp/pda:pda_drv_mt6879".format(kernel_version): "mt6879",
    "//kernel_device_modules-{}/drivers/misc/mediatek/cameraisp/pda:pda_drv_mt6895".format(kernel_version): "mt6895",
    "//kernel_device_modules-{}/drivers/misc/mediatek/cameraisp/pda:pda_drv_mt6878".format(kernel_version): "mt6878",

  ## write vendor file by platform here
    "//vendor/mediatek/kernel_modules/mtkcam/camsys:mtk-cam-plat-mt6983":"mt6983",
    "//vendor/mediatek/kernel_modules/mtkcam/camsys:mtk-cam-plat-mt6895":"mt6895",
    "//vendor/mediatek/kernel_modules/mtkcam/camsys:mtk-cam-plat-mt6879":"mt6879",
    "//vendor/mediatek/kernel_modules/mtkcam/mtk-pda:pda_drv_mt6991":"mt6991",
    "//vendor/mediatek/kernel_modules/mtkcam/mtk-pda:pda_drv_mt6993":"mt6993",
    "//vendor/mediatek/kernel_modules/mtkcam/imgsys/imgsys/isp8s:imgsys_8s":"mt6993",

    "//vendor/mediatek/kernel_modules/gpu:gpu_mt6895":"mt6895",

    "//vendor/mediatek/kernel_modules/gpu/gpu_mali/mali_avalon/a16w/drivers/gpu/arm/midgard:mali_kbase_mt6993_a16w":"mt6993",
    "//vendor/mediatek/kernel_modules/gpu/gpu_mali/mali_avalon/a16w/drivers/base/arm:mali_mgm_mt6993_a16w":"mt6993",
    "//vendor/mediatek/kernel_modules/gpu/gpu_mali/mali_avalon/a16w/drivers/base/arm:mali_prot_alloc_mt6993_a16w":"mt6993",
    "//vendor/mediatek/kernel_modules/gpu/gpu_mali/mali_avalon/a16w/drivers/base/arm:mali_dmabuf_test_mt6993_a16w":"mt6993",

    "//vendor/mediatek/kernel_modules/gpu/gpu_mali/mali_avalon/a16w/drivers/gpu/arm/midgard:mali_kbase_mt6991_a16w":"mt6991",
    "//vendor/mediatek/kernel_modules/gpu/gpu_mali/mali_avalon/a16w/drivers/base/arm:mali_mgm_mt6991_a16w":"mt6991",
    "//vendor/mediatek/kernel_modules/gpu/gpu_mali/mali_avalon/a16w/drivers/base/arm:mali_prot_alloc_mt6991_a16w":"mt6991",
    "//vendor/mediatek/kernel_modules/gpu/gpu_mali/mali_avalon/a16w/drivers/base/arm:mali_dmabuf_test_mt6991_a16w":"mt6991",

    "//kernel_device_modules-{}/drivers/misc/mediatek/cmdq/mailbox:cmdq-platform-mt6991".format(kernel_version): "mt6991",
    "//kernel_device_modules-{}/drivers/misc/mediatek/cmdq/mailbox:cmdq-platform-mt6993".format(kernel_version): "mt6993",
    "//kernel_device_modules-{}/drivers/misc/mediatek/cmdq/mailbox:cmdq-platform-mt6855".format(kernel_version): "mt6855",
    "//kernel_device_modules-{}/drivers/misc/mediatek/cmdq/mailbox:cmdq-platform-mt6789".format(kernel_version): "mt6789",
    "//kernel_device_modules-{}/drivers/misc/mediatek/cmdq/mailbox:cmdq-platform-mt6878".format(kernel_version): "mt6878",
    "//kernel_device_modules-{}/drivers/misc/mediatek/cmdq/mailbox:cmdq-platform-mt6895".format(kernel_version): "mt6895",
    "//vendor/mediatek/kernel_modules/mtkcam/mtk-dpe:camera_dpe_isp8":"mt6991",

    "//kernel_device_modules-{}/drivers/misc/mediatek/mdp:mdp_drv_mt6789".format(kernel_version): "mt6789",
    "//kernel_device_modules-{}/drivers/misc/mediatek/mdp:mdp_drv_mt6855".format(kernel_version): "mt6855",
    "//kernel_device_modules-{}/drivers/misc/mediatek/mdp:mdp_drv_mt6858".format(kernel_version): "mt6858",
    "//kernel_device_modules-{}/drivers/misc/mediatek/mdp:mdp_drv_mt6886".format(kernel_version): "mt6886",
    "//kernel_device_modules-{}/drivers/misc/mediatek/mdp:mdp_drv_mt6895".format(kernel_version): "mt6895",
    "//kernel_device_modules-{}/drivers/misc/mediatek/mdp:mdp_drv_mt6897".format(kernel_version): "mt6897",
    "//kernel_device_modules-{}/drivers/misc/mediatek/mdp:mdp_drv_mt6991".format(kernel_version): "mt6991",

    "//kernel_device_modules-{}/drivers/gpu/drm/mediatek/mml:mtk-mml-mt6878".format(kernel_version): "mt6878",
    "//kernel_device_modules-{}/drivers/gpu/drm/mediatek/mml:mtk-mml-mt6895".format(kernel_version): "mt6895",
    "//kernel_device_modules-{}/drivers/gpu/drm/mediatek/mml:mtk-mml-mt6991".format(kernel_version): "mt6991",
    "//kernel_device_modules-{}/drivers/gpu/drm/mediatek/mml:mtk-mml-mt6993".format(kernel_version): "mt6993",

    "//kernel_device_modules-{}/sound/soc/mediatek/mt6855:snd-soc-mt6855-afe".format(kernel_version): "mt6855",
    "//kernel_device_modules-{}/sound/soc/mediatek/mt6855:mt6855-mt6369".format(kernel_version): "mt6855",
    "//kernel_device_modules-{}/sound/soc/mediatek/mt6991:snd-soc-mt6991-afe".format(kernel_version): "mt6991",
    "//kernel_device_modules-{}/sound/soc/mediatek/mt6991:mt6991-mt6681".format(kernel_version): "mt6991",
    "//kernel_device_modules-{}/sound/soc/mediatek/mt6993:snd-soc-mt6993-afe".format(kernel_version): "mt6993",
    "//kernel_device_modules-{}/sound/soc/mediatek/mt6993:mt6993-mt6681".format(kernel_version): "mt6993",
}

mgk_64_kleaf_eng_device_modules = [
    # keep sorted
    "//kernel_device_modules-{}/drivers/misc/mediatek/cpufreq_v1:cpuhvfs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/irq_monitor:irq_monitor".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/locking:locking_aee".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/selinux_warning:mtk_selinux_aee_warning".format(kernel_version),
]

mgk_64_kleaf_platform_eng_modules = {
    # keep sorted

    ## write vendor file by platform here

}

mgk_64_kleaf_userdebug_device_modules = [
    # keep sorted
    "//kernel_device_modules-{}/drivers/misc/mediatek/cpufreq_v1:cpuhvfs".format(kernel_version),
    "//kernel_device_modules-{}/drivers/misc/mediatek/irq_monitor:irq_monitor".format(kernel_version),
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
    #"drivers/char/hw_random/sec-rng.ko",
    #"drivers/clk/mediatek/fhctl.ko",
    "drivers/misc/mediatek/pkvm_smmu/pkvm_smmu.ko",
    #"drivers/dma/mediatek/mtk-cqdma.ko",
    #"drivers/dma/mediatek/mtk-uart-apdma.ko",
    #"drivers/gpu/drm/mediatek/mediatek_v2/mediatek-drm.ko",
    #"drivers/gpu/drm/mediatek/mediatek_v2/v1/mediatek_drm_v1.ko",
    #"drivers/gpu/mediatek/gpu_bm/mtk_gpu_qos.ko",
    #"drivers/gpu/mediatek/gpu_bm/mtk_gpu_qos_bringup.ko",
    #"drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_wrapper.ko",
    #"drivers/i2c/busses/i2c-mt65xx.ko",
    #"drivers/i3c/master/mtk-i3c-master-mt69xx.ko",
    #"drivers/iio/adc/mt6577_auxadc.ko",
    #"drivers/iio/adc/mt6681-auxadc.ko",
    #"drivers/input/touchscreen/BoTai_Multi_Touch/BoTai_touch_one/botai_touch_one.ko",
    #"drivers/input/touchscreen/BoTai_Multi_Touch/BoTai_touch_two/botai_touch_two.ko",
    #"drivers/input/touchscreen/GT9886/gt9886.ko",
    #"drivers/input/touchscreen/GT9895/gt9895.ko",
    #"drivers/input/touchscreen/ts_scp/ts_scp_common.ko",
    #"drivers/input/touchscreen/GT9896S/gt9896s.ko",
    #"drivers/input/touchscreen/GT1151/gt1151.ko",
    #"drivers/input/touchscreen/NT36532/nt36532.ko",
    #"drivers/input/touchscreen/GT9966/gt9966.ko",
    #"drivers/input/touchscreen/gt9xx/gt9xx_touch.ko",
    #"drivers/input/touchscreen/ILITEK/ilitek_i2c.ko",
    #"drivers/input/touchscreen/NT36672C_I2C/nt36672c_i2c.ko",
    #"drivers/input/touchscreen/tui-common.ko",
    #"drivers/interconnect/mediatek/mmqos-common.ko",

    #"drivers/media/platform/mtk-jpeg/mtk_jpeg.ko",
    "drivers/media/platform/mtk-vcodec/mtk-vcodec-enc-v1.ko",
    "drivers/media/platform/mtk-vcodec/mtk-vcodec-enc-v2.ko",
    #"drivers/misc/mediatek/adsp/adsp.ko",
    #"drivers/misc/mediatek/adsp/v1/adsp-v1.ko",
    #"drivers/misc/mediatek/adsp/v2/adsp-v2.ko",
    #"drivers/misc/mediatek/adsp/v3/adsp-v3.ko",
    #"drivers/misc/mediatek/apusys/apusys.ko",
    #"drivers/misc/mediatek/apusys/apu_aov.ko",
    #"drivers/misc/mediatek/apusys/power/apu_top.ko",
    #"drivers/misc/mediatek/apusys/sapu/sapu.ko",
    #"drivers/misc/mediatek/audio_ipi/audio_ipi.ko",
    #"drivers/misc/mediatek/cameraisp/dpe/camera_dpe_isp70.ko",
    #"drivers/misc/mediatek/clkbuf/clkbuf.ko",
    #"drivers/misc/mediatek/cmdq/mailbox/cmdq-sec-drv.ko",
    #"drivers/misc/mediatek/cmdq/mailbox/cmdq-test.ko",
    #"drivers/misc/mediatek/cmdq/mailbox/mtk-cmdq-drv-ext.ko",
    #"drivers/misc/mediatek/cm_mgr/mtk_cm_ipi.ko",
    #"drivers/misc/mediatek/cm_mgr/mtk_cm_mgr.ko",
    #drivers/misc/mediatek/connectivity/connadp.ko",
    #drivers/misc/mediatek/conn_scp/connscp.ko",
    #"drivers/misc/mediatek/cci_lite/ccidvfs.ko",
    #"drivers/misc/mediatek/cross_sched/cross_sched.ko",
    #"drivers/misc/mediatek/dvfsrc/mtk-dvfsrc-helper.ko",
    #"drivers/misc/mediatek/flashlight/flashlight.ko",
    #"drivers/misc/mediatek/flashlight/v4l2/lm3643.ko",
    #"drivers/misc/mediatek/flashlight/v4l2/lm3644.ko",
    #"drivers/misc/mediatek/i3c_i2c_wrap/mtk-i3c-i2c-wrap.ko",
    #"drivers/misc/mediatek/imgsensor/src/isp6s/imgsensor_isp6s.ko",
    #"drivers/misc/mediatek/cameraisp/dip/isp_6s/camera_dip_isp6s.ko",
    # "drivers/misc/mediatek/cam_cal/src/custom/camera_eeprom.ko",
    #"drivers/misc/mediatek/ise_lpm/ise_lpm.ko",
    #"drivers/misc/mediatek/ise_lpm/ise_lpm_v2.ko",
    #"drivers/misc/mediatek/jpeg/jpeg-driver.ko",
    #"drivers/misc/mediatek/lpm/modules/debug/v1/mtk-lpm-dbg-common-v1.ko",
    #"drivers/misc/mediatek/lpm/modules/platform/v1/mtk-lpm-plat-v1.ko",
    #"drivers/misc/mediatek/masp/sec.ko",
    "drivers/misc/mediatek/mbraink/mtk_mbraink.ko",
    "drivers/misc/mediatek/mbraink/modules/v6989/mtk_mbraink_v6989.ko",
    "drivers/misc/mediatek/mbraink/modules/v6991/mtk_mbraink_v6991.ko",
    "drivers/misc/mediatek/mbraink/modules/v6899/mtk_mbraink_v6899.ko",
    "drivers/misc/mediatek/mbraink/modules/v6993/mtk_mbraink_v6993.ko",
    #"drivers/misc/mediatek/mddp/mddp.ko",
    #"drivers/misc/mediatek/mdp/cmdq_helper_inf.ko",
    #"drivers/misc/mediatek/mddp/mddp.ko",
    #"drivers/misc/mediatek/mdp/mdp_drv_dummy.ko",
    #"drivers/misc/mediatek/mminfra/mtk-mminfra-util.ko",
    #"drivers/misc/mediatek/mminfra/mtk-mminfra-debug.ko",
    #"drivers/misc/mediatek/mminfra/mtk-mminfra-imax.ko",
    #"drivers/misc/mediatek/mmp/src/mmprofile.ko",
    #"drivers/misc/mediatek/mme/src/mme.ko",
    #"drivers/misc/mediatek/mmqos/mmqos_wrapper.ko",
    #"drivers/misc/mediatek/mm_monitor/mtk-mm-monitor-controller.ko",
    #"drivers/misc/mediatek/pbm/mtk_peak_power_budget.ko",
    # "drivers/misc/mediatek/cg_ppt/mtk_cg_peak_power_throttling.ko",
    #"drivers/misc/mediatek/perf_common/mtk_perf_common.ko",
    #"drivers/misc/mediatek/performance/fpsgo_v8/mtk_fpsgo_v8.ko",
    "drivers/misc/mediatek/performance/fpsgo_v3/mtk_fpsgo_v3.ko",
    "drivers/misc/mediatek/performance/frs/frs.ko",
    #"drivers/misc/mediatek/performance/sbe/mtk_sbe.ko",
    "drivers/misc/mediatek/pkvm_mgmt/pkvm_mgmt.ko",
    "drivers/misc/mediatek/pkvm_tmem/pkvm_tmem.ko",
    "drivers/misc/mediatek/pkvm_mkp/pkvm_mkp.ko",
    "drivers/misc/mediatek/performance/game/mtk_game.ko",
    #"drivers/misc/mediatek/power_throttling/mtk_md_power_throttling.ko",
    #"drivers/misc/mediatek/qos/mtk_qos.ko",
    #"drivers/misc/mediatek/sched/cpufreq_sugov_ext.ko",
    #"drivers/misc/mediatek/sched/mtk_core_ctl.ko",
    #"drivers/misc/mediatek/sched/scheduler.ko",
    #"drivers/misc/mediatek/sensor/2.0/sensorhub/sensorhub.ko",
    #"drivers/misc/mediatek/task_turbo/task_turbo.ko",
    #"drivers/misc/mediatek/task_turbo/vip_engine.ko",
    #"drivers/misc/mediatek/vdec_fmt/vdec-fmt.ko",
    #"drivers/misc/mediatek/vmm/mtk-vmm-notifier.ko",
    #"drivers/misc/mediatek/widevine_drm/widevine_driver.ko",
    #"drivers/mmc/host/mtk-mmc.ko",
    #"drivers/mmc/host/mtk-sd.ko",
    #"drivers/mmc/host/mtk-mmc-wp.ko",
    #"drivers/nvmem/nvmem-mt6681-efuse.ko",
    #"drivers/pwm/pwm-mtk-disp.ko",
    #"drivers/regulator/mt6681-regulator.ko",
    #"drivers/regulator/mtk-vmm-isp71-regulator.ko",
    #"drivers/remoteproc/mtk_ccu.ko",
    #"drivers/soc/mediatek/mtk-mmsys.ko",
    #"drivers/soc/mediatek/mtk-mutex.ko",
    "drivers/soc/mediatek/mtk-socinfo.ko",
    #"drivers/tee/gud/700/TlcTui/t-base-tui.ko",
    #"drivers/tee/teei/510/isee.ko",
    #"drivers/tee/teei/510/isee-ffa.ko",
    #"drivers/thermal/mediatek/md_cooling_all.ko",
    #"drivers/thermal/mediatek/soc_temp_lvts.ko",
    #"drivers/thermal/mediatek/thermal_interface.ko",
    #"drivers/thermal/mediatek/thermal_trace.ko",
    #"drivers/thermal/mediatek/vtskin_temp.ko",
    #"drivers/tty/serial/8250/8250_mtk.ko",
    #"sound/soc/codecs/mt6338-accdet.ko",
    #"sound/soc/codecs/mt6357-accdet.ko",
    #"sound/soc/codecs/mt6358-accdet.ko",
    #"sound/soc/codecs/mt6359p-accdet.ko",
    #"sound/soc/codecs/mt6368-accdet.ko",
    #"sound/soc/codecs/mt6681-accdet.ko",
    #"sound/soc/codecs/richtek/richtek_spm_cls.ko",
    #"sound/soc/codecs/richtek/snd-soc-rt5512.ko",
    #"sound/soc/codecs/snd-soc-mt6338.ko",
    #"sound/soc/codecs/snd-soc-mt6368.ko",
    #"sound/soc/codecs/snd-soc-mt6681.ko",
    #"sound/soc/codecs/tfa98xx/snd-soc-tfa98xx.ko",
    #"sound/soc/mediatek/audio_dsp/mtk-soc-offload-common.ko",
    #"sound/soc/mediatek/audio_dsp/snd-soc-audiodsp-common.ko",
    #"sound/soc/mediatek/common/mtk-sp-spk-amp.ko",
    #"sound/soc/mediatek/common/snd-soc-mtk-common.ko",
    #"sound/soc/codecs/snd-soc-mt6366.ko",

]

mgk_64_platform_device_modules = {
    # keep sorted
    #"drivers/clk/mediatek/clk-chk-mt6886.ko": "mt6886",
    #"drivers/clk/mediatek/clk-chk-mt6897.ko": "mt6897",
    #"drivers/clk/mediatek/clk-mt6893-apu0.ko": "mt6893",
    #"drivers/clk/mediatek/clk-mt6893-apu1.ko": "mt6893",
    #"drivers/clk/mediatek/clk-mt6893-apu2.ko": "mt6893",
    #"drivers/clk/mediatek/clk-mt6893-apuc.ko": "mt6893",
    #"drivers/clk/mediatek/clk-mt6893-apum0.ko": "mt6893",
    #"drivers/clk/mediatek/clk-mt6893-apum1.ko": "mt6893",
    #"drivers/clk/mediatek/clk-mt6893-apuv.ko": "mt6893",
    #"drivers/clk/mediatek/clk-mt6893-audsys.ko": "mt6893",
    #"drivers/clk/mediatek/clk-mt6893-cam_m.ko": "mt6893",
    #"drivers/clk/mediatek/clk-mt6893-cam_ra.ko": "mt6893",
    #"drivers/clk/mediatek/clk-mt6893-cam_rb.ko": "mt6893",
    #"drivers/clk/mediatek/clk-mt6893-cam_rc.ko": "mt6893",
    #"drivers/clk/mediatek/clk-mt6893-imgsys1.ko": "mt6893",
    #"drivers/clk/mediatek/clk-mt6893-imgsys2.ko": "mt6893",
    #"drivers/clk/mediatek/clk-mt6893-impc.ko": "mt6893",
    #"drivers/clk/mediatek/clk-mt6893-impe.ko": "mt6893",
    #"drivers/clk/mediatek/clk-mt6893-impn.ko": "mt6893",
    #"drivers/clk/mediatek/clk-mt6893-imps.ko": "mt6893",
    #"drivers/clk/mediatek/clk-mt6893-ipe.ko": "mt6893",
    #"drivers/clk/mediatek/clk-mt6893-mdp.ko": "mt6893",
    #"drivers/clk/mediatek/clk-mt6893-mfgcfg.ko": "mt6893",
    #"drivers/clk/mediatek/clk-mt6893-mm.ko": "mt6893",
    #"drivers/clk/mediatek/clk-mt6893-scp_adsp.ko": "mt6893",
    #"drivers/clk/mediatek/clk-mt6893-vde1.ko": "mt6893",
    #"drivers/clk/mediatek/clk-mt6893-vde2.ko": "mt6893",
    #"drivers/clk/mediatek/clk-mt6893-ven1.ko": "mt6893",
    #"drivers/clk/mediatek/clk-mt6893-ven2.ko": "mt6893",
    #"drivers/clk/mediatek/clk-mt6893.ko": "mt6893",
    #"drivers/clk/mediatek/clk-mt6893-pg.ko": "mt6893",
    #"drivers/clk/mediatek/clk-chk-mt6893.ko": "mt6893",
    #"drivers/clk/mediatek/clk-chk-mt6899.ko": "mt6899",
    #"drivers/clk/mediatek/clk-chk-mt6983.ko": "mt6983",
    #"drivers/clk/mediatek/clk-chk-mt6989.ko": "mt6989",
    #"drivers/clk/mediatek/clk-dbg-mt6893.ko": "mt6893",
    #"drivers/clk/mediatek/clk-dbg-mt6886.ko": "mt6886",
    #"drivers/clk/mediatek/clk-dbg-mt6897.ko": "mt6897",
    #"drivers/clk/mediatek/clk-dbg-mt6899.ko": "mt6899",
    #"drivers/clk/mediatek/clk-dbg-mt6983.ko": "mt6983",
    #"drivers/clk/mediatek/clk-dbg-mt6989.ko": "mt6989",
    #"drivers/clk/mediatek/clk-fmeter-mt6886.ko": "mt6886",
    #"drivers/clk/mediatek/clk-fmeter-mt6897.ko": "mt6897",
    #"drivers/clk/mediatek/clk-fmeter-mt6899.ko": "mt6899",
    #"drivers/clk/mediatek/clk-fmeter-mt6983.ko": "mt6983",
    #"drivers/clk/mediatek/clk-fmeter-mt6893.ko": "mt6893",
    #"drivers/clk/mediatek/clk-fmeter-mt6989.ko": "mt6989",
    #"drivers/clk/mediatek/clk-mt6886.ko": "mt6886",
    #"drivers/clk/mediatek/clk-mt6886-adsp.ko": "mt6886",
    #"drivers/clk/mediatek/clk-mt6886-bus.ko": "mt6886",
    #"drivers/clk/mediatek/clk-mt6886-cam.ko": "mt6886",
    #"drivers/clk/mediatek/clk-mt6886-ccu.ko": "mt6886",
    #"drivers/clk/mediatek/clk-mt6886-img.ko": "mt6886",
    #"drivers/clk/mediatek/clk-mt6886-mdpsys.ko": "mt6886",
    #"drivers/clk/mediatek/clk-mt6886-mmsys.ko": "mt6886",
    #"drivers/clk/mediatek/clk-mt6886-peri.ko": "mt6886",
    #"drivers/clk/mediatek/clk-mt6886-scp.ko": "mt6886",
    #"drivers/clk/mediatek/clk-mt6886-vcodec.ko": "mt6886",
    #"drivers/clk/mediatek/clk-mt6897.ko": "mt6897",
    #"drivers/clk/mediatek/clk-mt6897-adsp.ko": "mt6897",
    #"drivers/clk/mediatek/clk-mt6897-bus.ko": "mt6897",
    #"drivers/clk/mediatek/clk-mt6897-cam.ko": "mt6897",
    #"drivers/clk/mediatek/clk-mt6897-ccu.ko": "mt6897",
    #"drivers/clk/mediatek/clk-mt6897-img.ko": "mt6897",
    #"drivers/clk/mediatek/clk-mt6897-mdpsys.ko": "mt6897",
    #"drivers/clk/mediatek/clk-mt6897-mmsys.ko": "mt6897",
    #"drivers/clk/mediatek/clk-mt6897-peri.ko": "mt6897",
    #"drivers/clk/mediatek/clk-mt6897-vcodec.ko": "mt6897",
    #"drivers/clk/mediatek/clk-mt6897-vlp.ko": "mt6897",
    #"drivers/clk/mediatek/clk-mt6899.ko": "mt6899",
    #"drivers/clk/mediatek/clk-mt6899-adsp.ko": "mt6899",
    #"drivers/clk/mediatek/clk-mt6899-cam.ko": "mt6899",
    #"drivers/clk/mediatek/clk-mt6899-img.ko": "mt6899",
    #"drivers/clk/mediatek/clk-mt6899-mmsys.ko": "mt6899",
    #"drivers/clk/mediatek/clk-mt6899-peri.ko": "mt6899",
    #"drivers/clk/mediatek/clk-mt6899-infracfg_ao.ko": "mt6899",
    #"drivers/clk/mediatek/clk-mt6899-mdpsys.ko": "mt6899",
    #"drivers/clk/mediatek/clk-mt6899-vcodec.ko": "mt6899",
    #"drivers/clk/mediatek/clk-mt6983.ko": "mt6983",
    #"drivers/clk/mediatek/clk-mt6983-adsp_grp.ko": "mt6983",
    #"drivers/clk/mediatek/clk-mt6983-cam.ko": "mt6983",
    #"drivers/clk/mediatek/clk-mt6983-ccu_main.ko": "mt6983",
    #"drivers/clk/mediatek/clk-mt6983-img.ko": "mt6983",
    #"drivers/clk/mediatek/clk-mt6983-imp_iic_wrap.ko": "mt6983",
    #"drivers/clk/mediatek/clk-mt6983-mdp_grp.ko": "mt6983",
    #"drivers/clk/mediatek/clk-mt6983-mfg_top_config.ko": "mt6983",
    #"drivers/clk/mediatek/clk-mt6983-mm.ko": "mt6983",
    #"drivers/clk/mediatek/clk-mt6983-vcodec.ko": "mt6983",
    #"drivers/clk/mediatek/clk-mt6989.ko": "mt6989",
    #"drivers/clk/mediatek/clk-mt6989-adsp.ko": "mt6989",
    #"drivers/clk/mediatek/clk-mt6989-bus.ko": "mt6989",
    #"drivers/clk/mediatek/clk-mt6989-cam.ko": "mt6989",
    #"drivers/clk/mediatek/clk-mt6989-img.ko": "mt6989",
    #"drivers/clk/mediatek/clk-mt6989-mdpsys.ko": "mt6989",
    #"drivers/clk/mediatek/clk-mt6989-mmsys.ko": "mt6989",
    #"drivers/clk/mediatek/clk-mt6989-peri.ko": "mt6989",
    #"drivers/clk/mediatek/clk-mt6989-vcodec.ko": "mt6989",
    #"drivers/clk/mediatek/clk-mt6989-vlp.ko": "mt6989",
    #"drivers/clk/mediatek/clk-mt6993.ko" : "mt6993",
    #"drivers/clk/mediatek/clk-mt6993-vcodec.ko" : "mt6993",
    #"drivers/clk/mediatek/clk-mt6993-adsp.ko" : "mt6993",
    #"drivers/clk/mediatek/clk-mt6993-mmsys.ko" : "mt6993",
    #"drivers/clk/mediatek/clk-mt6993-img.ko" : "mt6993",
    #"drivers/clk/mediatek/clk-mt6993-peri.ko" : "mt6993",
    #"drivers/clk/mediatek/clk-mt6993-ifrao.ko" : "mt6993",
    #"drivers/clk/mediatek/clk-mt6993-cam.ko" : "mt6993",
    #"drivers/clk/mediatek/clk-mt6993-pd.ko" : "mt6993",
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
    #"drivers/clk/mediatek/pd-chk-mt6886.ko": "mt6886",
    #"drivers/clk/mediatek/pd-chk-mt6893.ko": "mt6893",
    #"drivers/clk/mediatek/pd-chk-mt6897.ko": "mt6897",
    #"drivers/clk/mediatek/pd-chk-mt6899.ko": "mt6899",
    #"drivers/clk/mediatek/pd-chk-mt6983.ko": "mt6983",
    #"drivers/clk/mediatek/pd-chk-mt6989.ko": "mt6989",
    #"drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6886.ko": "mt6886",
    #"drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6897.ko": "mt6897",
    #"drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6899.ko": "mt6899",
    #"drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6985.ko": "mt6985",
    #"drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6989.ko": "mt6989",
    #"drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6989_fpga.ko": "mt6989",
    #"drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6991.ko": "mt6991",
    #"drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6993.ko": "mt6993",
    #"drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6855.ko": "mt6855",
    #"drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6895.ko": "mt6895",
    # "drivers/gpu/mediatek/ged/mtk_ged_mt6991.ko": "mt6991",
    # "drivers/gpu/mediatek/ged/mtk_ged_mt6993.ko": "mt6993",
    #"drivers/gpu/mediatek/gpu_iommu/mtk_gpu_iommu_mt6989.ko": "mt6989",
    #"drivers/gpu/mediatek/gpu_iommu/mtk_gpu_iommu_mt6991.ko": "mt6991",
    #"drivers/gpu/mediatek/gpu_iommu/mtk_gpu_iommu_mt6993.ko": "mt6993",
    #"drivers/misc/mediatek/cm_mgr/mtk_cm_mgr_mt6886.ko": "mt6886",
    #"drivers/misc/mediatek/cm_mgr/mtk_cm_mgr_mt6897.ko": "mt6897",
    #"drivers/misc/mediatek/cm_mgr/mtk_cm_mgr_mt6983.ko": "mt6983",
    #"drivers/misc/mediatek/cm_mgr/mtk_cm_mgr_mt6985.ko": "mt6985",
    #"drivers/misc/mediatek/cm_mgr/mtk_cm_mgr_mt6989.ko": "mt6989",
    #"drivers/misc/mediatek/cm_mgr/mtk_cm_mgr_mt6991.ko": "mt6991",
    #"drivers/misc/mediatek/cm_mgr/mtk_cm_mgr_mt6993.ko": "mt6993",
    #"drivers/misc/mediatek/lpm/modules/debug/mt6886/mtk-lpm-dbg-mt6886.ko": "mt6886",
    #"drivers/misc/mediatek/lpm/modules/debug/mt6897/mtk-lpm-dbg-mt6897.ko": "mt6897",
    #"drivers/misc/mediatek/lpm/modules/debug/mt6983/mtk-lpm-dbg-mt6983.ko": "mt6983",
    #"drivers/misc/mediatek/lpm/modules/debug/mt6985/mtk-lpm-dbg-mt6985.ko": "mt6985",
    #"drivers/misc/mediatek/lpm/modules/debug/mt6989/mtk-lpm-dbg-mt6989.ko": "mt6989",
    #"drivers/misc/mediatek/mdp/mdp_drv_mt6761.ko": "mt6761",
    #"drivers/misc/mediatek/mdp/mdp_drv_mt6765.ko": "mt6765",
    #"drivers/misc/mediatek/mdp/mdp_drv_mt6768.ko": "mt6768",
    #"drivers/misc/mediatek/mdp/mdp_drv_mt6781.ko": "mt6781",
    #"drivers/misc/mediatek/mdp/mdp_drv_mt6789.ko": "mt6789",
    #"drivers/misc/mediatek/mdp/mdp_drv_mt6833.ko": "mt6833",
    #"drivers/misc/mediatek/mdp/mdp_drv_mt6853.ko": "mt6853",
    #"drivers/misc/mediatek/mdp/mdp_drv_mt6855.ko": "mt6855",
    #"drivers/misc/mediatek/mdp/mdp_drv_mt6877.ko": "mt6877",
    #"drivers/misc/mediatek/mdp/mdp_drv_mt6886.ko": "mt6886",
    #"drivers/misc/mediatek/mdp/mdp_drv_mt6893.ko": "mt6893",
    #"drivers/misc/mediatek/mdp/mdp_drv_mt6895.ko": "mt6895",
    #"drivers/misc/mediatek/mdp/mdp_drv_mt6897.ko": "mt6897",
    #"drivers/misc/mediatek/mdp/mdp_drv_mt6983.ko": "mt6983",
    #"drivers/misc/mediatek/mdp/mdp_drv_mt6985.ko": "mt6985",
    #"drivers/misc/mediatek/mdp/mdp_drv_mt6989.ko": "mt6989",
    #"drivers/misc/mediatek/mdp/mdp_drv_mt6991.ko": "mt6991",
    #"drivers/soc/mediatek/mtk-pm-domains.ko": "mt8188",
    #"drivers/soc/mediatek/mtk-scpsys-mt6991-spm.ko": "mt6991",
    #"drivers/soc/mediatek/mtk-scpsys-mt6991-mmpc.ko": "mt6991",
    #"drivers/misc/mediatek/vmm/mtk-vmm-notifier-mt6991.ko": "mt6991",
    #"drivers/misc/mediatek/vmm/mtk-vmm-notifier-mt6993.ko": "mt6993",
    #"sound/soc/mediatek/mt6886/mt6886-mt6368.ko": "mt6886",
    #"sound/soc/mediatek/mt6886/snd-soc-mt6886-afe.ko": "mt6886",
    #"sound/soc/mediatek/mt6897/mt6897-mt6368.ko": "mt6897",
    #"sound/soc/mediatek/mt6897/snd-soc-mt6897-afe.ko": "mt6897",
    #"sound/soc/mediatek/mt6983/mt6983-mt6338.ko": "mt6983",
    #"sound/soc/mediatek/mt6983/snd-soc-mt6983-afe.ko": "mt6983",
    #"sound/soc/mediatek/mt6985/mt6985-mt6338.ko": "mt6985",
    #"sound/soc/mediatek/mt6985/snd-soc-mt6985-afe.ko": "mt6985",
    #"sound/soc/mediatek/mt6989/mt6989-mt6681.ko": "mt6989",
    #"sound/soc/mediatek/mt6989/snd-soc-mt6989-afe.ko": "mt6989",
    #"sound/soc/mediatek/mt6991/mt6991-mt6681.ko": "mt6991",
    #"sound/soc/mediatek/mt6991/snd-soc-mt6991-afe.ko": "mt6991",
    #"sound/soc/mediatek/mt6993/mt6993-mt6681.ko": "mt6993",
    #"sound/soc/mediatek/mt6993/snd-soc-mt6993-afe.ko": "mt6993",
    "sound/soc/mediatek/mt6789/mt6789-mt6366.ko": "mt6789",
    "sound/soc/mediatek/mt6789/snd-soc-mt6789-afe.ko": "mt6789",
    "sound/soc/mediatek/mt6895/mt6895-mt6368.ko": "mt6895",
    "sound/soc/mediatek/mt6895/snd-soc-mt6895-afe.ko": "mt6895",


}


mgk_64_device_eng_modules = [
]

mgk_64_platform_device_eng_modules = {
}


mgk_64_device_userdebug_modules = [
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
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/vow/ver02:mtk-vow".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/sound/soc/mediatek/audio_dsp:mtk-soc-offload-common".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/sound/soc/mediatek/ultrasound/ultra_common:mtk-scp-ultra".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/sound/soc/mediatek/ultrasound/ultra_scp:snd-soc-mtk-scp-ultra".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/sound/soc/mediatek/vow:mtk-scp-vow".format(kernel_version))

    if "fpga.config" in DEFCONFIG_OVERLAYS:
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/fpsgo_cus:fpsgo_cus")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/fpsgo_int:fpsgo_int")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/met_drv_secure_v3:met_drv_secure_v3")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/met_drv_v3/met_api/backlight_api_headers:backlight_api_headers_int")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/met_drv_v3/met_api/met_emi_api:met_emi_api_int")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/met_drv_v3/met_api/met_gpu_adv_api:met_gpu_adv_api_int")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/met_drv_v3/met_api/met_gpu_api:met_gpu_api_int")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/met_drv_v3/met_api/met_gpueb_api:met_gpueb_api_int")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/met_drv_v3/met_api/met_ipi_api:met_ipi_api_int")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/met_drv_v3/met_api/met_mcupm_api:met_mcupm_api_int")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/met_drv_v3/met_api/met_scmi_api:met_scmi_api_int")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/met_drv_v3/met_api/met_sspm_api:met_sspm_api_int")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/met_drv_v3/met_api/met_vcore_api:met_vcore_api_int")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/met_drv_v3/met_api/backlight_api_headers:backlight_api_headers_cus")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/met_drv_v3/met_api/met_emi_api:met_emi_api_cus")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/met_drv_v3/met_api/met_gpu_adv_api:met_gpu_adv_api_cus")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/met_drv_v3/met_api/met_gpu_api:met_gpu_api_cus")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/met_drv_v3/met_api/met_gpueb_api:met_gpueb_api_cus")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/met_drv_v3/met_api/met_ipi_api:met_ipi_api_cus")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/met_drv_v3/met_api/met_mcupm_api:met_mcupm_api_cus")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/met_drv_v3/met_api/met_scmi_api:met_scmi_api_cus")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/met_drv_v3/met_api/met_sspm_api:met_sspm_api_cus")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/met_drv_v3/met_api/met_vcore_api:met_vcore_api_cus")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/met_drv_v3:met_drv_v3")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/msync2_frd_cus/build:msync2_frd_cus")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/msync2_frd_int:msync2_frd_int")
        mgk_64_kleaf_eng_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase:ktf_testcase")
        mgk_64_kleaf_userdebug_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase:ktf_testcase")
        mgk_64_device_modules.remove("drivers/misc/mediatek/performance/fpsgo_v8/mtk_fpsgo_v8.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/performance/frs/frs.ko")

        mgk_64_kleaf_modules.append("//vendor/mediatek/kernel_modules/met_drv_secure_v3:met_drv_secure_v3_default")
        mgk_64_kleaf_modules.append("//vendor/mediatek/kernel_modules/met_drv_v3:met_drv_v3_default")
        mgk_64_kleaf_eng_modules.append("//vendor/mediatek/tests/kernel/ktf_testcase:ktf_testcase_fpga")
        mgk_64_kleaf_userdebug_modules.append("//vendor/mediatek/tests/kernel/ktf_testcase:ktf_testcase_fpga")

    if "wifionly.config" in DEFCONFIG_OVERLAYS:
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/ccci_util:ccci_util_lib")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/ccmni:ccmni")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/eccci:ccci_auxadc")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/eccci:ccci_md_all")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/eccci/fsm:ccci_fsm_scp")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/eccci/hif:ccci_ccif")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/eccci/hif:ccci_cldma")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/eccci/hif:ccci_dpmaif")
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
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/ccci_util:ccci_util_lib")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/ccmni:ccmni")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/eccci:ccci_auxadc")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/eccci:ccci_md_all")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/eccci/fsm:ccci_fsm_scp")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/eccci/hif:ccci_ccif")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/eccci/hif:ccci_cldma")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/eccci/hif:ccci_dpmaif")
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

    if "entry_level.config" in DEFCONFIG_OVERLAYS:
        mgk_64_kleaf_device_modules.append("//kernel_device_modules-{}/drivers/misc/mediatek/lpm_legacy:mtk-lpm-legacy".format(kernel_version))
        mgk_64_kleaf_device_modules.append("//kernel_device_modules-{}/drivers/misc/mediatek/lpm_legacy/modules/debug:mtk-lpm-dbg-common-v1-legacy".format(kernel_version))
        mgk_64_kleaf_device_modules.append("//kernel_device_modules-{}/drivers/misc/mediatek/lpm_legacy/modules/platform/v1:mtk-lpm-plat-v1-legacy".format(kernel_version))
        mgk_64_kleaf_device_modules.append("//kernel_device_modules-{}/drivers/misc/mediatek/lpm_legacy/modules/debug/k6789:mtk-lpm-dbg-mt6789-legacy".format(kernel_version))

        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/iommu/arm/arm-smmu-v3:arm_smmu_v3".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/iommu/arm/arm-smmu-v3:mtk-smmuv3-pmu".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/iommu/arm/arm-smmu-v3:mtk-smmuv3-mpam-mon".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/iommu:mtk_smmu_qos".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/iommu:smmu_secure".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/jpeg:jpeg-driver".format(kernel_version))

        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:mtk_cm_mgr".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:mtk_cm_mgr_mt6991".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:mtk_cm_mgr_mt6993".format(kernel_version))
        mgk_64_kleaf_device_modules.append("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr_legacy_v1:mtk_cm_mgr_legacy_v1".format(kernel_version))
        mgk_64_kleaf_device_modules.append("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr_legacy_v1:mtk_cm_ipi_legacy_v1".format(kernel_version))
        mgk_64_kleaf_platform_modules.update({"//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr_legacy_v1:mtk_cm_mgr_mt6789".format(kernel_version): "mt6789"})

        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/spmi:spmi-mtk-mpu".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/spmi:spmi-mtk-pmif".format(kernel_version))

        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/mminfra:mtk-mminfra-debug".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/mminfra:mtk-mminfra-imax".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/mminfra:mtk-mminfra-util".format(kernel_version))

        #mgk_64_platform_device_modules.pop("drivers/clk/mediatek/clk-chk-mt6989.ko")
        #mgk_64_platform_device_modules.pop("drivers/clk/mediatek/pd-chk-mt6989.ko")
        #mgk_64_platform_device_modules.pop("drivers/clk/mediatek/clk-chk-mt6991.ko")
        #mgk_64_platform_device_modules.pop("drivers/clk/mediatek/pd-chk-mt6991.ko")

        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/vcp/rv:vcp".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/mbraink/bridge:mtk_mbraink_bridge".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/mbraink/perf:mtk_mbraink_perf".format(kernel_version))
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/mtk_mbraink.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/modules/v6989/mtk_mbraink_v6989.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/modules/v6991/mtk_mbraink_v6991.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/modules/v6899/mtk_mbraink_v6899.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/modules/v6993/mtk_mbraink_v6993.ko")
        mgk_64_kleaf_eng_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase/mmdvfs:ktf_mmdvfs")
        mgk_64_kleaf_userdebug_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase/mmdvfs:ktf_mmdvfs")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/soc/mediatek:mtk-mmdvfs-v3".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/mmdvfs:mtk-mmdvfs-debug-v3".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/soc/mediatek/mmdvfs:mtk-mmdvfs-v5".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/mmdvfs:mtk-mmdvfs-debug-v5".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/soc/mediatek/mmdvfs:mmdvfs-mt6993".format(kernel_version))

        mgk_64_kleaf_userdebug_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase/mml:ktf_mml_ait")
        mgk_64_kleaf_eng_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase/mml:ktf_mml_ait")
        mgk_64_kleaf_userdebug_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase/display_mobile_wbgai:ktf_display_mobile_wbgai")
        mgk_64_kleaf_userdebug_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase/display_pq_wbgai:ktf_display_pq_wbgai")
        mgk_64_kleaf_eng_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase/display_mobile_wbgai:ktf_display_mobile_wbgai")
        mgk_64_kleaf_eng_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase/display_pq_wbgai:ktf_display_pq_wbgai")

        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/mtkcam/imgsys/imgsys/isp71:imgsys_71")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/mtkcam/imgsys/imgsys/isp7s:imgsys_7s")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/mtkcam/imgsys/imgsys/isp7sp:imgsys_7sp")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/mtkcam/imgsys/imgsys/isp8:imgsys_8")
        # mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/mtkcam/imgsys/imgsys/isp8s:imgsys_8s")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/mtkcam/imgsys/imgsys/cmdq/legacy:imgsys_cmdq")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/mtkcam/imgsys/imgsys/cmdq/isp8:imgsys_cmdq_isp8")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/mtkcam/imgsys/imgsys/cmdq/isp8s:imgsys_cmdq_isp8s")

        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/gpu/drm/mediatek/dpc/dpc_v1:mtk_dpc_v1".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/gpu/drm/mediatek/dpc/dpc_v1:mtk_vdisp_v1".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/gpu/drm/mediatek/dpc/dpc_v2:mtk_dpc_v2".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/gpu/drm/mediatek/dpc/dpc_v2:mtk_vdisp_v2".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/gpu/drm/mediatek/dpc/dpc_v3:mtk_dpc_v3".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/gpu/drm/mediatek/dpc/dpc_v3:mtk_vdisp_v3".format(kernel_version))

        mgk_64_kleaf_modules.append("//vendor/mediatek/kernel_modules/gpu:gpu_mt6789")
        mgk_64_kleaf_device_modules.append("//kernel_device_modules-{}/drivers/gpu/mediatek/gpufreq/v2_legacy:mtk_gpufreq_wrapper_legacy".format(kernel_version))

        mgk_64_kleaf_device_modules.append("//kernel_device_modules-{}/drivers/misc/mediatek/conn_md:conn_md_drv".format(kernel_version))

        mgk_64_kleaf_platform_modules.update({"//kernel_device_modules-{}/drivers/gpu/mediatek/gpufreq/v2_legacy:mtk_gpufreq_mt6789".format(kernel_version):"mt6789"})

        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/gpu/mediatek/gpueb:mtk_gpueb".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/gpueb:mtk_ghpm_mt6991".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/gpueb:mtk_ghpm_mt6993".format(kernel_version))

        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/gpu/mediatek/gpufreq/v2:mtk_gpufreq_wrapper".format(kernel_version))

        #mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6886.ko")
        #mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6897.ko")
        #mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6899.ko")
        #mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6985.ko")
        #mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6989.ko")
        #mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6989_fpga.ko")
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/gpufreq/v2:mtk_gpufreq_mt6991".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/gpufreq/v2:mtk_gpufreq_mt6993".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/gpufreq/v2:mtk_gpufreq_mt6855".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/gpufreq/v2:mtk_gpufreq_mt6895".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/gpu_pdma:mtk_gpu_pdma_mt6991".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/gpu_pdma:mtk_gpu_pdma_mt6993".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//vendor/mediatek/kernel_modules/gpu/gpu_mali/mali_avalon/a16w/drivers/gpu/arm/midgard:mali_kbase_mt6991_a16w")
        mgk_64_kleaf_platform_modules.pop("//vendor/mediatek/kernel_modules/gpu/gpu_mali/mali_avalon/a16w/drivers/gpu/arm/midgard:mali_kbase_mt6993_a16w")
        mgk_64_kleaf_platform_modules.pop("//vendor/mediatek/kernel_modules/gpu/gpu_mali/mali_avalon/a16w/drivers/base/arm:mali_prot_alloc_mt6991_a16w")
        mgk_64_kleaf_platform_modules.pop("//vendor/mediatek/kernel_modules/gpu/gpu_mali/mali_avalon/a16w/drivers/base/arm:mali_prot_alloc_mt6993_a16w")
        mgk_64_kleaf_platform_modules.pop("//vendor/mediatek/kernel_modules/gpu:gpu_mt6895")

        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/memory/mediatek:emi-fake-eng-v2".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/memory/mediatek:slc-parity".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/memory/mediatek:smpu-hook-v1".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/memory/mediatek:smpu".format(kernel_version))
        mgk_64_kleaf_device_modules.append("//kernel_device_modules-{}/drivers/memory/mediatek:emi-mpu-hook-v1".format(kernel_version))
        mgk_64_kleaf_device_modules.append("//kernel_device_modules-{}/drivers/memory/mediatek:emi-mpu-v2".format(kernel_version))
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/mtkcam/sched:c2ps")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/mtkcam/sched:c2ps_perf_ioctl")

        mgk_64_common_eng_modules.remove("drivers/firmware/arm_ffa/ffa-core.ko")
        mgk_64_common_userdebug_modules.remove("drivers/firmware/arm_ffa/ffa-core.ko")
        mgk_64_common_user_modules.remove("drivers/firmware/arm_ffa/ffa-core.ko")
        mgk_64_common_eng_modules.remove("drivers/firmware/arm_ffa/ffa-module.ko")
        mgk_64_common_userdebug_modules.remove("drivers/firmware/arm_ffa/ffa-module.ko")
        mgk_64_common_user_modules.remove("drivers/firmware/arm_ffa/ffa-module.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/trusted_mem:ffa".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/tee/gud:mcDrvModule-ffa".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/tee/teei/520:isee-ffa".format(kernel_version))

        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/trusty:trusty-core".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/trusty:trusty-ipc".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/trusty:trusty-log".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/trusty:trusty-test".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/trusty:trusty-virtio".format(kernel_version))

        mgk_64_kleaf_modules.append("//kernel_device_modules-{}/drivers/misc/mediatek/swpm_legacy_v2:mtk-swpm-perf-arm-pmu-legacy".format(kernel_version))
        mgk_64_kleaf_modules.append("//kernel_device_modules-{}/drivers/misc/mediatek/swpm_legacy_v2:mtk-swpm-legacy".format(kernel_version))
        mgk_64_kleaf_modules.append("//kernel_device_modules-{}/drivers/misc/mediatek/swpm_legacy_v2/modules/debug/v1:mtk-swpm-dbg-common-v1-legacy".format(kernel_version))
        mgk_64_kleaf_modules.append("//kernel_device_modules-{}/drivers/misc/mediatek/swpm_legacy_v2/modules/debug:mtk-swpm-dbg-v6789".format(kernel_version))

    if "mt6877_overlay.config" in DEFCONFIG_OVERLAYS:
        mgk_64_kleaf_modules.append("//vendor/mediatek/kernel_modules/connectivity/wlan/core/gen4m/build/connac2x/6877:wlan_drv_gen4m_6877")
        mgk_64_kleaf_eng_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase/slbc:ktf_slbc_it")
        mgk_64_kleaf_userdebug_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase/slbc:ktf_slbc_it")
        mgk_64_kleaf_modules.append("//vendor/mediatek/kernel_modules/gpu:gpu_mt6877")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/mtkcam/sched:c2ps")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/mtkcam/sched:c2ps_perf_ioctl")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/mtkcam/isp_pspm:isp_pspm")
        mgk_64_kleaf_platform_modules.update({"//kernel_device_modules-{}/drivers/pinctrl/mediatek:pinctrl-mt6877".format(kernel_version): "mt6877"})
        mgk_64_device_modules.remove("drivers/misc/mediatek/performance/mtk_perf_ioctl_magt.ko")

        mgk_64_common_eng_modules.remove("drivers/firmware/arm_ffa/ffa-module.ko")
        mgk_64_common_userdebug_modules.remove("drivers/firmware/arm_ffa/ffa-module.ko")
        mgk_64_common_user_modules.remove("drivers/firmware/arm_ffa/ffa-module.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/eemgpu/mtk_eem.ko")
        mgk_64_device_modules.append("drivers/tee/gud/600/MobiCoreDriver/mcDrvModule.ko")
        mgk_64_device_modules.append("drivers/tee/gud/600/MobiCoreDriver/mcDrvModule-ffa.ko")
        mgk_64_kleaf_userdebug_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase/mml:ktf_mml_ait")
        mgk_64_kleaf_eng_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase/mml:ktf_mml_ait")
        mgk_64_kleaf_userdebug_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase/display_mobile_wbgai:ktf_display_mobile_wbgai")
        mgk_64_kleaf_userdebug_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase/display_pq_wbgai:ktf_display_pq_wbgai")
        mgk_64_kleaf_eng_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase/display_mobile_wbgai:ktf_display_mobile_wbgai")
        mgk_64_kleaf_eng_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase/display_pq_wbgai:ktf_display_pq_wbgai")
        mgk_64_device_modules.remove("drivers/misc/mediatek/pkvm_tmem/pkvm_tmem.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/trusted_mem:ffa".format(kernel_version))
        mgk_64_device_modules.remove("drivers/tee/teei/510/isee-ffa.ko")
        mgk_64_device_modules.append("drivers/tee/teei/515/isee.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6991/mtk-swpm-audio-dbg-v6991.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6991/mtk-swpm-dbg-v6991.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6991/mtk-swpm-disp-dbg-v6991.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6991/mtk-swpm-mml-dbg-v6991.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug/v6991/subsys:mtk-swpm-isp-dbg-v6991".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug:mtk-swpm-dbg-v6993".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug/v6993/subsys:mtk-swpm-isp-dbg-v6993".format(kernel_version))
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
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/iommu/arm/arm-smmu-v3:arm_smmu_v3".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/iommu/arm/arm-smmu-v3:mtk-smmuv3-pmu".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/iommu/arm/arm-smmu-v3:mtk-smmuv3-mpam-mon".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/iommu:mtk_smmu_qos".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/iommu:smmu_secure".format(kernel_version))
        mgk_64_device_modules.remove("drivers/tee/gud/610/TlcTui/t-base-tui.ko")

        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/gpu/mediatek/gpufreq/v2:mtk_gpufreq_wrapper".format(kernel_version))
        mgk_64_kleaf_device_modules.append("//kernel_device_modules-{}/drivers/gpu/mediatek/gpufreq/v2_legacy:mtk_gpufreq_wrapper_legacy".format(kernel_version))
        mgk_64_device_modules.append("drivers/misc/mediatek/cpuhotplug/mtk_cpuhp.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/sspm/v3/sspm_v3.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/sspm/v2/sspm.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:mtk_cm_mgr".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:mtk_cm_ipi".format(kernel_version))
        mgk_64_device_modules.append("drivers/misc/mediatek/cm_mgr_legacy_v1/mtk_cm_mgr.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/ise_lpm:ise_lpm".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/ise_lpm:ise_lpm_v2".format(kernel_version))
        mgk_64_device_modules.remove("drivers/misc/mediatek/ise_trusty/ise-trusty.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/ise_trusty/ise-trusty-ipc.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/ise_trusty/ise-trusty-log.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/ise_trusty/ise-trusty-virtio.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/mminfra:mtk-mminfra-debug".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/mminfra:mtk-mminfra-imax".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/pmsr:pmsr".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/pmsr/v2:pmsr_v2".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/pmsr/v3:pmsr_v3".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/pmsr/v4:pmsr_v4".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/slbc:mmsram".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/slbc:mtk_slbc".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/slbc:slbc_ipi".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/slbc:slbc_trace".format(kernel_version))
        mgk_64_device_modules.remove("drivers/misc/mediatek/ssc/debug/v1/mtk-ssc-dbg-v1.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/ssc/debug/v2/mtk-ssc-dbg-v2.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/ssc/mtk-ssc.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug/v1:mtk-swpm-dbg-common-v1".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug/v1/subsys:mtk-swpm-isp-wrapper".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug/v6991/subsys:mtk-swpm-audio-dbg-v6991".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug/v6991/subsys:mtk-swpm-disp-dbg-v6991".format(kernel_version))
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
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm:mtk-swpm".format(kernel_version))
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
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug/v6993/subsys:mtk-swpm-disp-dbg-v6993".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/mbraink/bridge:mtk_mbraink_bridge".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/mbraink/perf:mtk_mbraink_perf".format(kernel_version))
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/mtk_mbraink.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/modules/v6989/mtk_mbraink_v6989.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/modules/v6991/mtk_mbraink_v6991.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/modules/v6899/mtk_mbraink_v6899.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/modules/v6993/mtk_mbraink_v6993.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/vcp/rv:vcp".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/vcp/rv:vcp_status".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/vdec_fmt:vdec-fmt".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/media/platform/mtk-vcodec:mtk-vcodec-dec-v2".format(kernel_version))
        mgk_64_device_modules.remove("drivers/media/platform/mtk-vcodec/mtk-vcodec-enc-v2.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/soc/mediatek:mtk-mmdvfs-v3".format(kernel_version))
        mgk_64_kleaf_device_modules.append("//kernel_device_modules-{}/drivers/soc/mediatek/devmpu:devmpu".format(kernel_version))
        mgk_64_kleaf_device_modules.append("//kernel_device_modules-{}/drivers/misc/mediatek/adsp/v0:adsp-v0".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/adsp/v1:adsp-v1".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/adsp/v2:adsp-v2".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/adsp/v3:adsp-v3".format(kernel_version))
        mgk_64_device_modules.remove("drivers/misc/mediatek/usb/usb_offload/usb_offload.ko")
        #mgk_64_device_modules.remove("sound/soc/codecs/snd-soc-mt6338.ko")
        #mgk_64_device_modules.remove("sound/soc/codecs/snd-soc-mt6368.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/sound/soc/codecs:snd-soc-mt6368".format(kernel_version))
        #mgk_64_device_modules.remove("sound/soc/codecs/snd-soc-mt6681.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/sound/soc/codecs:snd-soc-mt6681".format(kernel_version))

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
        #mgk_64_platform_device_modules.update({"drivers/misc/mediatek/cm_mgr_legacy_v1/mtk_cm_mgr_mt6877.ko":"mt6877"})
        mgk_64_kleaf_platform_modules.update({"//kernel_device_modules-{}/drivers/misc/mediatek/dcm:mt6877_dcm".format(kernel_version):"mt6877"})
        #mgk_64_platform_device_modules.pop("drivers/misc/mediatek/lpm/modules/debug/mt6989/mtk-lpm-dbg-mt6989.ko")
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/lpm/modules/debug/mt6991:mtk-lpm-dbg-mt6991".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:mtk_cm_mgr_mt6989".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:mtk_cm_mgr_mt6886".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:mtk_cm_mgr_mt6897".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:mtk_cm_mgr_mt6983".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:mtk_cm_mgr_mt6985".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:mtk_cm_mgr_mt6991".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:mtk_cm_mgr_mt6993".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/slbc:slbc_mt6895".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/slbc:slbc_mt6991".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/slbc:slbc_mt6993".format(kernel_version))
        #mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6886.ko")
        #mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6897.ko")
        #mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6899.ko")
        #mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6985.ko")
        #mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6989.ko")
        #mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6989_fpga.ko")
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/gpufreq/v2:mtk_gpufreq_mt6991".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/gpufreq/v2:mtk_gpufreq_mt6993".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/gpufreq/v2:mtk_gpufreq_mt6855".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/gpufreq/v2:mtk_gpufreq_mt6895".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/ged:mtk_ged_mt6991".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/ged:mtk_ged_mt6993".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/gpu_pdma:mtk_gpu_pdma_mt6991".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/gpu_pdma:mtk_gpu_pdma_mt6993".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//vendor/mediatek/kernel_modules/gpu/gpu_mali/mali_avalon/a16w/drivers/gpu/arm/midgard:mali_kbase_mt6991_a16w")
        mgk_64_kleaf_platform_modules.pop("//vendor/mediatek/kernel_modules/gpu/gpu_mali/mali_avalon/a16w/drivers/gpu/arm/midgard:mali_kbase_mt6993_a16w")
        mgk_64_kleaf_platform_modules.pop("//vendor/mediatek/kernel_modules/gpu/gpu_mali/mali_avalon/a16w/drivers/base/arm:mali_prot_alloc_mt6991_a16w")
        mgk_64_kleaf_platform_modules.pop("//vendor/mediatek/kernel_modules/gpu/gpu_mali/mali_avalon/a16w/drivers/base/arm:mali_prot_alloc_mt6993_a16w")
        mgk_64_kleaf_platform_modules.pop("//vendor/mediatek/kernel_modules/gpu:gpu_mt6895")

        #mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6886/mt6886-mt6368.ko")
        #mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6886/snd-soc-mt6886-afe.ko")
        #mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6897/mt6897-mt6368.ko")
        #mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6897/snd-soc-mt6897-afe.ko")
        #mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6983/mt6983-mt6338.ko")
        #mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6983/snd-soc-mt6983-afe.ko")
        #mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6985/mt6985-mt6338.ko")
        #mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6985/snd-soc-mt6985-afe.ko")
        #mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6989/mt6989-mt6681.ko")
        #mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6989/snd-soc-mt6989-afe.ko")
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/sound/soc/mediatek/mt6991:mt6991-mt6681".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/sound/soc/mediatek/mt6991:snd-soc-mt6991-afe".format(kernel_version))
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

        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/gpu/mediatek/gpueb:mtk_gpueb".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/gpueb:mtk_ghpm_mt6991".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/gpueb:mtk_ghpm_mt6993".format(kernel_version))

        mgk_64_device_modules.remove("drivers/misc/mediatek/imgsensor/src/isp6s/imgsensor_isp6s.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/imgsensor/src/isp6s_mon/imgsensor_isp6s_mon.ko")
        # mgk_64_device_modules.remove("drivers/misc/mediatek/cam_cal/src/custom/camera_eeprom.ko")
        # mgk_64_device_modules.append("drivers/misc/mediatek/cam_cal/src/isp6s_mon/camera_eeprom_isp6s_mon.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/cam_cal/src:camera_eeprom".format(kernel_version))
        mgk_64_kleaf_device_modules.append("//kernel_device_modules-{}/drivers/misc/mediatek/cam_cal/src:camera_eeprom_isp6s_mon".format(kernel_version))
        mgk_64_device_modules.append("drivers/misc/mediatek/cameraisp/wpe/isp_6s/camera_wpe_isp6s.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cameraisp/dpe/camera_dpe_isp60.ko")

    if "mt6781_overlay.config" in DEFCONFIG_OVERLAYS:
        mgk_64_kleaf_modules.append("//vendor/mediatek/kernel_modules/connectivity/wlan/core/gen4m/build/connac1x/6781:wlan_drv_gen4m_6781")
        mgk_64_kleaf_modules.append("//vendor/mediatek/kernel_modules/gpu:gpu_mt6781")
        mgk_64_platform_device_modules.update({"drivers/gpu/mediatek/gpufreq/v2_legacy/mtk_gpufreq_mt6781.ko":"mt6781"})
        #mgk_64_device_modules.append("drivers/gpu/mediatek/gpufreq/v2_legacy/mtk_gpufreq_mt6781.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/char/rpmb:rpmb".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/char/rpmb:rpmb-mtk".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/adsp:adsp".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/adsp/v1:adsp-v1".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/adsp/v2:adsp-v2".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/adsp/v3:adsp-v3".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/audio_ipi:audio_ipi".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/sound/soc/mediatek/audio_dsp:mtk-soc-offload-common".format(kernel_version))
        mgk_64_device_modules.remove("drivers/misc/mediatek/usb/usb_offload/usb_offload.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/sound/soc/mediatek/ultrasound/ultra_common:mtk-scp-ultra".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/sound/soc/mediatek/ultrasound/ultra_scp:snd-soc-mtk-scp-ultra".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/sound/soc/mediatek/audio_dsp:snd-soc-audiodsp-common".format(kernel_version),)
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/vow/ver02:mtk-vow".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/sound/soc/mediatek/vow:mtk-scp-vow".format(kernel_version))
        #mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6985/mt6985-mt6338.ko")
        #mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6985/snd-soc-mt6985-afe.ko")
        mgk_64_device_modules.append("sound/soc/codecs/snd-soc-mt6660.ko")
        mgk_64_kleaf_device_modules.append("//kernel_device_modules-{}/sound/soc/codecs:snd-soc-mt6366".format(kernel_version))
        #mgk_64_platform_device_modules.update({"sound/soc/mediatek/mt6781/mt6781-mt6366.ko":"mt6781"})
        #mgk_64_platform_device_modules.update({"sound/soc/mediatek/mt6781/snd-soc-mt6781-afe.ko":"mt6781"})
        mgk_64_platform_device_modules.update({"drivers/interconnect/mediatek/mmqos-mt6781.ko":"mt6781"})
        mgk_64_kleaf_platform_modules.update({"//kernel_device_modules-{}/drivers/pinctrl/mediatek:pinctrl-mt6781".format(kernel_version): "mt6781"})
        mgk_64_device_modules.remove("drivers/misc/mediatek/pkvm_tmem/pkvm_tmem.ko")
        mgk_64_common_eng_modules.remove("drivers/firmware/arm_ffa/ffa-module.ko")
        mgk_64_common_userdebug_modules.remove("drivers/firmware/arm_ffa/ffa-module.ko")
        mgk_64_common_user_modules.remove("drivers/firmware/arm_ffa/ffa-module.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/iommu/arm/arm-smmu-v3:arm_smmu_v3".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/iommu/arm/arm-smmu-v3:mtk-smmuv3-pmu".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/iommu/arm/arm-smmu-v3:mtk-smmuv3-mpam-mon".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/iommu:smmu_secure".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/iommu:mtk_smmu_qos".format(kernel_version))
        mgk_64_device_modules.append("drivers/clk/mediatek/clk-mt6781.ko")
        mgk_64_device_modules.append("drivers/clk/mediatek/clkdbg-mt6781.ko")
        mgk_64_device_modules.append("drivers/clk/mediatek/clk-chk-mt6781.ko")
        mgk_64_device_modules.append("drivers/clk/mediatek/clk-mt6781-pg.ko")
        mgk_64_kleaf_device_modules.append("//kernel_device_modules-{}/drivers/soc/mediatek/devmpu:devmpu".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/memory/mediatek:emi".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/memory/mediatek:emi-fake-eng".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/memory/mediatek:emi-fake-eng-v2".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/memory/mediatek:emi-mpu".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/memory/mediatek:emi-mpu-test".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/memory/mediatek:emi-mpu-test-v2".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/memory/mediatek:emi-slb".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/memory/mediatek:smpu".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/memory/mediatek:smpu-hook-v1".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/memory/mediatek:slc-parity".format(kernel_version))
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi_legacy_v2/emi.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi_legacy_v2/emi-mpu.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi_legacy_v2/emi-mpu-test.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi-dummy.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi_legacy_v2/emi-mpu-hook-v1.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi_legacy_v2/emi-slb.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi_legacy_v2/emi-mpu-test-v2.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi_legacy_v2/emi-fake-eng.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi_legacy_v2/emi-mpu-v2.ko")
        mgk_64_kleaf_device_modules.append("//kernel_device_modules-{}/drivers/gpu/mediatek/gpufreq/v2_legacy:mtk_gpufreq_wrapper_legacy".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/gpu/mediatek/gpueb:mtk_gpueb".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/gpueb:mtk_ghpm_mt6991".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/gpueb:mtk_ghpm_mt6993".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/gpu/mediatek/gpufreq/v2:mtk_gpufreq_wrapper".format(kernel_version))
        #mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6886.ko")
        #mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6897.ko")
        #mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6899.ko")
        #mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6985.ko")
        #mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6989.ko")
        #mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6989_fpga.ko")
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/gpufreq/v2:mtk_gpufreq_mt6991".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/gpufreq/v2:mtk_gpufreq_mt6993".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/gpufreq/v2:mtk_gpufreq_mt6855".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/gpufreq/v2:mtk_gpufreq_mt6895".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/ged:mtk_ged_mt6991".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/ged:mtk_ged_mt6993".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/gpu_pdma:mtk_gpu_pdma_mt6991".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/gpu_pdma:mtk_gpu_pdma_mt6993".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//vendor/mediatek/kernel_modules/gpu/gpu_mali/mali_avalon/a16w/drivers/gpu/arm/midgard:mali_kbase_mt6991_a16w")
        mgk_64_kleaf_platform_modules.pop("//vendor/mediatek/kernel_modules/gpu/gpu_mali/mali_avalon/a16w/drivers/gpu/arm/midgard:mali_kbase_mt6993_a16w")
        mgk_64_kleaf_platform_modules.pop("//vendor/mediatek/kernel_modules/gpu/gpu_mali/mali_avalon/a16w/drivers/base/arm:mali_prot_alloc_mt6991_a16w")
        mgk_64_kleaf_platform_modules.pop("//vendor/mediatek/kernel_modules/gpu/gpu_mali/mali_avalon/a16w/drivers/base/arm:mali_prot_alloc_mt6993_a16w")
        mgk_64_kleaf_platform_modules.pop("//vendor/mediatek/kernel_modules/gpu:gpu_mt6895")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/vcp/rv:vcp".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/vcp/rv:vcp_status".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/media/platform/mtk-vcodec:mtk-vcodec-dec-v2".format(kernel_version))
        mgk_64_device_modules.remove("drivers/media/platform/mtk-vcodec/mtk-vcodec-enc-v2.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/soc/mediatek:mtk-mmdvfs-v3".format(kernel_version))
        mgk_64_platform_device_modules.pop("drivers/clk/mediatek/clk-chk-mt6989.ko")
        mgk_64_platform_device_modules.pop("drivers/clk/mediatek/pd-chk-mt6989.ko")
        mgk_64_platform_device_modules.pop("drivers/clk/mediatek/clk-chk-mt6991.ko")
        mgk_64_platform_device_modules.pop("drivers/clk/mediatek/pd-chk-mt6991.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/performance/mtk_perf_ioctl_magt.ko")

        mgk_64_device_modules.append("drivers/misc/mediatek/scp/cm4/scp.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/sound/soc/codecs:mt6681-accdet".format(kernel_version))
        mgk_64_device_modules.remove("drivers/misc/mediatek/apusys/apusys.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/apusys/apu_aov.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/sensor/2.0/sensorhub/sensorhub.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/gpu/drm/mediatek/mediatek_v2:mtk_aod_scp".format(kernel_version))
        mgk_64_device_modules.remove("drivers/misc/mediatek/apusys/power/apu_top.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/apusys/sapu/sapu.ko")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/mtkcam/scpsys/mtk-aov:mtk_aov")

        mgk_64_kleaf_userdebug_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase/mml:ktf_mml_ait")
        mgk_64_kleaf_eng_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase/mml:ktf_mml_ait")
        mgk_64_kleaf_userdebug_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase/display_mobile_wbgai:ktf_display_mobile_wbgai")
        mgk_64_kleaf_userdebug_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase/display_pq_wbgai:ktf_display_pq_wbgai")
        mgk_64_kleaf_eng_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase/display_mobile_wbgai:ktf_display_mobile_wbgai")
        mgk_64_kleaf_eng_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase/display_pq_wbgai:ktf_display_pq_wbgai")

        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/trusted_mem:ffa".format(kernel_version))
        mgk_64_device_modules.append("drivers/tee/gud/600/MobiCoreDriver/mcDrvModule.ko")
        mgk_64_device_modules.append("drivers/tee/gud/600/MobiCoreDriver/mcDrvModule-ffa.ko")
        mgk_64_device_modules.append("drivers/tee/gud/600/TlcTui/t-base-tui.ko")
        mgk_64_device_modules.remove("drivers/tee/teei/510/isee-ffa.ko")
        mgk_64_device_modules.append("drivers/tee/teei/515/isee.ko")

        mgk_64_device_modules.append("drivers/misc/mediatek/eem_v2/mediatek_eem.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cpuhotplug/mtk_cpuhp.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cpufreq_v2/src/CPU_DVFS.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/upower/Upower.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/leakage_table_v2/mediatek_static_power.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/ppm_v3/mtk_ppm_v3.ko")

        mgk_64_kleaf_device_modules.append("//kernel_device_modules-{}/drivers/misc/mediatek/conn_md:conn_md_drv".format(kernel_version))

    if "mt6768_overlay.config" in DEFCONFIG_OVERLAYS:
        mgk_64_kleaf_modules.append("//vendor/mediatek/kernel_modules/connectivity/wlan/core/gen4m/build/connac1x/6768:wlan_drv_gen4m_6768")
        mgk_64_kleaf_eng_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase/slbc:ktf_slbc_it")
        mgk_64_kleaf_userdebug_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase/slbc:ktf_slbc_it")
        mgk_64_kleaf_modules.append("//vendor/mediatek/kernel_modules/gpu:gpu_mt6768")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/mtkcam/mtk-aie:mtk-aie")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/mtkcam/scpsys/mtk-aov:mtk_aov")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/mtkcam/mtk-hcp/legacy:mtk-hcp")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/mtkcam/mtk-hcp/isp8:mtk-hcp-isp8")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/mtkcam/mtk-hcp/isp8s:mtk-hcp-isp8s")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/mtkcam/imgsys/imgsys/isp71:imgsys_71")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/mtkcam/imgsys/imgsys/isp7s:imgsys_7s")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/mtkcam/imgsys/imgsys/isp7sp:imgsys_7sp")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/mtkcam/imgsys/imgsys/isp8:imgsys_8")
        # mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/mtkcam/imgsys/imgsys/isp8s:imgsys_8s")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/mtkcam/imgsys/imgsys/cmdq/legacy:imgsys_cmdq")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/mtkcam/imgsys/imgsys/cmdq/isp8:imgsys_cmdq_isp8")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/mtkcam/imgsys/imgsys/cmdq/isp8s:imgsys_cmdq_isp8s")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/mtkcam/sched:c2ps")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/mtkcam/sched:c2ps_perf_ioctl")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/mtkcam/isp_pspm:isp_pspm")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/performance:mtk_perf_ioctl_magt".format(kernel_version))
        mgk_64_common_eng_modules.remove("drivers/firmware/arm_ffa/ffa-module.ko")
        mgk_64_common_userdebug_modules.remove("drivers/firmware/arm_ffa/ffa-module.ko")
        mgk_64_common_user_modules.remove("drivers/firmware/arm_ffa/ffa-module.ko")
        mgk_64_common_eng_modules.remove("drivers/firmware/arm_ffa/ffa-core.ko")
        mgk_64_common_userdebug_modules.remove("drivers/firmware/arm_ffa/ffa-core.ko")
        mgk_64_common_user_modules.remove("drivers/firmware/arm_ffa/ffa-core.ko")
        #mgk_64_device_modules.remove("drivers/misc/mediatek/pkvm_tmem/pkvm_tmem.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/tee/gud:mcDrvModule".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/tee/gud:mcDrvModule-ffa".format(kernel_version))
        mgk_64_device_modules.append("drivers/tee/gud/600/MobiCoreDriver/mcDrvModule.ko")
        mgk_64_device_modules.append("drivers/tee/gud/600/MobiCoreDriver/mcDrvModule-ffa.ko")
        mgk_64_device_modules.append("drivers/tee/gud/600/TlcTui/t-base-tui.ko")

        mgk_64_device_modules.append("drivers/misc/mediatek/qos/mtk_qos_legacy.ko")

        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/gpu/drm/mediatek/mediatek_v2:mtk_aod_scp".format(kernel_version))
        mgk_64_device_modules.remove("drivers/gpu/drm/panel/mediatek-cust-panel-sample.ko")
        mgk_64_device_modules.remove("drivers/gpu/drm/panel/mediatek-drm-gateic.ko")
        mgk_64_device_modules.remove("drivers/gpu/drm/panel/mediatek-drm-panel-drv.ko")

        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/iommu/arm/arm-smmu-v3:arm_smmu_v3".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/iommu/arm/arm-smmu-v3:mtk-smmuv3-pmu".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/iommu/arm/arm-smmu-v3:mtk-smmuv3-mpam-mon".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/iommu:mtk_smmu_qos".format(kernel_version))

        #mgk_64_device_modules.remove("drivers/media/platform/mtk-aie/mtk_aie.ko")
        #mgk_64_device_modules.remove("drivers/media/platform/mtk-isp/mtk-aov/mtk_aov.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/media/platform/mtk-vcodec:mtk-vcodec-dec-v2".format(kernel_version))
        mgk_64_device_modules.remove("drivers/media/platform/mtk-vcodec/mtk-vcodec-enc-v2.ko")

        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/memory/mediatek:emi".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/memory/mediatek:emi-fake-eng".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/memory/mediatek:emi-fake-eng-v2".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/memory/mediatek:emi-mpu".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/memory/mediatek:emi-mpu-test".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/memory/mediatek:emi-mpu-test-v2".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/memory/mediatek:emi-slb".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/memory/mediatek:smpu".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/memory/mediatek:smpu-hook-v1".format(kernel_version))
        mgk_64_kleaf_device_modules.append("//kernel_device_modules-{}/drivers/memory/mediatek:emicen".format(kernel_version))
        mgk_64_kleaf_device_modules.append("//kernel_device_modules-{}/drivers/memory/mediatek:emiisu".format(kernel_version))
        mgk_64_kleaf_device_modules.append("//kernel_device_modules-{}/drivers/memory/mediatek:emimpu".format(kernel_version))
        mgk_64_kleaf_device_modules.append("//kernel_device_modules-{}/drivers/memory/mediatek:emictrl".format(kernel_version))
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi-dummy.ko")

        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/adsp:adsp".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/adsp/v1:adsp-v1".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/adsp/v2:adsp-v2".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/adsp/v3:adsp-v3".format(kernel_version))

        mgk_64_device_modules.remove("drivers/misc/mediatek/apusys/apusys.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/apusys/apu_aov.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/apusys/power/apu_top.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/apusys/sapu/sapu.ko")

        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/audio_ipi:audio_ipi".format(kernel_version))

        mgk_64_device_modules.append("drivers/misc/mediatek/cameraisp/src/isp_4/camera_isp_4_t.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cameraisp/fdvt/camera_fdvt_isp40.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cameraisp/dpe/camera_dpe_isp40.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cameraisp/src/isp_4/cam_qos_4.ko")

        mgk_64_device_modules.append("drivers/misc/mediatek/cmdq/bridge/cmdq-bdg-mailbox.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cmdq/bridge/cmdq-bdg-test.ko")

        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:mtk_cm_mgr".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:mtk_cm_ipi".format(kernel_version))
        mgk_64_device_modules.append("drivers/misc/mediatek/cm_mgr_legacy_v1/mtk_cm_mgr.ko")

        mgk_64_device_modules.append("drivers/misc/mediatek/cpufreq_v2/src/CPU_DVFS.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cpuhotplug/mtk_cpuhp.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cpuidle/mtk_cpuidle.ko")

        mgk_64_kleaf_device_modules.append("//kernel_device_modules-{}/drivers/misc/mediatek/dcm:mt6768_dcm".format(kernel_version))

        mgk_64_kleaf_device_modules.append("//kernel_device_modules-{}/drivers/misc/mediatek/dvfsrc:dvfsrc-opp-mt6768".format(kernel_version))

        mgk_64_kleaf_device_modules.append("//kernel_device_modules-{}/drivers/misc/mediatek/conn_md:conn_md_drv".format(kernel_version))

        mgk_64_device_modules.append("drivers/misc/mediatek/eem_v2/mediatek_eem.ko")

        #mgk_64_device_modules.remove("drivers/misc/mediatek/imgsensor/src/isp6s/imgsensor_isp6s.ko")
        #mgk_64_device_modules.append("drivers/misc/mediatek/imgsensor/src/isp4_t/imgsensor_isp4_t.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/imgsensor/src:imgsensor_isp6s".format(kernel_version))
        mgk_64_kleaf_device_modules.append("//kernel_device_modules-{}/drivers/misc/mediatek/imgsensor/src:imgsensor_isp4_t".format(kernel_version))
        # mgk_64_device_modules.remove("drivers/misc/mediatek/cam_cal/src/custom/camera_eeprom.ko")
        # mgk_64_device_modules.append("drivers/misc/mediatek/cam_cal/src/isp4_t/camera_eeprom_isp4_t.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/cam_cal/src:camera_eeprom".format(kernel_version))
        mgk_64_kleaf_device_modules.append("//kernel_device_modules-{}/drivers/misc/mediatek/cam_cal/src:camera_eeprom_isp4_t".format(kernel_version))

        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/iommu:smmu_secure".format(kernel_version))

        mgk_64_device_modules.append("drivers/misc/mediatek/leakage_table_v2/mediatek_static_power.ko")

        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/mbraink/bridge:mtk_mbraink_bridge".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/mbraink/perf:mtk_mbraink_perf".format(kernel_version))
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/mtk_mbraink.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/modules/v6989/mtk_mbraink_v6989.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/modules/v6991/mtk_mbraink_v6991.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/modules/v6899/mtk_mbraink_v6899.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/modules/v6993/mtk_mbraink_v6993.ko")

        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/mcupm/v3:mcupm".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/mcupm/v2:mcupm".format(kernel_version))

        mgk_64_device_modules.append("drivers/misc/mediatek/mcdi/mcdi.ko")

        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/mminfra:mtk-mminfra-debug".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/mminfra:mtk-mminfra-imax".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/mminfra:mtk-mminfra-util".format(kernel_version))

        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/pmsr:pmsr".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/pmsr/v2:pmsr_v2".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/pmsr/v3:pmsr_v3".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/pmsr/v4:pmsr_v4".format(kernel_version))

        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/soc/mediatek/devapc:device-apc-common".format(kernel_version))

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

        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/sspm/v3:sspm_v3".format(kernel_version))
        mgk_64_kleaf_device_modules.append("//kernel_device_modules-{}/drivers/misc/mediatek/sspm/v1:sspm_v1".format(kernel_version))

        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug/v1:mtk-swpm-dbg-common-v1".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug/v1/subsys:mtk-swpm-isp-wrapper".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug/v6991/subsys:mtk-swpm-audio-dbg-v6991".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug/v6991/subsys:mtk-swpm-disp-dbg-v6991".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug/v6991/subsys:mtk-swpm-mml-dbg-v6991".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug/v6991/subsys:mtk-swpm-mml-dbg-v6993".format(kernel_version))
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
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug:mtk-swpm-dbg-v6991".format(kernel_version))
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6991/mtk-swpm-disp-dbg-v6991.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6991/mtk-swpm-mml-dbg-v6991.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug/v6991/subsys:mtk-swpm-isp-dbg-v6991".format(kernel_version))
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6991/mtk-swpm-audio-dbg-v6991.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug:mtk-swpm-dbg-v6993".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug/v6993/subsys:mtk-swpm-isp-dbg-v6993".format(kernel_version))

        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm:mtk-swpm".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm:mtk-swpm-perf-arm-pmu".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/tinysys_scmi:tinysys-scmi".format(kernel_version))

        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/trusted_mem:ffa".format(kernel_version))

        mgk_64_device_modules.append("drivers/misc/mediatek/upower/Upower.ko")

        mgk_64_device_modules.remove("drivers/misc/mediatek/usb/usb_offload/usb_offload.ko")

        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/vcp/rv:vcp".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/vcp/rv:vcp_status".format(kernel_version))

        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/vdec_fmt:vdec-fmt".format(kernel_version))

        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/vow/ver02:mtk-vow".format(kernel_version))

        mgk_64_device_modules.append("drivers/power/supply/bq2589x_charger.ko")

        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/spmi:spmi-mtk-mpu".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/spmi:spmi-mtk-pmif".format(kernel_version))

        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/soc/mediatek:mtk-mmdvfs-v3".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/mmdvfs:mtk-mmdvfs-debug-v3".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/soc/mediatek/mmdvfs:mtk-mmdvfs-v5".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/mmdvfs:mtk-mmdvfs-debug-v5".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/soc/mediatek/mmdvfs:mmdvfs-mt6993".format(kernel_version))

        mgk_64_device_modules.remove("drivers/tee/teei/510/isee-ffa.ko")
        mgk_64_device_modules.append("drivers/tee/teei/515/isee.ko")

        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/thermal/mediatek:backlight_cooling".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/thermal/mediatek:board_temp".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/thermal/mediatek:charger_cooling".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/thermal/mediatek:wifi_cooling".format(kernel_version))
        if "drivers/thermal/mediatek/md_cooling_all.ko" in mgk_64_device_modules:
            mgk_64_device_modules.remove("drivers/thermal/mediatek/md_cooling_all.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/thermal/mediatek:pmic_temp".format(kernel_version))
        mgk_64_device_modules.remove("drivers/thermal/mediatek/soc_temp_lvts.ko")
        mgk_64_device_modules.remove("drivers/thermal/mediatek/thermal_interface.ko")
        #mgk_64_device_modules.remove("drivers/thermal/mediatek/thermal_jatm.ko")
        mgk_64_device_modules.remove("drivers/thermal/mediatek/thermal_trace.ko")
        mgk_64_device_modules.remove("drivers/thermal/mediatek/vtskin_temp.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/trusty:trusty-core".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/trusty:trusty-ipc".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/trusty:trusty-log".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/trusty:trusty-test".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/trusty:trusty-virtio".format(kernel_version))
        #mgk_64_device_modules.remove("sound/soc/codecs/mt6338-accdet.ko")
        #mgk_64_device_modules.remove("sound/soc/codecs/snd-soc-mt6338.ko")
        #mgk_64_device_modules.remove("sound/soc/codecs/snd-soc-mt6368.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/sound/soc/mediatek/audio_dsp:mtk-soc-offload-common".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/sound/soc/mediatek/audio_dsp:snd-soc-audiodsp-common".format(kernel_version),)
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/sound/soc/mediatek/ultrasound/ultra_common:mtk-scp-ultra".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/sound/soc/mediatek/ultrasound/ultra_scp:snd-soc-mtk-scp-ultra".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/sound/soc/mediatek/vow:mtk-scp-vow".format(kernel_version))
        mgk_64_device_modules.append("sound/soc/codecs/snd-soc-mt6660.ko")
        mgk_64_device_modules.append("sound/soc/codecs/fs18xx/snd-soc-fsm.ko")
        mgk_64_device_modules.append("sound/soc/codecs/snd-soc-mt6358.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/spi_slave_drv/spi_slave.ko")
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/gpu_pdma:mtk_gpu_pdma_mt6991".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/gpu_pdma:mtk_gpu_pdma_mt6993".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//vendor/mediatek/kernel_modules/gpu/gpu_mali/mali_avalon/a16w/drivers/gpu/arm/midgard:mali_kbase_mt6991_a16w")
        mgk_64_kleaf_platform_modules.pop("//vendor/mediatek/kernel_modules/gpu/gpu_mali/mali_avalon/a16w/drivers/gpu/arm/midgard:mali_kbase_mt6993_a16w")
        mgk_64_kleaf_platform_modules.pop("//vendor/mediatek/kernel_modules/gpu/gpu_mali/mali_avalon/a16w/drivers/base/arm:mali_prot_alloc_mt6991_a16w")
        mgk_64_kleaf_platform_modules.pop("//vendor/mediatek/kernel_modules/gpu/gpu_mali/mali_avalon/a16w/drivers/base/arm:mali_prot_alloc_mt6993_a16w")
        mgk_64_kleaf_platform_modules.pop("//vendor/mediatek/kernel_modules/gpu:gpu_mt6895")

        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/gpu/mediatek/gpueb:mtk_gpueb".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/gpueb:mtk_ghpm_mt6991".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/gpueb:mtk_ghpm_mt6993".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/gpu/mediatek/gpufreq/v2:mtk_gpufreq_wrapper".format(kernel_version))
        mgk_64_kleaf_device_modules.append("//kernel_device_modules-{}/drivers/gpu/mediatek/gpufreq/v2_legacy:mtk_gpufreq_wrapper_legacy".format(kernel_version))

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

        mgk_64_kleaf_userdebug_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase/mml:ktf_mml_ait")
        mgk_64_kleaf_eng_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase/mml:ktf_mml_ait")
        mgk_64_kleaf_userdebug_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase/display_mobile_wbgai:ktf_display_mobile_wbgai")
        mgk_64_kleaf_userdebug_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase/display_pq_wbgai:ktf_display_pq_wbgai")
        mgk_64_kleaf_eng_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase/display_mobile_wbgai:ktf_display_mobile_wbgai")
        mgk_64_kleaf_eng_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase/display_pq_wbgai:ktf_display_pq_wbgai")

        mgk_64_platform_device_modules.update({"drivers/gpu/mediatek/gpufreq/v2_legacy/mtk_gpufreq_mt6768.ko":"mt6768"})

        mgk_64_platform_device_modules.update({"drivers/interconnect/mediatek/mmqos-mt6768.ko":"mt6768"})

        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:mtk_cm_mgr_mt6886".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:mtk_cm_mgr_mt6897".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:mtk_cm_mgr_mt6983".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:mtk_cm_mgr_mt6985".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:mtk_cm_mgr_mt6991".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:mtk_cm_mgr_mt6993".format(kernel_version))
        #mgk_64_platform_device_modules.update({"drivers/misc/mediatek/cm_mgr_legacy_v1/mtk_cm_mgr_mt6768.ko":"mt6768"})
        #mgk_64_platform_device_modules.update({"drivers/misc/mediatek/cm_mgr_legacy_v1/mtk_cm_mgr_mt6893.ko":"mt6893"})

        mgk_64_kleaf_platform_modules.update({"//kernel_device_modules-{}/drivers/pinctrl/mediatek:pinctrl-mt6768".format(kernel_version): "mt6768"})

        #mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6886/mt6886-mt6368.ko")
        #mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6886/snd-soc-mt6886-afe.ko")
        #mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6897/mt6897-mt6368.ko")
        #mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6897/snd-soc-mt6897-afe.ko")
        #mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6983/mt6983-mt6338.ko")
        #mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6983/snd-soc-mt6983-afe.ko")
        #mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6985/mt6985-mt6338.ko")
        #mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6985/snd-soc-mt6985-afe.ko")
        #mgk_64_platform_device_modules.update({"sound/soc/mediatek/mt6768/mt6768-mt6358.ko":"mt6768"})
        #mgk_64_platform_device_modules.update({"sound/soc/mediatek/mt6768/snd-soc-mt6768-afe.ko":"mt6768"})

        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/memory/mediatek:slc-parity".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/ise_lpm:ise_lpm".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/ise_lpm:ise_lpm_v2".format(kernel_version))
        mgk_64_device_modules.remove("drivers/misc/mediatek/ise_trusty/ise-trusty.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/ise_trusty/ise-trusty-ipc.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/ise_trusty/ise-trusty-log.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/ise_trusty/ise-trusty-virtio.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/lpm/governors:lpm-gov-MHSP".format(kernel_version))
        #mgk_64_device_modules.remove("drivers/misc/mediatek/lpm/modules/debug/v1/mtk-lpm-dbg-common-v1.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/lpm/modules/debug:mtk-lpm-dbg-common-v2".format(kernel_version))
        #mgk_64_device_modules.remove("drivers/misc/mediatek/lpm/modules/platform/v1/mtk-lpm-plat-v1.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/lpm/modules/platform/v2:mtk-lpm-plat-v2".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/lpm:mtk-lpm".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/mdpm:mtk_mdpm".format(kernel_version))
        mgk_64_device_modules.append("drivers/misc/mediatek/mdpm_v1/mtk_mdpm_v1.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/slbc:mmsram".format(kernel_version))
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
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug/v6993/subsys:mtk-swpm-disp-dbg-v6993".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/sound/soc/codecs:mt6681-accdet".format(kernel_version))
        mgk_64_device_modules.remove("drivers/misc/mediatek/usb/usb_sram/usb_sram.ko")

        #mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6886.ko")
        #mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6897.ko")
        #mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6899.ko")
        #mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6985.ko")
        #mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6989.ko")
        #mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6989_fpga.ko")
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/gpufreq/v2:mtk_gpufreq_mt6991".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/gpufreq/v2:mtk_gpufreq_mt6993".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/gpufreq/v2:mtk_gpufreq_mt6855".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/gpufreq/v2:mtk_gpufreq_mt6895".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/ged:mtk_ged_mt6991".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/ged:mtk_ged_mt6993".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:mtk_cm_mgr_mt6989".format(kernel_version))
        #mgk_64_platform_device_modules.pop("drivers/misc/mediatek/lpm/modules/debug/mt6886/mtk-lpm-dbg-mt6886.ko")
        #mgk_64_platform_device_modules.pop("drivers/misc/mediatek/lpm/modules/debug/mt6897/mtk-lpm-dbg-mt6897.ko")
        #mgk_64_platform_device_modules.pop("drivers/misc/mediatek/lpm/modules/debug/mt6983/mtk-lpm-dbg-mt6983.ko")
        #mgk_64_platform_device_modules.pop("drivers/misc/mediatek/lpm/modules/debug/mt6985/mtk-lpm-dbg-mt6985.ko")
        #mgk_64_platform_device_modules.pop("drivers/misc/mediatek/lpm/modules/debug/mt6989/mtk-lpm-dbg-mt6989.ko")
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/lpm/modules/debug/mt6991:mtk-lpm-dbg-mt6991".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/slbc:slbc_mt6895".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/slbc:slbc_mt6991".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/slbc:slbc_mt6993".format(kernel_version))
        mgk_64_platform_device_modules.pop("drivers/clk/mediatek/clk-chk-mt6989.ko")
        mgk_64_platform_device_modules.pop("drivers/clk/mediatek/pd-chk-mt6989.ko")
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/soc/mediatek/devapc:device-apc-mt6789".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/soc/mediatek/devapc:device-apc-mt6895".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/soc/mediatek/devapc:device-apc-mt6991".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/soc/mediatek/devapc:device-apc-mt6993".format(kernel_version))

    if "mt6761_overlay.config" in DEFCONFIG_OVERLAYS:
        mgk_64_kleaf_modules.append("//vendor/mediatek/kernel_modules/connectivity/wlan/core/gen4m/build/connac1x/6765:wlan_drv_gen4m_6765")
        mgk_64_kleaf_eng_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase:ktf_testcase")
        mgk_64_kleaf_userdebug_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase:ktf_testcase")
        mgk_64_kleaf_eng_modules.append("//vendor/mediatek/tests/kernel/ktf_testcase:ktf_testcase_k61")
        mgk_64_kleaf_userdebug_modules.append("//vendor/mediatek/tests/kernel/ktf_testcase:ktf_testcase_k61")
        mgk_64_kleaf_eng_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase/slbc:ktf_slbc_it")
        mgk_64_kleaf_userdebug_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase/slbc:ktf_slbc_it")

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
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/spmi:spmi-mtk-mpu".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/spmi:spmi-mtk-pmif".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/trusted_mem:ffa".format(kernel_version))
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
        mgk_64_kleaf_userdebug_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase/mml:ktf_mml_ait")
        mgk_64_kleaf_eng_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase/mml:ktf_mml_ait")
        mgk_64_kleaf_userdebug_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase/display_mobile_wbgai:ktf_display_mobile_wbgai")
        mgk_64_kleaf_userdebug_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase/display_pq_wbgai:ktf_display_pq_wbgai")
        mgk_64_kleaf_eng_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase/display_mobile_wbgai:ktf_display_mobile_wbgai")
        mgk_64_kleaf_eng_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase/display_pq_wbgai:ktf_display_pq_wbgai")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/gpu/drm/mediatek/mediatek_v2:mtk_disp_sec".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/gpu/mediatek/gpueb:mtk_gpueb".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/gpueb:mtk_ghpm_mt6991".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/gpueb:mtk_ghpm_mt6993".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/gpu/mediatek/gpufreq/v2:mtk_gpufreq_wrapper".format(kernel_version))
        mgk_64_kleaf_device_modules.append("//kernel_device_modules-{}/drivers/gpu/mediatek/gpufreq/v2_legacy:mtk_gpufreq_wrapper_legacy".format(kernel_version))
        #mgk_64_platform_device_modules.update({"drivers/gpu/mediatek/gpufreq/v2_legacy/mtk_gpufreq_mt6761.ko":"mt6761"})
        #mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6886.ko")
        #mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6897.ko")
        #mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6899.ko")
        #mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6985.ko")
        #mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6989.ko")
        #mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6989_fpga.ko")
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/gpufreq/v2:mtk_gpufreq_mt6991".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/gpufreq/v2:mtk_gpufreq_mt6993".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/gpufreq/v2:mtk_gpufreq_mt6855".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/gpufreq/v2:mtk_gpufreq_mt6895".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/ged:mtk_ged_mt6991".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/ged:mtk_ged_mt6993".format(kernel_version))
        mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpu_pdma/mtk_gpu_pdma_mt6991.ko")
        mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpu_pdma/mtk_gpu_pdma_mt6993.ko")
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/gpu_pdma:mtk_gpu_pdma_mt6991".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/gpu_pdma:mtk_gpu_pdma_mt6993".format(kernel_version))
        #mgk_64_platform_device_modules.pop("drivers/misc/mediatek/lpm/modules/debug/mt6886/mtk-lpm-dbg-mt6886.ko")
        #mgk_64_platform_device_modules.pop("drivers/misc/mediatek/lpm/modules/debug/mt6897/mtk-lpm-dbg-mt6897.ko")
        #mgk_64_platform_device_modules.pop("drivers/misc/mediatek/lpm/modules/debug/mt6983/mtk-lpm-dbg-mt6983.ko")
        #mgk_64_platform_device_modules.pop("drivers/misc/mediatek/lpm/modules/debug/mt6985/mtk-lpm-dbg-mt6985.ko")
        #mgk_64_platform_device_modules.pop("drivers/misc/mediatek/lpm/modules/debug/mt6989/mtk-lpm-dbg-mt6989.ko")
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/lpm/modules/debug/mt6991:mtk-lpm-dbg-mt6991".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/iommu/arm/arm-smmu-v3:arm_smmu_v3".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/iommu/arm/arm-smmu-v3:mtk-smmuv3-pmu".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/iommu/arm/arm-smmu-v3:mtk-smmuv3-mpam-mon".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/iommu:mtk_smmu_qos".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/iommu:smmu_secure".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/iommu:iommu_secure".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/dma-buf/heaps:mtk_sec_heap".format(kernel_version))
        mgk_64_device_modules.remove("drivers/tee/gud/610/TlcTui/t-base-tui.ko")

        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/slbc:mmsram".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/slbc:mtk_slbc".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/slbc:slbc_ipi".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/slbc:slbc_trace".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/slbc:slbc_mt6895".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/slbc:slbc_mt6991".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/slbc:slbc_mt6993".format(kernel_version))

        mgk_64_device_modules.append("drivers/misc/mediatek/thermal/thermal_monitor.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/hps_v3/mtk_cpuhp.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/power_gs_v1/mtk_power_gs_v1.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cm_mgr_legacy_v0/mtk_cm_mgr_v0.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cpuidle/mtk_cpuidle.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/mcdi/mcdi.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/spm/common_v0/MTK_INTERNAL_SPM.ko")

        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/ise_lpm:ise_lpm".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/ise_lpm:ise_lpm_v2".format(kernel_version))
        mgk_64_device_modules.remove("drivers/misc/mediatek/ise_trusty/ise-trusty.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/ise_trusty/ise-trusty-ipc.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/ise_trusty/ise-trusty-log.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/ise_trusty/ise-trusty-virtio.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/lpm/governors:lpm-gov-MHSP".format(kernel_version))
        mgk_64_device_modules.remove("drivers/misc/mediatek/lpm/modules/debug/v1/mtk-lpm-dbg-common-v1.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/lpm/modules/debug/v2/mtk-lpm-dbg-common-v2.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/lpm/modules/platform/v1/mtk-lpm-plat-v1.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/lpm/modules/platform/v2/mtk-lpm-plat-v2.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/lpm/mtk-lpm.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/mbraink/bridge:mtk_mbraink_bridge".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/mbraink/perf:mtk_mbraink_perf".format(kernel_version))
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/mtk_mbraink.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/memory/mediatek:slc-parity".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/mminfra:mtk-mminfra-debug".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/mminfra:mtk-mminfra-imax".format(kernel_version))
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi_legacy_v1/emicen.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi_legacy_v1/emiisu.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi_legacy_v1/emimpu.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi_legacy_v1/emictrl.ko")
        mgk_64_device_modules.append("drivers/memory/mediatek/emi_legacy/emi-dummy.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mminfra/mtk-mminfra-debug.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mminfra/mtk-mminfra-imax.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/smi/mtk-smi-bwc.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/videocodec/vcodec_kernel_common_driver.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/videocodec/vcodec_kernel_driver-v1.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/vcp/rv:vcp".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/vcp/rv:vcp_status".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/vdec_fmt:vdec-fmt".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/media/platform/mtk-vcodec:mtk-vcodec-dec-v2".format(kernel_version))
        mgk_64_device_modules.remove("drivers/media/platform/mtk-vcodec/mtk-vcodec-enc-v2.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/media/platform/mtk-vcodec:mtk-vcodec-dec-v1".format(kernel_version))
        mgk_64_device_modules.remove("drivers/media/platform/mtk-vcodec/mtk-vcodec-enc-v1.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/media/platform/mtk-vcodec:mtk-vcodec-common".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/media/platform/mtk-vcu:mtk-vcu".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/soc/mediatek:mtk-mmdvfs-v3".format(kernel_version))
        mgk_64_device_modules.remove("drivers/misc/mediatek/qos/mtk_qos.ko")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/cpufreq_cus:cpufreq_cus")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/cpufreq_int:cpufreq_int")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/mtkcam/scpsys/mtk-aov:mtk_aov")
        mgk_64_platform_device_modules.pop("drivers/clk/mediatek/clk-chk-mt6989.ko")
        mgk_64_platform_device_modules.pop("drivers/clk/mediatek/pd-chk-mt6989.ko")
        mgk_64_platform_device_modules.pop("drivers/clk/mediatek/clk-chk-mt6991.ko")
        mgk_64_platform_device_modules.pop("drivers/clk/mediatek/pd-chk-mt6991.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/modules/v6989/mtk_mbraink_v6989.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/modules/v6991/mtk_mbraink_v6991.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/modules/v6899/mtk_mbraink_v6899.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/modules/v6993/mtk_mbraink_v6993.ko")
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
        # mgk_64_device_modules.remove("drivers/misc/mediatek/cam_cal/src/custom/camera_eeprom.ko")
        # mgk_64_device_modules.append("drivers/misc/mediatek/cam_cal/src/isp3_m/camera_eeprom_isp3_m.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/cam_cal/src:camera_eeprom".format(kernel_version))
        mgk_64_kleaf_device_modules.append("//kernel_device_modules-{}/drivers/misc/mediatek/cam_cal/src:camera_eeprom_isp3_m".format(kernel_version))

        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug/v1:mtk-swpm-dbg-common-v1".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug/v1/subsys:mtk-swpm-isp-wrapper".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug/v6991/subsys:mtk-swpm-audio-dbg-v6991".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug/v6991/subsys:mtk-swpm-disp-dbg-v6991".format(kernel_version))
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
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug:mtk-swpm-dbg-v6991".format(kernel_version))
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6991/mtk-swpm-disp-dbg-v6991.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6991/mtk-swpm-mml-dbg-v6991.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug/v6991/subsys:mtk-swpm-isp-dbg-v6991".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug:mtk-swpm-dbg-v6993".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug/v6993/subsys:mtk-swpm-isp-dbg-v6993".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm:mtk-swpm".format(kernel_version))
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
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug/v6993/subsys:mtk-swpm-disp-dbg-v6993".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/pmsr:pmsr".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/pmsr/v2:pmsr_v2".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/pmsr/v3:pmsr_v3".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/pmsr/v4:pmsr_v4".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:mtk_cm_mgr".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:mtk_cm_ipi".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:mtk_cm_mgr_mt6989".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:mtk_cm_mgr_mt6886".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:mtk_cm_mgr_mt6897".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:mtk_cm_mgr_mt6983".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:mtk_cm_mgr_mt6985".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:mtk_cm_mgr_mt6991".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:mtk_cm_mgr_mt6993".format(kernel_version))
        mgk_64_device_modules.remove("drivers/misc/mediatek/ssc/debug/v1/mtk-ssc-dbg-v1.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/ssc/debug/v2/mtk-ssc-dbg-v2.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/ssc/mtk-ssc.ko")
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/mcupm/v3:mcupm".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/mcupm/v2:mcupm".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//vendor/mediatek/kernel_modules/gpu:gpu_mt6895")
        mgk_64_kleaf_modules.append("//vendor/mediatek/kernel_modules/gpu:gpu_mt6761")

        mgk_64_device_modules.remove("drivers/tee/teei/510/isee-ffa.ko")
        mgk_64_device_modules.append("drivers/tee/teei/515/isee.ko")

        mgk_64_kleaf_device_modules.append("//kernel_device_modules-{}/drivers/misc/mediatek/conn_md:conn_md_drv".format(kernel_version))

        mgk_64_device_modules.append("drivers/misc/mediatek/cameraisp/src/isp_3/camera_isp_3_m.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cameraisp/src/isp_3/cam_qos_3.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cameraisp/fdvt/camera_fdvt_isp35.ko")

        #mgk_64_device_modules.remove("sound/soc/codecs/snd-soc-mt6338.ko")
        #mgk_64_device_modules.remove("sound/soc/codecs/snd-soc-mt6368.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/sound/soc/mediatek/audio_dsp:mtk-soc-offload-common".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/sound/soc/mediatek/audio_dsp:snd-soc-audiodsp-common".format(kernel_version),)
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/sound/soc/mediatek/ultrasound/ultra_common:mtk-scp-ultra".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/sound/soc/mediatek/ultrasound/ultra_scp:snd-soc-mtk-scp-ultra".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/sound/soc/mediatek/vow:mtk-scp-vow".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/adsp:adsp".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/adsp/v1:adsp-v1".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/adsp/v2:adsp-v2".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/adsp/v3:adsp-v3".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/audio_ipi:audio_ipi".format(kernel_version))
        mgk_64_device_modules.remove("drivers/misc/mediatek/usb/usb_offload/usb_offload.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/vow/ver02:mtk-vow".format(kernel_version))
        #mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6886/mt6886-mt6368.ko")
        #mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6886/snd-soc-mt6886-afe.ko")
        #mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6897/mt6897-mt6368.ko")
        #mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6897/snd-soc-mt6897-afe.ko")
        #mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6983/mt6983-mt6338.ko")
        #mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6983/snd-soc-mt6983-afe.ko")
        #mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6985/mt6985-mt6338.ko")
        #mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6985/snd-soc-mt6985-afe.ko")

        mgk_64_device_modules.append("sound/soc/mediatek/codec/snd-mtk-soc-codec-6357.ko")
        mgk_64_device_modules.append("sound/soc/codecs/snd-soc-rt5509.ko")

        mgk_64_device_modules.remove("drivers/misc/mediatek/mdpm/mtk_mdpm.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/mdpm_v1/mtk_mdpm_v1.ko")
        mgk_64_kleaf_platform_modules.update({"//kernel_device_modules-{}/drivers/misc/mediatek/dcm:mt6761_dcm".format(kernel_version):"mt6761"})

        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/widevine_drm:widevine_driver".format(kernel_version))

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
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/sound/soc/codecs:mt6681-accdet".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/gpu/drm/mediatek/mediatek_v2:mtk_aod_scp".format(kernel_version))
        mgk_64_device_modules.remove("drivers/misc/mediatek/pkvm_tmem/pkvm_tmem.ko")
        mgk_64_device_modules.append("drivers/mmc/host/mtk-mmc-swcqhci.ko")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/mtkcam/sched:c2ps")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/mtkcam/sched:c2ps_perf_ioctl")
        mgk_64_common_eng_modules.remove("drivers/perf/arm_dsu_pmu.ko")
        mgk_64_common_userdebug_modules.remove("drivers/perf/arm_dsu_pmu.ko")
        mgk_64_common_user_modules.remove("drivers/perf/arm_dsu_pmu.ko")


    if "mt6768_overlay_ref.config" in DEFCONFIG_OVERLAYS:
        mgk_64_device_modules.append("drivers/misc/mediatek/flashlight/flashlights-ocp81375.ko")
        mgk_64_kleaf_eng_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase/emi_slc:ktf_emi_slc_ut")
        mgk_64_kleaf_userdebug_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase/emi_slc:ktf_emi_slc_ut")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/typec/mux:fusb304".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/typec/mux:mux_switch".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/typec/mux:ps5169".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/typec/mux:ps5170".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/typec/mux:ptn36241g".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/typec/mux:usb_dp_selector".format(kernel_version))

        mgk_64_device_modules.append("drivers/power/supply/mtk_chg_det.ko")
        mgk_64_device_modules.append("sound/soc/mediatek/mt6768/mt6768-mt6358-ref.ko")

        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/typec/tcpc:pd_dbg_info".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/typec/tcpc:rt_pd_manager".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/typec/tcpc:tcpc_class".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/typec/tcpc:tcpc_mt6375".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/typec/tcpc:tcpc_mt6379".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/typec/tcpc:tcpc_rt1711h".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/typec/tcpc:tcpci_late_sync".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/power/supply/ufcs:ufcs_class".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/power/supply/ufcs:ufcs_mt6379".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/power/supply:mt6379-chg".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/power/supply:mtk_2p_charger".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/power/supply:mtk_ufcs_adapter".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/power/supply:mtk_chg_type_det".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/power/supply:mtk_hvbpc".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/power/supply:mtk_pd_adapter".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/power/supply:mtk_pd_charging".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/power/supply:mtk_pep".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/power/supply:mtk_pep20".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/power/supply:mtk_pep40".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/power/supply:mtk_pep45".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/power/supply:mtk_pep50".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/power/supply:mtk_pep50p".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/power/supply:rt9759".format(kernel_version))
        mgk_64_kleaf_device_modules.append("//kernel_device_modules-{}/drivers/power/supply:sgm41516d".format(kernel_version))

    if "mt6893_overlay.config" in DEFCONFIG_OVERLAYS:
        mgk_64_kleaf_modules.append("//vendor/mediatek/kernel_modules/connectivity/wlan/core/gen4m/build/connac2x/6893:wlan_drv_gen4m_6893")
        mgk_64_kleaf_platform_modules.pop("//vendor/mediatek/kernel_modules/gpu:gpu_mt6895")
        mgk_64_kleaf_modules.append("//vendor/mediatek/kernel_modules/gpu:gpu_mt6893")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/mtkcam/sched:c2ps")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/mtkcam/sched:c2ps_perf_ioctl")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/mtkcam/isp_pspm:isp_pspm")

        mgk_64_platform_device_modules.update({"drivers/gpu/mediatek/gpufreq/v2_legacy/mtk_gpufreq_mt6893.ko":"mt6893"})
        mgk_64_kleaf_platform_modules.update({"//kernel_device_modules-{}/drivers/pinctrl/mediatek:pinctrl-mt6893".format(kernel_version): "mt6893"})
        #mgk_64_platform_device_modules.update({"drivers/misc/mediatek/cm_mgr_legacy_v1/mtk_cm_mgr_mt6893.ko":"mt6893"})

        mgk_64_common_eng_modules.remove("drivers/firmware/arm_ffa/ffa-module.ko")
        mgk_64_common_userdebug_modules.remove("drivers/firmware/arm_ffa/ffa-module.ko")
        mgk_64_common_user_modules.remove("drivers/firmware/arm_ffa/ffa-module.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/pkvm_tmem/pkvm_tmem.ko")
        mgk_64_device_modules.append("drivers/tee/gud/600/MobiCoreDriver/mcDrvModule.ko")
        mgk_64_device_modules.append("drivers/tee/gud/600/MobiCoreDriver/mcDrvModule-ffa.ko")
        mgk_64_device_modules.append("drivers/tee/gud/600/TlcTui/t-base-tui.ko")


        mgk_64_kleaf_device_modules.append("//kernel_device_modules-{}/drivers/misc/mediatek/dcm:mt6885_dcm".format(kernel_version))

        mgk_64_device_modules.append("drivers/misc/mediatek/cameraisp/wpe/isp_6s/camera_wpe_isp6s.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cameraisp/dip/isp_6s/camera_dip_isp6s.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cameraisp/dpe/camera_dpe_isp60.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cameraisp/mfb/camera_mfb_isp6s.ko")

        mgk_64_device_modules.append("drivers/misc/mediatek/smi/mtk-smi-bwc.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cpuidle/mtk_cpuidle.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cpuhotplug/mtk_cpuhp.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/upower/Upower.ko")
        mgk_64_kleaf_device_modules.append("//kernel_device_modules-{}/drivers/soc/mediatek/devmpu:devmpu".format(kernel_version))
        mgk_64_device_modules.append("drivers/misc/mediatek/eemgpu/mtk_eem.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/leakage_table_v2/mediatek_static_power.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/sspm/v2/sspm.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/mcupm/v1/mcupm.ko")
        mgk_64_kleaf_device_modules.append("//kernel_device_modules-{}/drivers/gpu/mediatek/gpufreq/v2_legacy:mtk_gpufreq_wrapper_legacy".format(kernel_version))
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

        mgk_64_kleaf_userdebug_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase/mml:ktf_mml_ait")
        mgk_64_kleaf_eng_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase/mml:ktf_mml_ait")
        mgk_64_kleaf_userdebug_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase/display_mobile_wbgai:ktf_display_mobile_wbgai")
        mgk_64_kleaf_userdebug_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase/display_pq_wbgai:ktf_display_pq_wbgai")
        mgk_64_kleaf_eng_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase/display_mobile_wbgai:ktf_display_mobile_wbgai")
        mgk_64_kleaf_eng_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase/display_pq_wbgai:ktf_display_pq_wbgai")

        mgk_64_device_modules.remove("drivers/gpu/drm/panel/mediatek-cust-panel-sample.ko")
        mgk_64_device_modules.remove("drivers/gpu/drm/panel/mediatek-drm-gateic.ko")
        mgk_64_device_modules.remove("drivers/gpu/drm/panel/mediatek-drm-panel-drv.ko")

        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/gpu/mediatek/gpueb:mtk_gpueb".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/gpueb:mtk_ghpm_mt6991".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/gpueb:mtk_ghpm_mt6993".format(kernel_version))

        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/gpu/mediatek/gpufreq/v2:mtk_gpufreq_wrapper".format(kernel_version))

        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/iommu/arm/arm-smmu-v3:arm_smmu_v3".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/iommu/arm/arm-smmu-v3:mtk-smmuv3-pmu".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/iommu/arm/arm-smmu-v3:mtk-smmuv3-mpam-mon".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/iommu:mtk_smmu_qos".format(kernel_version))

        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/adsp:adsp".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/adsp/v1:adsp-v1".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/adsp/v2:adsp-v2".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/adsp/v3:adsp-v3".format(kernel_version))

        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/mminfra:mtk-mminfra-debug".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/mminfra:mtk-mminfra-imax".format(kernel_version))
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

        #mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6886.ko")
        #mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6897.ko")
        #mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6899.ko")
        #mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6985.ko")
        #mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6989.ko")
        #mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6989_fpga.ko")
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/gpufreq/v2:mtk_gpufreq_mt6991".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/gpufreq/v2:mtk_gpufreq_mt6993".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/gpufreq/v2:mtk_gpufreq_mt6855".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/gpufreq/v2:mtk_gpufreq_mt6895".format(kernel_version))

        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/ged:mtk_ged_mt6991".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/ged:mtk_ged_mt6993".format(kernel_version))

        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:mtk_cm_mgr_mt6989".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:mtk_cm_mgr_mt6886".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:mtk_cm_mgr_mt6897".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:mtk_cm_mgr_mt6983".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:mtk_cm_mgr_mt6985".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:mtk_cm_mgr_mt6991".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:mtk_cm_mgr_mt6993".format(kernel_version))

        #mgk_64_platform_device_modules.pop("drivers/misc/mediatek/lpm/modules/debug/mt6886/mtk-lpm-dbg-mt6886.ko")
        #mgk_64_platform_device_modules.pop("drivers/misc/mediatek/lpm/modules/debug/mt6897/mtk-lpm-dbg-mt6897.ko")
        #mgk_64_platform_device_modules.pop("drivers/misc/mediatek/lpm/modules/debug/mt6983/mtk-lpm-dbg-mt6983.ko")
        #mgk_64_platform_device_modules.pop("drivers/misc/mediatek/lpm/modules/debug/mt6985/mtk-lpm-dbg-mt6985.ko")
        #mgk_64_platform_device_modules.pop("drivers/misc/mediatek/lpm/modules/debug/mt6989/mtk-lpm-dbg-mt6989.ko")
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/lpm/modules/debug/mt6991:mtk-lpm-dbg-mt6991".format(kernel_version))

        #mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6985/mt6985-mt6338.ko")
        #mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6985/snd-soc-mt6985-afe.ko")

        #mgk_64_platform_device_modules.pop("drivers/clk/mediatek/clk-chk-mt6989.ko")
        #mgk_64_platform_device_modules.pop("drivers/clk/mediatek/pd-chk-mt6989.ko")
        #mgk_64_platform_device_modules.pop("drivers/clk/mediatek/clk-chk-mt6991.ko")
        #mgk_64_platform_device_modules.pop("drivers/clk/mediatek/pd-chk-mt6991.ko")

        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/vcp/rv:vcp".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/vcp/rv:vcp_status".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/vdec_fmt:vdec-fmt".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/media/platform/mtk-vcodec:mtk-vcodec-dec-v1".format(kernel_version))
        mgk_64_device_modules.remove("drivers/media/platform/mtk-vcodec/mtk-vcodec-enc-v1.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/soc/mediatek:mtk-mmdvfs-v3".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/sound/soc/codecs:mt6681-accdet".format(kernel_version))

        mgk_64_device_modules.append("drivers/misc/mediatek/sensor/2.0/mtk_nanohub/nanohub.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/sensor/2.0/sensorhub/sensorhub.ko")

        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/pmsr:pmsr".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/pmsr/v2:pmsr_v2".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/pmsr/v3:pmsr_v3".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/pmsr/v4:pmsr_v4".format(kernel_version))

        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug/v1:mtk-swpm-dbg-common-v1".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug/v1/subsys:mtk-swpm-isp-wrapper".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug/v6991/subsys:mtk-swpm-audio-dbg-v6991".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug/v6991/subsys:mtk-swpm-disp-dbg-v6991".format(kernel_version))
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

        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm:mtk-swpm".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm:mtk-swpm-perf-arm-pmu".format(kernel_version))
        mgk_64_device_modules.remove("drivers/misc/mediatek/tinysys_scmi/tinysys-scmi.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/trusted_mem:ffa".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug:mtk-swpm-dbg-v6991".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug:mtk-swpm-dbg-v6993".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/audio_ipi:audio_ipi".format(kernel_version))
        mgk_64_device_modules.append("drivers/misc/mediatek/ppm_v3/mtk_ppm_v3.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/cm_mgr/mtk_cm_ipi.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/iommu:smmu_secure".format(kernel_version))

        mgk_64_device_modules.append("drivers/misc/mediatek/cm_mgr_legacy_v1/mtk_cm_mgr.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/ise_lpm:ise_lpm".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/ise_lpm:ise_lpm_v2".format(kernel_version))
        mgk_64_device_modules.remove("drivers/misc/mediatek/ise_trusty/ise-trusty.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/ise_trusty/ise-trusty-ipc.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/ise_trusty/ise-trusty-log.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/ise_trusty/ise-trusty-virtio.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/lpm/governors:lpm-gov-MHSP".format(kernel_version))
        mgk_64_device_modules.remove("drivers/misc/mediatek/lpm/modules/debug/v1/mtk-lpm-dbg-common-v1.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/lpm/modules/debug/v2/mtk-lpm-dbg-common-v2.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/lpm/modules/platform/v1/mtk-lpm-plat-v1.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/lpm/modules/platform/v2/mtk-lpm-plat-v2.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/lpm/mtk-lpm.ko")

        mgk_64_device_modules.remove("drivers/misc/mediatek/ssc/debug/v1/mtk-ssc-dbg-v1.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/ssc/debug/v2/mtk-ssc-dbg-v2.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/ssc/mtk-ssc.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/mtk_mbraink.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/perf/mtk_mbraink_perf.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/sspm/v3/sspm_v3.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/usb/usb_offload/usb_offload.ko")
        mgk_64_device_modules.remove("drivers/tee/teei/510/isee-ffa.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/sound/soc/mediatek/audio_dsp:mtk-soc-offload-common".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/sound/soc/mediatek/audio_dsp:snd-soc-audiodsp-common".format(kernel_version),)
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/sound/soc/mediatek/ultrasound/ultra_common:mtk-scp-ultra".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/sound/soc/mediatek/ultrasound/ultra_scp:snd-soc-mtk-scp-ultra".format(kernel_version))
        mgk_64_device_modules.remove("drivers/misc/mediatek/performance/mtk_perf_ioctl_magt.ko")

        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/modules/v6989/mtk_mbraink_v6989.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/modules/v6991/mtk_mbraink_v6991.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/modules/v6899/mtk_mbraink_v6899.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/modules/v6993/mtk_mbraink_v6993.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/mbraink/bridge:mtk_mbraink_bridge".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/mbraink/perf:mtk_mbraink_perf".format(kernel_version))
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6991/mtk-swpm-audio-dbg-v6991.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6991/mtk-swpm-disp-dbg-v6991.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/swpm/modules/debug/v6991/mtk-swpm-mml-dbg-v6991.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug/v6991/subsys:mtk-swpm-isp-dbg-v6991".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug/v6993/subsys:mtk-swpm-disp-dbg-v6993".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug/v6993/subsys:mtk-swpm-isp-dbg-v6993".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/gpu_pdma:mtk_gpu_pdma_mt6991".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/gpu_pdma:mtk_gpu_pdma_mt6993".format(kernel_version))

        mgk_64_device_modules.remove("drivers/mailbox/mtk-mbox-mailbox.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/pgboost/pgboost.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/lpm_legacy/modules/platform/v1/mtk-lpm-plat-v1-legacy.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/lpm_legacy/modules/debug/k6893/mtk-lpm-dbg-mt6893-legacy.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/lpm_legacy/mtk-lpm-legacy.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/lpm_legacy/modules/debug/v1/mtk-lpm-dbg-common-v1-legacy.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/eem_v2/mediatek_eem.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/slbc:slbc_ipi".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/slbc:slbc_trace".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/slbc:slbc_mt6895".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/slbc:slbc_mt6991".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/slbc:slbc_mt6993".format(kernel_version))

    if "mt6765_overlay.config" in DEFCONFIG_OVERLAYS:
        mgk_64_kleaf_modules.append("//vendor/mediatek/kernel_modules/connectivity/wlan/core/gen4m/build/connac1x/6765:wlan_drv_gen4m_6765")
        mgk_64_kleaf_eng_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase/slbc:ktf_slbc_it")
        mgk_64_kleaf_userdebug_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase/slbc:ktf_slbc_it")
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

        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/sound/soc/codecs:mt6681-accdet".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/sound/soc/codecs:mt6359p-accdet".format(kernel_version))

        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/slbc:mmsram".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/slbc:mtk_slbc".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/slbc:slbc_ipi".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/slbc:slbc_trace".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/slbc:slbc_mt6895".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/slbc:slbc_mt6991".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/slbc:slbc_mt6993".format(kernel_version))

        mgk_64_kleaf_device_modules.append("//kernel_device_modules-{}/drivers/misc/mediatek/conn_md:conn_md_drv".format(kernel_version))

        mgk_64_device_modules.append("drivers/tee/gud/600/MobiCoreDriver/mcDrvModule.ko")
        mgk_64_device_modules.append("drivers/tee/gud/600/MobiCoreDriver/mcDrvModule-ffa.ko")
        mgk_64_device_modules.append("drivers/tee/gud/600/TlcTui/t-base-tui.ko")
        mgk_64_device_modules.append("drivers/tee/teei/515/isee.ko")

        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/gpu/mediatek/gpueb:mtk_gpueb".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/gpueb:mtk_ghpm_mt6991".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/gpueb:mtk_ghpm_mt6993".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/gpu/mediatek/gpufreq/v2:mtk_gpufreq_wrapper".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/spmi:spmi-mtk-mpu".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/spmi:spmi-mtk-pmif".format(kernel_version))
        mgk_64_kleaf_device_modules.append("//kernel_device_modules-{}/drivers/gpu/mediatek/gpufreq/v2_legacy:mtk_gpufreq_wrapper_legacy".format(kernel_version))
        mgk_64_platform_device_modules.update({"drivers/gpu/mediatek/gpufreq/v2_legacy/mtk_gpufreq_mt6765.ko":"mt6765"})
        #mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6886.ko")
        #mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6897.ko")
        #mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6899.ko")
        #mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6985.ko")
        #mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6989.ko")
        #mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6989_fpga.ko")
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/gpufreq/v2:mtk_gpufreq_mt6991".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/gpufreq/v2:mtk_gpufreq_mt6993".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/gpufreq/v2:mtk_gpufreq_mt6855".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/gpufreq/v2:mtk_gpufreq_mt6895".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/gpu_pdma:mtk_gpu_pdma_mt6991".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/gpu_pdma:mtk_gpu_pdma_mt6993".format(kernel_version))

        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/ged:mtk_ged_mt6991".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/ged:mtk_ged_mt6993".format(kernel_version))

        mgk_64_device_modules.remove("drivers/gpu/drm/panel/mediatek-cust-panel-sample.ko")
        mgk_64_device_modules.remove("drivers/gpu/drm/panel/mediatek-drm-gateic.ko")
        mgk_64_device_modules.remove("drivers/gpu/drm/panel/mediatek-drm-panel-drv.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/gpu/drm/mediatek/dpc/dpc_v1:mtk_dpc_v1".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/gpu/drm/mediatek/dpc/dpc_v1:mtk_vdisp_v1".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/gpu/drm/mediatek/dpc/dpc_v2:mtk_dpc_v2".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/gpu/drm/mediatek/dpc/dpc_v2:mtk_vdisp_v2".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/gpu/drm/mediatek/dpc/dpc_v3:mtk_dpc_v3".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/gpu/drm/mediatek/dpc/dpc_v3:mtk_vdisp_v3".format(kernel_version))

        mgk_64_kleaf_userdebug_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase/mml:ktf_mml_ait")
        mgk_64_kleaf_eng_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase/mml:ktf_mml_ait")
        mgk_64_kleaf_userdebug_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase/display_mobile_wbgai:ktf_display_mobile_wbgai")
        mgk_64_kleaf_userdebug_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase/display_pq_wbgai:ktf_display_pq_wbgai")
        mgk_64_kleaf_eng_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase/display_mobile_wbgai:ktf_display_mobile_wbgai")
        mgk_64_kleaf_eng_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase/display_pq_wbgai:ktf_display_pq_wbgai")

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

        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/iommu/arm/arm-smmu-v3:arm_smmu_v3".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/iommu/arm/arm-smmu-v3:mtk-smmuv3-pmu".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/iommu/arm/arm-smmu-v3:mtk-smmuv3-mpam-mon".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/iommu:smmu_secure".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/iommu:mtk_smmu_qos".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/vcp/rv:vcp".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/vcp/rv:vcp_status".format(kernel_version))
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

        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/memory/mediatek:emi".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/memory/mediatek:emi-fake-eng".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/memory/mediatek:emi-fake-eng-v2".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/memory/mediatek:emi-mpu".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/memory/mediatek:emi-mpu-test".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/memory/mediatek:emi-mpu-test-v2".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/memory/mediatek:emi-slb".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/memory/mediatek:smpu".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/memory/mediatek:smpu-hook-v1".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/memory/mediatek:slc-parity".format(kernel_version))
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
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/trusted_mem:ffa".format(kernel_version))
        mgk_64_device_modules.remove("drivers/tee/teei/510/isee-ffa.ko")

        mgk_64_device_modules.append("drivers/misc/mediatek/videocodec/vcodec_kernel_common_driver.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/videocodec/vcodec_kernel_driver-v1.ko")
        #mgk_64_device_modules.remove("drivers/misc/mediatek/vdec_fmt/vdec-fmt.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/media/platform/mtk-vcodec:mtk-vcodec-dec-v2".format(kernel_version))
        mgk_64_device_modules.remove("drivers/media/platform/mtk-vcodec/mtk-vcodec-enc-v2.ko")

        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/soc/mediatek:mtk-mmdvfs-v3".format(kernel_version))

        mgk_64_device_modules.remove("drivers/misc/mediatek/sspm/v3/sspm_v3.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/sspm/v1/sspm_v1.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/qos/mtk_qos_legacy.ko")
        mgk_64_platform_device_modules.update({"drivers/power/supply/mt6357-charger-type.ko":"mt6765"})
        mgk_64_platform_device_modules.update({"drivers/power/supply/rt9465.ko":"mt6765"})

        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:mtk_cm_mgr".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:mtk_cm_ipi".format(kernel_version))

        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/modules/v6989/mtk_mbraink_v6989.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/modules/v6991/mtk_mbraink_v6991.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/modules/v6899/mtk_mbraink_v6899.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/modules/v6993/mtk_mbraink_v6993.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/mtk_mbraink.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/mbraink/bridge:mtk_mbraink_bridge".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/mbraink/perf:mtk_mbraink_perf".format(kernel_version))

        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/mminfra:mtk-mminfra-debug".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/mminfra:mtk-mminfra-imax".format(kernel_version))

        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/pmsr:pmsr".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/pmsr/v2:pmsr_v2".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/pmsr/v3:pmsr_v3".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/pmsr/v4:pmsr_v4".format(kernel_version))

        mgk_64_device_modules.remove("drivers/misc/mediatek/qos/mtk_qos.ko")

        mgk_64_device_modules.remove("drivers/misc/mediatek/ssc/debug/v1/mtk-ssc-dbg-v1.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/ssc/debug/v2/mtk-ssc-dbg-v2.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/ssc/mtk-ssc.ko")

        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug/v1:mtk-swpm-dbg-common-v1".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug/v1/subsys:mtk-swpm-isp-wrapper".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug/v6991/subsys:mtk-swpm-audio-dbg-v6991".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug/v6991/subsys:mtk-swpm-disp-dbg-v6991".format(kernel_version))
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
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug:mtk-swpm-dbg-v6991".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug:mtk-swpm-dbg-v6993".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm:mtk-swpm".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm:mtk-swpm-perf-arm-pmu".format(kernel_version))

        mgk_64_device_modules.remove("drivers/misc/mediatek/tinysys_scmi/tinysys-scmi.ko")

        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:mtk_cm_mgr_mt6886".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:mtk_cm_mgr_mt6897".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:mtk_cm_mgr_mt6983".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:mtk_cm_mgr_mt6985".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:mtk_cm_mgr_mt6991".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:mtk_cm_mgr_mt6993".format(kernel_version))

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
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug/v6991/subsys:mtk-swpm-isp-dbg-v6991".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug/v6993/subsys:mtk-swpm-disp-dbg-v6993".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug/v6993/subsys:mtk-swpm-isp-dbg-v6993".format(kernel_version))
        mgk_64_device_modules.remove("drivers/misc/mediatek/imgsensor/src/isp6s/imgsensor_isp6s.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/imgsensor/src/isp4_c/imgsensor_isp4_c.ko")
        # mgk_64_device_modules.remove("drivers/misc/mediatek/cam_cal/src/custom/camera_eeprom.ko")
        # mgk_64_device_modules.append("drivers/misc/mediatek/cam_cal/src/isp4_c/camera_eeprom_isp4_c.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/cam_cal/src:camera_eeprom".format(kernel_version))
        mgk_64_kleaf_device_modules.append("//kernel_device_modules-{}/drivers/misc/mediatek/cam_cal/src:camera_eeprom_isp4_c".format(kernel_version))
        mgk_64_device_modules.append("drivers/misc/mediatek/cameraisp/dpe/camera_dpe_isp40.ko")
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:mtk_cm_mgr_mt6989".format(kernel_version))

        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/mcupm/v3:mcupm".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/mcupm/v2:mcupm".format(kernel_version))
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/cpufreq_cus:cpufreq_cus")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/cpufreq_int:cpufreq_int")
        mgk_64_kleaf_device_modules.append("//kernel_device_modules-{}/drivers/misc/mediatek/dcm:mt6765_dcm".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//vendor/mediatek/kernel_modules/gpu:gpu_mt6895")
        mgk_64_kleaf_modules.append("//vendor/mediatek/kernel_modules/gpu:gpu_mt6765")

        #mgk_64_device_modules.remove("sound/soc/codecs/snd-soc-mt6338.ko")
        #mgk_64_device_modules.remove("sound/soc/codecs/snd-soc-mt6368.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/sound/soc/mediatek/audio_dsp:mtk-soc-offload-common".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/sound/soc/mediatek/audio_dsp:snd-soc-audiodsp-common".format(kernel_version),)
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/sound/soc/mediatek/ultrasound/ultra_common:mtk-scp-ultra".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/sound/soc/mediatek/ultrasound/ultra_scp:snd-soc-mtk-scp-ultra".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/sound/soc/mediatek/vow:mtk-scp-vow".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/adsp:adsp".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/adsp/v1:adsp-v1".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/adsp/v2:adsp-v2".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/adsp/v3:adsp-v3".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/audio_ipi:audio_ipi".format(kernel_version))
        mgk_64_device_modules.remove("drivers/misc/mediatek/usb/usb_offload/usb_offload.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/vow/ver02:mtk-vow".format(kernel_version))
        #mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6886/mt6886-mt6368.ko")
        #mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6886/snd-soc-mt6886-afe.ko")
        #mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6897/mt6897-mt6368.ko")
        #mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6897/snd-soc-mt6897-afe.ko")
        #mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6983/mt6983-mt6338.ko")
        #mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6983/snd-soc-mt6983-afe.ko")
        #mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6985/mt6985-mt6338.ko")
        #mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6985/snd-soc-mt6985-afe.ko")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/mtkcam/isp_pspm:isp_pspm")

        mgk_64_device_modules.append("drivers/misc/mediatek/scp/cm4/scp.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/gpu/drm/mediatek/mediatek_v2:mtk_aod_scp".format(kernel_version))
        mgk_64_device_modules.remove("drivers/misc/mediatek/apusys/apusys.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/apusys/apu_aov.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/apusys/power/apu_top.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/apusys/sapu/sapu.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/sensor/2.0/sensorhub/sensorhub.ko")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/mtkcam/scpsys/mtk-aov:mtk_aov")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/mtkcam/sched:c2ps")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/mtkcam/sched:c2ps_perf_ioctl")

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
        mgk_64_kleaf_eng_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase/slbc:ktf_slbc_it")
        mgk_64_kleaf_userdebug_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase/slbc:ktf_slbc_it")
        mgk_64_kleaf_platform_modules.pop("//vendor/mediatek/kernel_modules/gpu:gpu_mt6895")
        mgk_64_kleaf_modules.append("//vendor/mediatek/kernel_modules/gpu:gpu_mt6833")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/mtkcam/sched:c2ps")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/mtkcam/sched:c2ps_perf_ioctl")
        mgk_64_platform_device_modules.update({"drivers/gpu/mediatek/gpufreq/v2_legacy/mtk_gpufreq_mt6833.ko":"mt6833"})
        #mgk_64_device_modules.append("drivers/gpu/mediatek/gpufreq/v2_legacy/mtk_gpufreq_mt6833.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/char/rpmb:rpmb".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/char/rpmb:rpmb-mtk".format(kernel_version))
        mgk_64_common_eng_modules.remove("drivers/firmware/arm_ffa/ffa-module.ko")
        mgk_64_common_userdebug_modules.remove("drivers/firmware/arm_ffa/ffa-module.ko")
        mgk_64_common_user_modules.remove("drivers/firmware/arm_ffa/ffa-module.ko")
        mgk_64_device_modules.append("drivers/tee/gud/600/MobiCoreDriver/mcDrvModule.ko")
        mgk_64_device_modules.append("drivers/tee/gud/600/MobiCoreDriver/mcDrvModule-ffa.ko")

        mgk_64_device_modules.append("drivers/tee/gud/600/TlcTui/t-base-tui.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/iommu/arm/arm-smmu-v3:arm_smmu_v3".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/iommu/arm/arm-smmu-v3:mtk-smmuv3-pmu".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/iommu/arm/arm-smmu-v3:mtk-smmuv3-mpam-mon".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/iommu:smmu_secure".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/iommu:mtk_smmu_qos".format(kernel_version))
        mgk_64_kleaf_device_modules.append("//kernel_device_modules-{}/drivers/soc/mediatek/devmpu:devmpu".format(kernel_version))
        mgk_64_kleaf_device_modules.append("//kernel_device_modules-{}/drivers/misc/mediatek/conn_md:conn_md_drv".format(kernel_version))
        mgk_64_device_modules.remove("drivers/misc/mediatek/pkvm_tmem/pkvm_tmem.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/slbc:mmsram".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/slbc:mtk_slbc".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/slbc:slbc_ipi".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/slbc:slbc_trace".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/slbc:slbc_mt6895".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/slbc:slbc_mt6991".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/slbc:slbc_mt6993".format(kernel_version))
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
        mgk_64_kleaf_device_modules.append("//kernel_device_modules-{}/drivers/misc/mediatek/dcm:mt6833_dcm".format(kernel_version))
        #mgk_64_device_modules.append("drivers/misc/mediatek/eemgpu/mtk_eem.ko")
        mgk_64_kleaf_device_modules.append("//kernel_device_modules-{}/drivers/gpu/mediatek/gpufreq/v2_legacy:mtk_gpufreq_wrapper_legacy".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/gpu/mediatek/gpueb:mtk_gpueb".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/gpueb:mtk_ghpm_mt6991".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/gpueb:mtk_ghpm_mt6993".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/gpu/mediatek/gpufreq/v2:mtk_gpufreq_wrapper".format(kernel_version))

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
        #mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6886.ko")
        #mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6897.ko")
        #mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6899.ko")
        #mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6985.ko")
        #mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6989.ko")
        #mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6989_fpga.ko")
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/gpufreq/v2:mtk_gpufreq_mt6991".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/gpufreq/v2:mtk_gpufreq_mt6993".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/gpufreq/v2:mtk_gpufreq_mt6855".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/gpufreq/v2:mtk_gpufreq_mt6895".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/ged:mtk_ged_mt6991".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/ged:mtk_ged_mt6993".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/gpu_pdma:mtk_gpu_pdma_mt6991".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/gpu_pdma:mtk_gpu_pdma_mt6993".format(kernel_version))
        mgk_64_platform_device_modules.update({"drivers/interconnect/mediatek/mmqos-mt6833.ko":"mt6833"})

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
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/mminfra:mtk-mminfra-debug".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/mminfra:mtk-mminfra-imax".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/vcp/rv:vcp".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/vcp/rv:vcp_status".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/vdec_fmt:vdec-fmt".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/media/platform/mtk-vcodec:mtk-vcodec-dec-v2".format(kernel_version))
        mgk_64_device_modules.remove("drivers/media/platform/mtk-vcodec/mtk-vcodec-enc-v2.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/soc/mediatek:mtk-mmdvfs-v3".format(kernel_version))
        mgk_64_platform_device_modules.pop("drivers/clk/mediatek/clk-chk-mt6989.ko")
        mgk_64_platform_device_modules.pop("drivers/clk/mediatek/pd-chk-mt6989.ko")
        mgk_64_platform_device_modules.pop("drivers/clk/mediatek/clk-chk-mt6991.ko")
        mgk_64_platform_device_modules.pop("drivers/clk/mediatek/pd-chk-mt6991.ko")

        mgk_64_device_modules.append("drivers/misc/mediatek/sspm/v2/sspm.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/sspm/v3/sspm_v3.ko")
        #mgk_64_platform_device_modules.pop("drivers/misc/mediatek/lpm/modules/debug/mt6989/mtk-lpm-dbg-mt6989.ko")
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/lpm/modules/debug/mt6991:mtk-lpm-dbg-mt6991".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/mbraink/bridge:mtk_mbraink_bridge".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/mbraink/perf:mtk_mbraink_perf".format(kernel_version))
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/mtk_mbraink.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/modules/v6989/mtk_mbraink_v6989.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/modules/v6991/mtk_mbraink_v6991.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/modules/v6899/mtk_mbraink_v6899.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/modules/v6993/mtk_mbraink_v6993.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/pmsr:pmsr".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/pmsr/v2:pmsr_v2".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/pmsr/v3:pmsr_v3".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/pmsr/v4:pmsr_v4".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug/v1/subsys:mtk-swpm-isp-wrapper".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug/v1:mtk-swpm-dbg-common-v1".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug/v6991/subsys:mtk-swpm-audio-dbg-v6991".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug/v6991/subsys:mtk-swpm-disp-dbg-v6991".format(kernel_version))
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
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug:mtk-swpm-dbg-v6991".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug:mtk-swpm-dbg-v6993".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm:mtk-swpm".format(kernel_version))
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
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug/v6991/subsys:mtk-swpm-isp-dbg-v6991".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug/v6993/subsys:mtk-swpm-disp-dbg-v6993".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug/v6993/subsys:mtk-swpm-isp-dbg-v6993".format(kernel_version))
        mgk_64_device_modules.remove("drivers/misc/mediatek/tinysys_scmi/tinysys-scmi.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:mtk_cm_mgr".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:mtk_cm_ipi".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:mtk_cm_mgr_mt6886".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:mtk_cm_mgr_mt6897".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:mtk_cm_mgr_mt6983".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:mtk_cm_mgr_mt6985".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:mtk_cm_mgr_mt6989".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:mtk_cm_mgr_mt6991".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:mtk_cm_mgr_mt6993".format(kernel_version))
        mgk_64_device_modules.remove("drivers/misc/mediatek/ssc/debug/v1/mtk-ssc-dbg-v1.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/ssc/debug/v2/mtk-ssc-dbg-v2.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/ssc/mtk-ssc.ko")

        mgk_64_kleaf_userdebug_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase/mml:ktf_mml_ait")
        mgk_64_kleaf_eng_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase/mml:ktf_mml_ait")
        mgk_64_kleaf_userdebug_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase/display_mobile_wbgai:ktf_display_mobile_wbgai")
        mgk_64_kleaf_userdebug_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase/display_pq_wbgai:ktf_display_pq_wbgai")
        mgk_64_kleaf_eng_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase/display_mobile_wbgai:ktf_display_mobile_wbgai")
        mgk_64_kleaf_eng_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase/display_pq_wbgai:ktf_display_pq_wbgai")

        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/trusted_mem:ffa".format(kernel_version))
        mgk_64_device_modules.remove("drivers/tee/teei/510/isee-ffa.ko")
        mgk_64_device_modules.append("drivers/tee/teei/515/isee.ko")

        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/gpu/drm/mediatek/dpc/dpc_v1:mtk_dpc_v1".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/gpu/drm/mediatek/dpc/dpc_v1:mtk_vdisp_v1".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/gpu/drm/mediatek/dpc/dpc_v2:mtk_dpc_v2".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/gpu/drm/mediatek/dpc/dpc_v2:mtk_vdisp_v2".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/gpu/drm/mediatek/dpc/dpc_v3:mtk_dpc_v3".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/gpu/drm/mediatek/dpc/dpc_v3:mtk_vdisp_v3".format(kernel_version))
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

        #mgk_64_platform_device_modules.update({"drivers/misc/mediatek/cm_mgr_legacy_v1/mtk_cm_mgr_mt6833.ko":"mt6833"})
        mgk_64_device_modules.append("drivers/misc/mediatek/cm_mgr_legacy_v1/mtk_cm_mgr.ko")

        mgk_64_device_modules.append("drivers/misc/mediatek/spi_slave_drv/spi_slave.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cmdq/bridge/cmdq-bdg-mailbox.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cmdq/bridge/cmdq-bdg-test.ko")

        mgk_64_device_modules.append("sound/soc/codecs/snd-soc-rt5509.ko")

        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/adsp:adsp".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/adsp/v1:adsp-v1".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/adsp/v2:adsp-v2".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/adsp/v3:adsp-v3".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/audio_ipi:audio_ipi".format(kernel_version))
        mgk_64_device_modules.remove("drivers/misc/mediatek/usb/usb_offload/usb_offload.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/sound/soc/mediatek/audio_dsp:mtk-soc-offload-common".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/sound/soc/mediatek/audio_dsp:snd-soc-audiodsp-common".format(kernel_version),)
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/sound/soc/mediatek/ultrasound/ultra_common:mtk-scp-ultra".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/sound/soc/mediatek/ultrasound/ultra_scp:snd-soc-mtk-scp-ultra".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/vow/ver02:mtk-vow".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/sound/soc/mediatek/vow:mtk-scp-vow".format(kernel_version))

        #mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6985/mt6985-mt6338.ko")
        #mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6985/snd-soc-mt6985-afe.ko")

        mgk_64_device_modules.append("drivers/misc/mediatek/cameraisp/dip/isp_6s/camera_dip_isp6s.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cameraisp/mfb/camera_mfb_isp6s.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cameraisp/dpe/camera_dpe_isp60.ko")

    if "mt6853_overlay.config" in DEFCONFIG_OVERLAYS:
        mgk_64_kleaf_eng_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase/slbc:ktf_slbc_it")
        mgk_64_kleaf_userdebug_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase/slbc:ktf_slbc_it")
        mgk_64_kleaf_platform_modules.pop("//vendor/mediatek/kernel_modules/gpu:gpu_mt6895")
        mgk_64_kleaf_modules.append("//vendor/mediatek/kernel_modules/gpu:gpu_mt6853")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/mtkcam/sched:c2ps")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/mtkcam/sched:c2ps_perf_ioctl")
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

        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/trusted_mem:ffa".format(kernel_version))
        mgk_64_device_modules.remove("drivers/tee/teei/510/isee-ffa.ko")

        mgk_64_device_modules.append("drivers/regulator/mt6362-regulator.ko")
        mgk_64_device_modules.append("drivers/leds/leds-mt6362.ko")
        mgk_64_device_modules.append("drivers/mfd/mt6362-core.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/iommu/arm/arm-smmu-v3:arm_smmu_v3".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/iommu/arm/arm-smmu-v3:mtk-smmuv3-pmu".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/iommu/arm/arm-smmu-v3:mtk-smmuv3-mpam-mon".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/iommu:smmu_secure".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/iommu:mtk_smmu_qos".format(kernel_version))

        mgk_64_device_modules.append("drivers/misc/mediatek/sensor/2.0/mtk_nanohub/nanohub.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/sensor/2.0/sensorhub/sensorhub.ko")

        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/vcp/rv:vcp".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/vcp/rv:vcp_status".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/vdec_fmt:vdec-fmt".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/media/platform/mtk-vcodec:mtk-vcodec-dec-v2".format(kernel_version))
        mgk_64_device_modules.remove("drivers/media/platform/mtk-vcodec/mtk-vcodec-enc-v2.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/soc/mediatek:mtk-mmdvfs-v3".format(kernel_version))
        mgk_64_kleaf_device_modules.append("//kernel_device_modules-{}/drivers/soc/mediatek/devmpu:devmpu".format(kernel_version))

        mgk_64_device_modules.remove("drivers/misc/mediatek/pkvm_tmem/pkvm_tmem.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/sspm/v2/sspm.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/sspm/v3/sspm_v3.ko")
        #mgk_64_platform_device_modules.pop("drivers/misc/mediatek/lpm/modules/debug/mt6989/mtk-lpm-dbg-mt6989.ko")
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/lpm/modules/debug/mt6991:mtk-lpm-dbg-mt6991".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/mbraink/bridge:mtk_mbraink_bridge".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/mbraink/perf:mtk_mbraink_perf".format(kernel_version))
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/mtk_mbraink.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/modules/v6989/mtk_mbraink_v6989.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/modules/v6991/mtk_mbraink_v6991.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/modules/v6899/mtk_mbraink_v6899.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/modules/v6993/mtk_mbraink_v6993.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/pmsr:pmsr".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/pmsr/v2:pmsr_v2".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/pmsr/v3:pmsr_v3".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/pmsr/v4:pmsr_v4".format(kernel_version))

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

        mgk_64_kleaf_userdebug_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase/mml:ktf_mml_ait")
        mgk_64_kleaf_eng_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase/mml:ktf_mml_ait")
        mgk_64_kleaf_userdebug_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase/display_mobile_wbgai:ktf_display_mobile_wbgai")
        mgk_64_kleaf_userdebug_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase/display_pq_wbgai:ktf_display_pq_wbgai")
        mgk_64_kleaf_eng_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase/display_mobile_wbgai:ktf_display_mobile_wbgai")
        mgk_64_kleaf_eng_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase/display_pq_wbgai:ktf_display_pq_wbgai")

        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/gpu/drm/mediatek/dpc/dpc_v1:mtk_dpc_v1".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/gpu/drm/mediatek/dpc/dpc_v1:mtk_vdisp_v1".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/gpu/drm/mediatek/dpc/dpc_v2:mtk_dpc_v2".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/gpu/drm/mediatek/dpc/dpc_v2:mtk_vdisp_v2".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/gpu/drm/mediatek/dpc/dpc_v3:mtk_dpc_v3".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/gpu/drm/mediatek/dpc/dpc_v3:mtk_vdisp_v3".format(kernel_version))

        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug/v1:mtk-swpm-dbg-common-v1".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug/v1/subsys:mtk-swpm-isp-wrapper".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug/v6991/subsys:mtk-swpm-audio-dbg-v6991".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug/v6991/subsys:mtk-swpm-disp-dbg-v6991".format(kernel_version))
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
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug:mtk-swpm-dbg-v6991".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug:mtk-swpm-dbg-v6993".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm:mtk-swpm".format(kernel_version))
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
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug/v6991/subsys:mtk-swpm-isp-dbg-v6991".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug/v6993/subsys:mtk-swpm-disp-dbg-v6993".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug/v6993/subsys:mtk-swpm-isp-dbg-v6993".format(kernel_version))

        mgk_64_device_modules.remove("drivers/misc/mediatek/imgsensor/src/isp6s/imgsensor_isp6s.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/imgsensor/src/isp6s_mou/imgsensor_isp6s_mou.ko")
        # mgk_64_device_modules.remove("drivers/misc/mediatek/cam_cal/src/custom/camera_eeprom.ko")
        # mgk_64_device_modules.append("drivers/misc/mediatek/cam_cal/src/isp6s_mou/camera_eeprom_isp6s_mou.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/cam_cal/src:camera_eeprom".format(kernel_version))
        mgk_64_kleaf_device_modules.append("//kernel_device_modules-{}/drivers/misc/mediatek/cam_cal/src:camera_eeprom_isp6s_mou".format(kernel_version))
        mgk_64_device_modules.append("drivers/misc/mediatek/cameraisp/dip/isp_6s/camera_dip_isp6s.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cameraisp/mfb/camera_mfb_isp6s.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cameraisp/dpe/camera_dpe_isp60.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cameraisp/wpe/isp_6s/camera_wpe_isp6s.ko")

        mgk_64_device_modules.remove("drivers/misc/mediatek/tinysys_scmi/tinysys-scmi.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:mtk_cm_mgr".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:mtk_cm_ipi".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:mtk_cm_mgr_mt6886".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:mtk_cm_mgr_mt6897".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:mtk_cm_mgr_mt6983".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:mtk_cm_mgr_mt6985".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:mtk_cm_mgr_mt6989".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:mtk_cm_mgr_mt6991".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:mtk_cm_mgr_mt6993".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/mminfra:mtk-mminfra-debug".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/mminfra:mtk-mminfra-imax".format(kernel_version))
        mgk_64_device_modules.remove("drivers/misc/mediatek/ssc/debug/v1/mtk-ssc-dbg-v1.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/ssc/debug/v2/mtk-ssc-dbg-v2.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/ssc/mtk-ssc.ko")

        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/slbc:mmsram".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/slbc:mtk_slbc".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/slbc:slbc_ipi".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/slbc:slbc_trace".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/slbc:slbc_mt6895".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/slbc:slbc_mt6991".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/slbc:slbc_mt6993".format(kernel_version))
        mgk_64_kleaf_device_modules.append("//kernel_device_modules-{}/drivers/gpu/mediatek/gpufreq/v2_legacy:mtk_gpufreq_wrapper_legacy".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/gpu/mediatek/gpueb:mtk_gpueb".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/gpueb:mtk_ghpm_mt6991".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/gpueb:mtk_ghpm_mt6993".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/gpu/mediatek/gpufreq/v2:mtk_gpufreq_wrapper".format(kernel_version))
        #mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6886.ko")
        #mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6897.ko")
        #mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6899.ko")
        #mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6985.ko")
        #mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6989.ko")
        #mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6989_fpga.ko")
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/gpufreq/v2:mtk_gpufreq_mt6991".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/gpufreq/v2:mtk_gpufreq_mt6993".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/gpufreq/v2:mtk_gpufreq_mt6855".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/gpufreq/v2:mtk_gpufreq_mt6895".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/ged:mtk_ged_mt6991".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/ged:mtk_ged_mt6993".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/gpu_pdma:mtk_gpu_pdma_mt6991".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/gpu_pdma:mtk_gpu_pdma_mt6993".format(kernel_version))
        mgk_64_device_modules.remove("drivers/misc/mediatek/performance/mtk_perf_ioctl_magt.ko")
        #mgk_64_platform_device_modules.update({"drivers/misc/mediatek/cm_mgr_legacy_v1/mtk_cm_mgr_mt6853.ko":"mt6853"})
        mgk_64_device_modules.append("drivers/misc/mediatek/cm_mgr_legacy_v1/mtk_cm_mgr.ko")
        mgk_64_device_modules.append("sound/soc/mediatek/mt6853/snd-soc-mt6853-afe.ko")
        mgk_64_device_modules.append("sound/soc/mediatek/mt6853/mt6853-mt6359p.ko")
        mgk_64_device_modules.append("sound/soc/codecs/snd-soc-mt6660.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/adsp:adsp".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/adsp/v1:adsp-v1".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/adsp/v2:adsp-v2".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/adsp/v3:adsp-v3".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/audio_ipi:audio_ipi".format(kernel_version))
        mgk_64_device_modules.remove("drivers/misc/mediatek/usb/usb_offload/usb_offload.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/sound/soc/mediatek/audio_dsp:mtk-soc-offload-common".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/sound/soc/mediatek/audio_dsp:snd-soc-audiodsp-common".format(kernel_version),)
        #mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6985/mt6985-mt6338.ko")
        #mgk_64_platform_device_modules.pop("sound/soc/mediatek/mt6985/snd-soc-mt6985-afe.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/sound/soc/mediatek/ultrasound/ultra_common:mtk-scp-ultra".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/sound/soc/mediatek/ultrasound/ultra_scp:snd-soc-mtk-scp-ultra".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/vow/ver02:mtk-vow".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/sound/soc/mediatek/vow:mtk-scp-vow".format(kernel_version))

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
        mgk_64_kleaf_eng_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase/slbc:ktf_slbc_it")
        mgk_64_kleaf_userdebug_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase/slbc:ktf_slbc_it")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/mtkcam/sched:c2ps")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/mtkcam/sched:c2ps_perf_ioctl")
        mgk_64_device_modules.append("drivers/regulator/mt6358-regulator.ko")
        mgk_64_device_modules.append("drivers/regulator/mtk-pmic-oc-debug.ko")
        mgk_64_device_modules.append("//kernel_device_modules-{}/drivers/misc/mediatek/dvfsrc:dvfsrc-opp-mt6781".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/mbraink/bridge:mtk_mbraink_bridge".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/mbraink/perf:mtk_mbraink_perf".format(kernel_version))
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/mtk_mbraink.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/modules/v6989/mtk_mbraink_v6989.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/modules/v6991/mtk_mbraink_v6991.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/modules/v6899/mtk_mbraink_v6899.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/modules/v6993/mtk_mbraink_v6993.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/spmi:spmi-mtk-mpu".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/spmi:spmi-mtk-pmif".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/gpu/drm/mediatek/dpc/dpc_v1:mtk_dpc_v1".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/gpu/drm/mediatek/dpc/dpc_v1:mtk_vdisp_v1".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/gpu/drm/mediatek/dpc/dpc_v2:mtk_dpc_v2".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/gpu/drm/mediatek/dpc/dpc_v2:mtk_vdisp_v2".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/gpu/drm/mediatek/dpc/dpc_v3:mtk_dpc_v3".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/gpu/drm/mediatek/dpc/dpc_v3:mtk_vdisp_v3".format(kernel_version))
        mgk_64_device_modules.append("drivers/misc/mediatek/sspm/v2/sspm.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/sspm/v3/sspm_v3.ko")
        #mgk_64_platform_device_modules.pop("drivers/misc/mediatek/lpm/modules/debug/mt6989/mtk-lpm-dbg-mt6989.ko")
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/lpm/modules/debug/mt6991:mtk-lpm-dbg-mt6991".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/pmsr:pmsr".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/pmsr/v2:pmsr_v2".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/pmsr/v3:pmsr_v3".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/pmsr/v4:pmsr_v4".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug/v1:mtk-swpm-dbg-common-v1".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug/v1/subsys:mtk-swpm-isp-wrapper".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug/v6991/subsys:mtk-swpm-audio-dbg-v6991".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug/v6991/subsys:mtk-swpm-disp-dbg-v6991".format(kernel_version))
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
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug:mtk-swpm-dbg-v6991".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug:mtk-swpm-dbg-v6993".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm:mtk-swpm-perf-arm-pmu".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm:mtk-swpm".format(kernel_version))
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
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug/v6991/subsys:mtk-swpm-isp-dbg-v6991".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug/v6993/subsys:mtk-swpm-disp-dbg-v6993".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/swpm/modules/debug/v6993/subsys:mtk-swpm-isp-dbg-v6993".format(kernel_version))

        mgk_64_device_modules.remove("drivers/misc/mediatek/imgsensor/src/isp6s/imgsensor_isp6s.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/imgsensor/src/isp6s_lag/imgsensor_isp6s_lag.ko")
        # mgk_64_device_modules.remove("drivers/misc/mediatek/cam_cal/src/custom/camera_eeprom.ko")
        # mgk_64_device_modules.append("drivers/misc/mediatek/cam_cal/src/isp6s_lag/camera_eeprom_isp6s_lag.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/cam_cal/src:camera_eeprom".format(kernel_version))
        mgk_64_kleaf_device_modules.append("//kernel_device_modules-{}/drivers/misc/mediatek/cam_cal/src:camera_eeprom_isp6s_lag".format(kernel_version))
        mgk_64_device_modules.append("drivers/misc/mediatek/cameraisp/fdvt/camera_fdvt_isp51_v1.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cameraisp/rsc/camera_rsc_isp6s_v1.ko")

        mgk_64_device_modules.remove("drivers/misc/mediatek/tinysys_scmi/tinysys-scmi.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:mtk_cm_mgr".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:mtk_cm_ipi".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:mtk_cm_mgr_mt6886".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:mtk_cm_mgr_mt6897".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:mtk_cm_mgr_mt6983".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:mtk_cm_mgr_mt6985".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:mtk_cm_mgr_mt6989".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:mtk_cm_mgr_mt6991".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:mtk_cm_mgr_mt6993".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/mminfra:mtk-mminfra-debug".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/mminfra:mtk-mminfra-imax".format(kernel_version))
        mgk_64_device_modules.remove("drivers/misc/mediatek/ssc/debug/v1/mtk-ssc-dbg-v1.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/ssc/debug/v2/mtk-ssc-dbg-v2.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/ssc/mtk-ssc.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/slbc:mmsram".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/slbc:mtk_slbc".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/slbc:slbc_ipi".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/slbc:slbc_trace".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/slbc:slbc_mt6895".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/slbc:slbc_mt6991".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/slbc:slbc_mt6993".format(kernel_version))
        #mgk_64_platform_device_modules.update({"drivers/misc/mediatek/cm_mgr_legacy_v1/mtk_cm_mgr_mt6781.ko":"mt6781"})
        mgk_64_device_modules.append("drivers/misc/mediatek/cm_mgr_legacy_v1/mtk_cm_mgr.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cameraisp/dip/isp_6s/camera_dip_isp6s.ko")
        mgk_64_device_modules.append("drivers/misc/mediatek/cameraisp/mfb/camera_mfb_isp6s.ko")

    if "mt8786p2_overlay.config" in DEFCONFIG_OVERLAYS:
        mgk_64_device_modules.append("drivers/video/backlight/sgm37604a.ko")

    if "entry_level_5g.config" in DEFCONFIG_OVERLAYS:
        mgk_64_kleaf_device_modules.append("//kernel_device_modules-{}/drivers/misc/mediatek/lpm_legacy:mtk-lpm-legacy".format(kernel_version))
        mgk_64_kleaf_device_modules.append("//kernel_device_modules-{}/drivers/misc/mediatek/lpm_legacy/modules/debug:mtk-lpm-dbg-common-v1-legacy".format(kernel_version))
        mgk_64_kleaf_device_modules.append("//kernel_device_modules-{}/drivers/misc/mediatek/lpm_legacy/modules/platform/v1:mtk-lpm-plat-v1-legacy".format(kernel_version))
        mgk_64_kleaf_device_modules.append("//kernel_device_modules-{}/drivers/misc/mediatek/lpm_legacy/modules/debug/k6855:mtk-lpm-dbg-mt6855-legacy".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/iommu/arm/arm-smmu-v3:arm_smmu_v3".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/iommu/arm/arm-smmu-v3:mtk-smmuv3-pmu".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/iommu/arm/arm-smmu-v3:mtk-smmuv3-mpam-mon".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/iommu:mtk_smmu_qos".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/iommu:smmu_secure".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/jpeg:jpeg-driver".format(kernel_version))

        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/mminfra:mtk-mminfra-debug".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/mminfra:mtk-mminfra-imax".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/mminfra:mtk-mminfra-util".format(kernel_version))

        #mgk_64_platform_device_modules.pop("drivers/clk/mediatek/clk-chk-mt6989.ko")
        #mgk_64_platform_device_modules.pop("drivers/clk/mediatek/pd-chk-mt6989.ko")
        #mgk_64_platform_device_modules.pop("drivers/clk/mediatek/clk-chk-mt6991.ko")
        #mgk_64_platform_device_modules.pop("drivers/clk/mediatek/pd-chk-mt6991.ko")


        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:mtk_cm_mgr".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:mtk_cm_mgr_mt6991".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:mtk_cm_mgr_mt6993".format(kernel_version))
        mgk_64_kleaf_device_modules.append("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr_legacy_v1:mtk_cm_mgr_legacy_v1".format(kernel_version))
        mgk_64_kleaf_device_modules.append("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr_legacy_v1:mtk_cm_ipi_legacy_v1".format(kernel_version))
        mgk_64_kleaf_platform_modules.update({"//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr_legacy_v1:mtk_cm_mgr_mt6855".format(kernel_version): "mt6855"})

        mgk_64_kleaf_eng_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase/mmdvfs:ktf_mmdvfs")
        mgk_64_kleaf_userdebug_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase/mmdvfs:ktf_mmdvfs")
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/soc/mediatek/mmdvfs:mmdvfs-mt6993".format(kernel_version))

        mgk_64_kleaf_userdebug_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase/mml:ktf_mml_ait")
        mgk_64_kleaf_eng_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase/mml:ktf_mml_ait")
        mgk_64_kleaf_userdebug_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase/display_mobile_wbgai:ktf_display_mobile_wbgai")
        mgk_64_kleaf_userdebug_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase/display_pq_wbgai:ktf_display_pq_wbgai")
        mgk_64_kleaf_eng_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase/display_mobile_wbgai:ktf_display_mobile_wbgai")
        mgk_64_kleaf_eng_modules.remove("//vendor/mediatek/tests/kernel/ktf_testcase/display_pq_wbgai:ktf_display_pq_wbgai")

        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/memory/mediatek:emi-fake-eng-v2".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/memory/mediatek:slc-parity".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/memory/mediatek:smpu-hook-v1".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/memory/mediatek:smpu".format(kernel_version))
        mgk_64_kleaf_device_modules.append("//kernel_device_modules-{}/drivers/memory/mediatek:emi-mpu-hook-v1".format(kernel_version))
        mgk_64_kleaf_device_modules.append("//kernel_device_modules-{}/drivers/memory/mediatek:emi-mpu-v2".format(kernel_version))

        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/gpu/drm/mediatek/dpc/dpc_v1:mtk_dpc_v1".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/gpu/drm/mediatek/dpc/dpc_v1:mtk_vdisp_v1".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/gpu/drm/mediatek/dpc/dpc_v2:mtk_dpc_v2".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/gpu/drm/mediatek/dpc/dpc_v2:mtk_vdisp_v2".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/gpu/drm/mediatek/dpc/dpc_v3:mtk_dpc_v3".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/gpu/drm/mediatek/dpc/dpc_v3:mtk_vdisp_v3".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//vendor/mediatek/kernel_modules/gpu:gpu_mt6895")
        mgk_64_kleaf_modules.append("//vendor/mediatek/kernel_modules/gpu:gpu_mt6855")
        mgk_64_kleaf_device_modules.append("//kernel_device_modules-{}/drivers/misc/mediatek/conn_md:conn_md_drv".format(kernel_version))

        #mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/gpu/mediatek/gpueb:mtk_gpueb".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/gpueb:mtk_ghpm_mt6991".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/gpueb:mtk_ghpm_mt6993".format(kernel_version))

        #mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6886.ko")
        #mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6897.ko")
        #mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6899.ko")
        #mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6985.ko")
        #mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6989.ko")
        #mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpufreq/v2/mtk_gpufreq_mt6989_fpga.ko")
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/gpufreq/v2:mtk_gpufreq_mt6991".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/gpufreq/v2:mtk_gpufreq_mt6993".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/gpufreq/v2:mtk_gpufreq_mt6895".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/gpu_pdma:mtk_gpu_pdma_mt6991".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/gpu_pdma:mtk_gpu_pdma_mt6993".format(kernel_version))

        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/mtkcam/sched:c2ps")
        mgk_64_kleaf_modules.remove("//vendor/mediatek/kernel_modules/mtkcam/sched:c2ps_perf_ioctl")
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/ged:mtk_ged_mt6991".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/ged:mtk_ged_mt6993".format(kernel_version))
        #mgk_64_platform_device_modules.pop("drivers/gpu/mediatek/gpu_iommu/mtk_gpu_iommu_mt6989.ko")
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/gpu_iommu:mtk_gpu_iommu_mt6991".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/gpu/mediatek/gpu_iommu:mtk_gpu_iommu_mt6993".format(kernel_version))

        #mgk_64_device_modules.append("drivers/misc/mediatek/lpm_legacy/mtk-lpm-legacy.ko")
        #mgk_64_device_modules.append("drivers/misc/mediatek/lpm_legacy/modules/platform/v1/mtk-lpm-plat-v1-legacy.ko")
        #mgk_64_device_modules.append("drivers/misc/mediatek/lpm_legacy/modules/debug/k6855/mtk-lpm-dbg-mt6855-legacy.ko")
        #mgk_64_device_modules.append("drivers/misc/mediatek/lpm_legacy/modules/debug/v1/mtk-lpm-dbg-common-v1-legacy.ko")

        mgk_64_common_eng_modules.remove("drivers/firmware/arm_ffa/ffa-core.ko")
        mgk_64_common_userdebug_modules.remove("drivers/firmware/arm_ffa/ffa-core.ko")
        mgk_64_common_user_modules.remove("drivers/firmware/arm_ffa/ffa-core.ko")
        mgk_64_common_eng_modules.remove("drivers/firmware/arm_ffa/ffa-module.ko")
        mgk_64_common_userdebug_modules.remove("drivers/firmware/arm_ffa/ffa-module.ko")
        mgk_64_common_user_modules.remove("drivers/firmware/arm_ffa/ffa-module.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/trusted_mem:ffa".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/tee/gud:mcDrvModule-ffa".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/tee/teei/520:isee-ffa".format(kernel_version))

        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/trusty:trusty-core".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/trusty:trusty-ipc".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/trusty:trusty-log".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/trusty:trusty-test".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/trusty:trusty-virtio".format(kernel_version))

    if "entry_level_legacy.config" in DEFCONFIG_OVERLAYS:
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/modules/v6989/mtk_mbraink_v6989.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/modules/v6991/mtk_mbraink_v6991.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/modules/v6899/mtk_mbraink_v6899.ko")
        mgk_64_device_modules.remove("drivers/misc/mediatek/mbraink/modules/v6993/mtk_mbraink_v6993.ko")

        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:mtk_cm_mgr".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:mtk_cm_mgr_mt6991".format(kernel_version))
        mgk_64_kleaf_platform_modules.pop("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr:mtk_cm_mgr_mt6993".format(kernel_version))
        mgk_64_kleaf_device_modules.append("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr_legacy_v1:mtk_cm_mgr_legacy_v1".format(kernel_version))
        mgk_64_kleaf_device_modules.append("//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr_legacy_v1:mtk_cm_ipi_legacy_v1".format(kernel_version))
        mgk_64_kleaf_platform_modules.update({"//kernel_device_modules-{}/drivers/misc/mediatek/cm_mgr_legacy_v1:mtk_cm_mgr_mt6895".format(kernel_version): "mt6895"})

        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/memory/mediatek:emi-fake-eng-v2".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/memory/mediatek:slc-parity".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/memory/mediatek:smpu-hook-v1".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/memory/mediatek:smpu".format(kernel_version))
        mgk_64_kleaf_device_modules.append("//kernel_device_modules-{}/drivers/memory/mediatek:emi-mpu-hook-v1".format(kernel_version))
        mgk_64_kleaf_device_modules.append("//kernel_device_modules-{}/drivers/memory/mediatek:emi-mpu-v2".format(kernel_version))

        mgk_64_common_eng_modules.remove("drivers/firmware/arm_ffa/ffa-core.ko")
        mgk_64_common_userdebug_modules.remove("drivers/firmware/arm_ffa/ffa-core.ko")
        mgk_64_common_user_modules.remove("drivers/firmware/arm_ffa/ffa-core.ko")
        mgk_64_common_eng_modules.remove("drivers/firmware/arm_ffa/ffa-module.ko")
        mgk_64_common_userdebug_modules.remove("drivers/firmware/arm_ffa/ffa-module.ko")
        mgk_64_common_user_modules.remove("drivers/firmware/arm_ffa/ffa-module.ko")
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/misc/mediatek/trusted_mem:ffa".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/tee/gud:mcDrvModule-ffa".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/tee/teei/520:isee-ffa".format(kernel_version))

        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/trusty:trusty-core".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/trusty:trusty-ipc".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/trusty:trusty-log".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/trusty:trusty-test".format(kernel_version))
        mgk_64_kleaf_device_modules.remove("//kernel_device_modules-{}/drivers/trusty:trusty-virtio".format(kernel_version))

get_overlay_modules_list()
