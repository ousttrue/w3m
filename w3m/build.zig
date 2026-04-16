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

    const options = b.addOptions();
    const TaskBackend = enum {
        coroutine,
        thread,
    };
    options.addOption(TaskBackend, "task_backend", .coroutine);
    mod.addOptions("config", options);

    mod.addIncludePath(b.path("."));
    mod.addIncludePath(b.path(".."));
    const lib = b.addLibrary(.{
        .name = "w3m",
        .root_module = mod,
        .use_llvm = true,
    });

    const co = build_coroutine(b, target, optimize);
    mod.addImport("co", co);

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

fn build_coroutine(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
) *std.Build.Module {
    const coroutine_dep = b.dependency("coroutine", .{});
    const coroutine_t = b.addTranslateC(.{
        .target = target,
        .optimize = optimize,
        .root_source_file = coroutine_dep.path("coroutine.h"),
    });
    coroutine_t.addIncludePath(coroutine_dep.path(""));
    const coroutine_mod = coroutine_t.createModule();
    coroutine_mod.addCSourceFiles(.{
        .root = coroutine_dep.path(""),
        .files = &.{
            "coroutine.c",
        },
    });
    return coroutine_mod;
}
