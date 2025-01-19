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
            ko_deps = [
                "//kernel_device_modules-{}/drivers/clk/mediatek:clk-common".format(kernel_version),
            ],
            includes = ["."],
            copts = ["-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/"],
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
            ko_deps = [
                "//kernel_device_modules-{}/drivers/clk/mediatek:clk-common".format(kernel_version),
                "//kernel_device_modules-{}/drivers/clk/mediatek:clk-chk-{}".format(kernel_version, platform),
            ],
            includes = ["."],
            hdrs = native.glob(["**/*.h"]),
        )

