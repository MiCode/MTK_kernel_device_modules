load("//build/bazel_mgk_rules:mgk_ddk_ko.bzl", "define_mgk_ddk_ko")
load("@mgk_info//:kernel_version.bzl","kernel_version",)

def define_platform_clkmgr_ko(platform, subsys):
    if subsys:
        define_mgk_ddk_ko(
            name = "clk-" + platform + "-" + subsys,
            srcs = [
                "clk-" + platform + "-" + subsys + ".c",
            ],
            ko_deps = [
                "//kernel_device_modules-{}/drivers/clk/mediatek:clk-common".format(kernel_version),
            ],
            includes = ["."],
            hdrs = native.glob(["**/*.h"]),
        )
    else:
        define_mgk_ddk_ko(
            name = "clk-" + platform,
            srcs = [
                "clk-" + platform + ".c",
            ],
            ko_deps = [
                "//kernel_device_modules-{}/drivers/clk/mediatek:clk-common".format(kernel_version),
            ],
            includes = ["."],
            hdrs = native.glob(["**/*.h"]),
        )
        define_mgk_ddk_ko(
            name = "clk-chk-" + platform,
            srcs = [
                "clkchk-" + platform + ".c",
            ],
            header_deps = [
                "//kernel_device_modules-{}/drivers/misc/mediatek/vcp/include:vcp_public_headers".format(kernel_version),
                "//kernel_device_modules-{}/drivers/misc/mediatek/vcp/rv:vcp_status_headers".format(kernel_version),
            ],
            ko_deps = [
                "//kernel_device_modules-{}/drivers/clk/mediatek:clk-common".format(kernel_version),
                "//kernel_device_modules-{}/drivers/misc/mediatek/dvfsrc:mtk-dvfsrc-helper".format(kernel_version),
                "//kernel_device_modules-{}/drivers/soc/mediatek/devapc:device-apc-common".format(kernel_version),
                "//kernel_device_modules-{}/drivers/misc/mediatek/vcp/rv:vcp_status".format(kernel_version),
            ],
            includes = ["."],
            copts = [
                "-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/",
                "-I$(DEVICE_MODULES_PATH)/include/linux/soc/mediatek/",
            ],
            hdrs = native.glob(["**/*.h"]),
        )
        define_mgk_ddk_ko(
            name = "clk-fmeter-" + platform,
            srcs = [
                 "clk-fmeter-" + platform + ".c",
            ],
            ko_deps = [
                "//kernel_device_modules-{}/drivers/clk/mediatek:clk-common".format(kernel_version),
            ],
            includes = ["."],
            hdrs = native.glob(["**/*.h"]),
        )
        define_mgk_ddk_ko(
            name = "clk-dbg-" + platform,
            srcs = [
                "clkdbg-" + platform + ".c",
            ],
            ko_deps = [
                "//kernel_device_modules-{}/drivers/clk/mediatek:clk-common".format(kernel_version),
            ],
            includes = ["."],
            hdrs = native.glob(["**/*.h"]),
        )
        define_mgk_ddk_ko(
            name = "pd-chk-" + platform,
            srcs = [
                "mtk-pd-chk-" + platform + ".c",
            ],
            header_deps = [
                "//kernel_device_modules-{}/drivers/misc/mediatek/vcp/include:vcp_public_headers".format(kernel_version),
                "//kernel_device_modules-{}/drivers/misc/mediatek/vcp/rv:vcp_status_headers".format(kernel_version),
                "//kernel_device_modules-{}/drivers/soc/mediatek:soc_mediatek_headers".format(kernel_version),
            ],
            ko_deps = [
                "//kernel_device_modules-{}/drivers/clk/mediatek:clk-common".format(kernel_version),
                "//kernel_device_modules-{}/drivers/clk/mediatek:clk-chk-{}".format(kernel_version, platform),
            ],
            includes = ["."],
            hdrs = native.glob(["**/*.h"]),
        )

def define_v1_platform_clkmgr_ko(platform, subsys):
    if subsys:
        define_mgk_ddk_ko(
            name = "clk-" + platform + "-" + subsys,
            srcs = [
                "clk-" + platform + "-" + subsys + ".c",
            ],
            includes = ["."],
            hdrs = native.glob(["**/*.h"]),
            ko_deps = [
                "//kernel_device_modules-{}/drivers/clk/mediatek:clk-common".format(kernel_version),
                "//kernel_device_modules-{}/drivers/misc/mediatek/aee/aed:aee_aed".format(kernel_version),
            ],
        )
    else:
        define_mgk_ddk_ko(
            name = "clk-" + platform,
            srcs = [
                "clk-" + platform + ".c",
            ],
            ko_deps = [
                "//kernel_device_modules-{}/drivers/clk/mediatek:clk-common".format(kernel_version),
            ],
            includes = ["."],
            hdrs = native.glob(["**/*.h"]),
        )
        define_mgk_ddk_ko(
            name = "clk-chk-" + platform,
            srcs = [
                "clkchk-" + platform + ".c",
            ],
            header_deps = [
                "//kernel_device_modules-{}/drivers/misc/mediatek/vcp/include:vcp_public_headers".format(kernel_version),
            ],
           includes = ["."],
           copts = ["-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/vcp/include/",
                    "-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/",
                ],
           hdrs = native.glob(["**/*.h"]),
           ko_deps = [
                "//kernel_device_modules-{}/drivers/clk/mediatek:clk-common".format(kernel_version),
                "//kernel_device_modules-{}/drivers/misc/mediatek/aee/aed:aee_aed".format(kernel_version),
                "//kernel_device_modules-{}/drivers/misc/mediatek/vcp/rv_v2:vcp_status_v2".format(kernel_version),
                "//kernel_device_modules-{}/drivers/soc/mediatek/devapc:device-apc-common".format(kernel_version),
            ],
        )
        define_mgk_ddk_ko(
            name = "clk-fmeter-" + platform,
            srcs = [
                 "clk-fmeter-" + platform + ".c",
            ],
            includes = ["."],
            hdrs = native.glob(["**/*.h"]),
            ko_deps = [
                 "//kernel_device_modules-{}/drivers/clk/mediatek:clk-common".format(kernel_version),
                 "//kernel_device_modules-{}/drivers/misc/mediatek/aee/aed:aee_aed".format(kernel_version),
            ],
        )
        define_mgk_ddk_ko(
            name = "clk-dbg-" + platform,
            srcs = [
                "clkdbg-" + platform + ".c",
            ],
            includes = ["."],
            hdrs = native.glob(["**/*.h"]),
            ko_deps = [
                 "//kernel_device_modules-{}/drivers/clk/mediatek:clk-common".format(kernel_version),
                 "//kernel_device_modules-{}/drivers/misc/mediatek/aee/aed:aee_aed".format(kernel_version),
            ],
        )
        define_mgk_ddk_ko(
            name = "pd-chk-" + platform,
            srcs = [
                "mtk-pd-chk-" + platform + ".c",
            ],
           includes = ["."],
           hdrs = native.glob(["**/*.h"]),
           ko_deps = [
                "//kernel_device_modules-{}/drivers/clk/mediatek:clk-common".format(kernel_version),
                "//kernel_device_modules-{}/drivers/misc/mediatek/aee/aed:aee_aed".format(kernel_version),
                "//kernel_device_modules-{}/drivers/misc/mediatek/vcp/rv_v2:vcp_status_v2".format(kernel_version),
                "//kernel_device_modules-{}/drivers/soc/mediatek/devapc:device-apc-common".format(kernel_version),
                "//kernel_device_modules-{}/drivers/clk/mediatek:clk-chk-{}".format(kernel_version, platform),
            ],
        )

def define_v2_platform_clkmgr_ko(platform, subsys):
    if subsys:
        define_mgk_ddk_ko(
            name = "clk-" + platform + "-" + subsys,
            srcs = [
                "clk-" + platform + "-" + subsys + ".c",
            ],
            includes = ["."],
            hdrs = native.glob(["**/*.h"]),
            ko_deps = [
                "//kernel_device_modules-{}/drivers/clk/mediatek:clk-common".format(kernel_version),
                "//kernel_device_modules-{}/drivers/misc/mediatek/aee/aed:aee_aed".format(kernel_version),
            ],
        )
    else:
        define_mgk_ddk_ko(
            name = "clk-" + platform,
            srcs = [
                "clk-" + platform + ".c",
            ],
            ko_deps = [
                "//kernel_device_modules-{}/drivers/clk/mediatek:clk-common".format(kernel_version),
            ],
            includes = ["."],
            hdrs = native.glob(["**/*.h"]),
        )
        define_mgk_ddk_ko(
            name = "clk-chk-" + platform,
            srcs = [
                "clkchk-" + platform + ".c",
            ],
            header_deps = [
                "//kernel_device_modules-{}/drivers/misc/mediatek/vcp/include:vcp_public_headers".format(kernel_version),
                "//kernel_device_modules-{}/drivers/misc/mediatek/vcp/rv:vcp_status_headers".format(kernel_version),
            ],
           includes = ["."],
           copts = ["-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/vcp/include/",
                    "-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/",
                ],
           hdrs = native.glob(["**/*.h"]),
           ko_deps = [
                "//kernel_device_modules-{}/drivers/clk/mediatek:clk-common".format(kernel_version),
                "//kernel_device_modules-{}/drivers/misc/mediatek/aee/aed:aee_aed".format(kernel_version),
                "//kernel_device_modules-{}/drivers/misc/mediatek/vcp/rv_v2:vcp_status_v2".format(kernel_version),
                "//kernel_device_modules-{}/drivers/soc/mediatek/devapc:device-apc-common".format(kernel_version),
                "//kernel_device_modules-{}/drivers/misc/mediatek/vcp/rv:vcp_status".format(kernel_version),
            ],
        )
        define_mgk_ddk_ko(
            name = "clk-fmeter-" + platform,
            srcs = [
                 "clk-fmeter-" + platform + ".c",
            ],
            includes = ["."],
            hdrs = native.glob(["**/*.h"]),
            ko_deps = [
                 "//kernel_device_modules-{}/drivers/clk/mediatek:clk-common".format(kernel_version),
                 "//kernel_device_modules-{}/drivers/misc/mediatek/aee/aed:aee_aed".format(kernel_version),
            ],
        )
        define_mgk_ddk_ko(
            name = "clk-dbg-" + platform,
            srcs = [
                "clkdbg-" + platform + ".c",
            ],
            includes = ["."],
            hdrs = native.glob(["**/*.h"]),
            ko_deps = [
                 "//kernel_device_modules-{}/drivers/clk/mediatek:clk-common".format(kernel_version),
                 "//kernel_device_modules-{}/drivers/misc/mediatek/aee/aed:aee_aed".format(kernel_version),
            ],
        )
        define_mgk_ddk_ko(
            name = "pd-chk-" + platform,
            srcs = [
                "mtk-pd-chk-" + platform + ".c",
            ],
            header_deps = [
                "//kernel_device_modules-{}/drivers/misc/mediatek/vcp/include:vcp_public_headers".format(kernel_version),
                "//kernel_device_modules-{}/drivers/misc/mediatek/vcp/rv:vcp_status_headers".format(kernel_version),
                "//kernel_device_modules-{}/drivers/soc/mediatek:soc_mediatek_headers".format(kernel_version),
            ],
           includes = ["."],
           hdrs = native.glob(["**/*.h"]),
           ko_deps = [
                "//kernel_device_modules-{}/drivers/clk/mediatek:clk-common".format(kernel_version),
                "//kernel_device_modules-{}/drivers/misc/mediatek/aee/aed:aee_aed".format(kernel_version),
                "//kernel_device_modules-{}/drivers/misc/mediatek/vcp/rv_v2:vcp_status_v2".format(kernel_version),
                "//kernel_device_modules-{}/drivers/soc/mediatek/devapc:device-apc-common".format(kernel_version),
                "//kernel_device_modules-{}/drivers/clk/mediatek:clk-chk-{}".format(kernel_version, platform),
            ],
        )
