def _dummy_impl(ctx):
    out = ctx.actions.declare_file(ctx.label.name)
    ctx.actions.write(
        output = out,
        content = "#include <linux/module.h>\nMODULE_LICENSE(\"GPL v2\");"
    )
    return [DefaultInfo(files = depset([out]))]

dummy = rule(
    implementation = _dummy_impl
)
