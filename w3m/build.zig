const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const gen = b.addExecutable(.{
        .name = "global_gen",
        .root_module = b.addModule("global_gen", .{
            .root_source_file = b.path("global_header_gen.zig"),
            .target = b.graph.host,
            .link_libc = true,
        }),
    });
    gen.root_module.addIncludePath(b.path("."));
    gen.root_module.addIncludePath(b.path(".."));

    // const wc_dep = b.dependency("wc", .{
    //     .target = target,
    //     .optimize = optimize,
    // });
    // const libwc = wc_dep.artifact("wc");
    // gen.root_module.addIncludePath(libwc.getEmittedIncludeTree());

    b.installArtifact(gen);

    const mod = b.addModule("w3m", .{
        .root_source_file = b.path("lib.zig"),
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });
    mod.addIncludePath(b.path("."));
    mod.addIncludePath(b.path(".."));
    const lib = b.addLibrary(.{
        .name = "w3m",
        .root_module = mod,
        .use_llvm = true,
    });

    b.installArtifact(lib);

    // const gcstr_dep = b.dependency("gcstr", .{
    //     .target = target,
    //     .optimize = optimize,
    // });
    // const gcstr_lib = gcstr_dep.artifact("gcstr");
    // lib.root_module.linkLibrary(gcstr_lib);
    // lib.installHeadersDirectory(gcstr_lib.getEmittedIncludeTree(), "", .{});

    const test_bin = b.addTest(.{
        .root_module = mod,
    });
    const test_run = b.addRunArtifact(test_bin);
    b.step("test", "test").dependOn(&test_run.step);
}

// const global_lib = build_lib(b, target, optimize, "global");
//
// const util_lib = build_lib(b, target, optimize, "util");
//
// const keybind_lib = build_lib(b, target, optimize, "keybind");
// mod.addImport("keybind", keybind_lib.root_module);
// keybind_lib.root_module.addImport("global", global_lib.root_module);
// keybind_lib.root_module.addImport("util", util_lib.root_module);

// fn build_lib(
//     b: *std.Build,
//     target: std.Build.ResolvedTarget,
//     optimize: std.builtin.OptimizeMode,
//     comptime name: []const u8,
// ) *std.Build.Step.Compile {
//     const mod = b.addModule(name, .{
//         .target = target,
//         .optimize = optimize,
//         .root_source_file = b.path(name ++ ".zig"),
//         .link_libc = true,
//     });
//     mod.addIncludePath(b.path(""));
//     const lib = b.addLibrary(.{
//         .name = "gloabl",
//         .root_module = mod,
//         // for break point
//         .use_llvm = true,
//     });
//     return lib;
// }
