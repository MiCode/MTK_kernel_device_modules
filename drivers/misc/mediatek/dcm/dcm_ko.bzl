load("//build/bazel_mgk_rules:mgk_ddk_ko.bzl", "define_mgk_ddk_ko")
load("@mgk_info//:kernel_version.bzl","kernel_version",)

def define_platform_dcm_ko(platform):
    define_mgk_ddk_ko(
        name = platform + "_dcm",
        srcs = [
            platform + "_dcm_internal.c",
            platform + "_dcm_autogen.c"
        ],
        includes = ["include"],
        hdrs = native.glob([
            "include/" + platform + "_dcm_internal.h",
            "include/" + platform + "_dcm_autogen.h"
        ]) + [":dcm_common_headers"],
        ko_deps = [
            "//kernel_device_modules-{}/drivers/misc/mediatek/dcm:mtk_dcm".format(kernel_version),
        ],
    )
