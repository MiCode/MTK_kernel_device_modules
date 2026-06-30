load("//build/bazel_mgk_rules:mgk_ddk_ko.bzl", "define_mgk_ddk_ko")
load("@mgk_info//:kernel_version.bzl","kernel_version",)

def define_platform_scpsys_ko(platform):
    define_mgk_ddk_ko(
        name = "mtk-scpsys-" + platform,

        srcs = [
            "mtk-scpsys-" + platform + ".c",
        ],

        includes = ["."],

        hdrs = native.glob(["**/*.h"]),

        ko_deps = [
            "//kernel_device_modules-{}/drivers/soc/mediatek:mtk-scpsys".format(kernel_version),
        ],
    )
