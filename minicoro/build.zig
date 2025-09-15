const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const mod = b.addModule("minicoro", .{
        .target = target,
        .optimize = optimize,
    });
    const lib = b.addLibrary(.{
        .name = "minicoro",
        .root_module = mod,
    });
    b.installArtifact(lib);
    lib.linkLibC();
    lib.installHeader(b.path("minicoro.h"), "minicoro.h");
    lib.addCSourceFiles(.{
        .files = &.{
            "minicoro.c",
        },
    });
}
