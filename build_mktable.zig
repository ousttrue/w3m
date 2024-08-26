const std = @import("std");

pub fn build(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
) *std.Build.Step.Compile {
    const exe = b.addExecutable(.{
        .target = target,
        .optimize = optimize,
        .name = "mkTable",
        .link_libc = true,
    });
    exe.addCSourceFile(.{
        .file = b.path("entity.c"),
        .flags = &.{
            "-DDUMMY",
        },
    });
    exe.addCSourceFiles(.{
        .files = &.{
            "mktable.c",
            // "dummy.c",
            "Str.c",
            "hash.c",
            "myctype.c",
        },
    });
    return exe;
}
