const std = @import("std");

pub fn build(b: *std.Build) void {
    // make a list of targets that have include files and c source files
    var targets: std.ArrayList(*std.Build.Step.Compile) = .initBuffer(&.{});

    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});
    const mod = b.addModule("wc", .{
        .root_source_file = b.path("src/ces.zig"),
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });
    mod.addIncludePath(b.path("include"));
    mod.addIncludePath(b.path("include/libwc"));
    mod.addCSourceFiles(.{
        .root = b.path("src"),
        .files = &libwc_srcs,
        .flags = &.{
            "-std=c23",
            "-DHAVE_CONFIG_H",
            "-DUSE_UNICODE",
        },
    });
    const lib = b.addLibrary(.{
        .name = "wc",
        .root_module = mod,
    });
    b.installArtifact(lib);
    lib.installHeadersDirectory(b.path("include"), "", .{});
    targets.append(b.allocator, lib) catch @panic("OOM");

    // const gcstr_dep = b.dependency("gcstr", .{
    //     .target = target,
    //     .optimize = optimize,
    // });
    // const gcstr_lib = gcstr_dep.artifact("gcstr");
    // mod.linkLibrary(gcstr_lib);
    // lib.installHeadersDirectory(gcstr_lib.getEmittedIncludeTree(), "", .{});

    // const wc_test = b.addExecutable(.{
    //     .name = "wc_test",
    //     .root_module = b.addModule("wc_test", .{
    //         .target = target,
    //         .optimize = optimize,
    //         .link_libc = true,
    //         .imports = &.{.{
    //             .name = "wc",
    //             .module = mod,
    //         }},
    //     }),
    // });
    // wc_test.root_module.addIncludePath(lib.getEmittedIncludeTree());
    // wc_test.root_module.linkSystemLibrary("gc", .{});
    // wc_test.root_module.addCSourceFiles(.{
    //     .files = &.{
    //         "test.c",
    //     },
    // });
    // const wc_test_run = b.addRunArtifact(wc_test);
    // b.step("test", "test").dependOn(&wc_test_run.step);

    // add a step called "cdb" (Compile commands DataBase) for making
    // compile_commands.json. could be named anything. cdb is just quick to type
    // b.step("cdb", "cdb").dependOn(&zcc.createStep(b, targets.toOwnedSlice(b.allocator) catch @panic("OOM")).step);
}

const libwc_srcs = [_][]const u8{
    //
    "wc_output.c",
    //
    "big5.c",
    // "ces.c",
    "charset.c",
    "combining.c",
    "conv.c",
    "detect.c",
    "gb18030.c",
    "gbk.c",
    "hkscs.c",
    "hz.c",
    "iso2022.c",
    "jis.c",
    "johab.c",
    "priv.c",
    "putc.c",
    "search.c",
    "sjis.c",
    "status.c",
    "ucs.c",
    "uhc.c",
    "utf7.c",
    "utf8.c",
    "viet.c",
    "wtf.c",
};
