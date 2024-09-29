//
// ./configure
//   --disable-m17n
//   --disable-nls
//   --disable-rpath
//   --disable-color
//   --disable-menu
//   --disable-mouse
//   --disable-alarm
//   --disable-nntp
//   --disable-gopher
//   --disable-external-uri-loader
//   --disable-w3mmailer
//   --without-x
//   --without-imagelib
//
//   config.h
//
// make
//   entity.h
//   funcname.tab
//   funcname.c
//   funcname1.h
//   funcname2.h
//   functable.c
//   tagtable.c

//
const std = @import("std");
const build_mktable = @import("build_mktable.zig");
const build_src = @import("build_src.zig");

pub fn build(b: *std.Build) void {
    var targets = std.ArrayList(*std.Build.Step.Compile).init(b.allocator);

    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    // const SHELL = "/bin/bash";
    const PACKAGE = "w3m";
    // const VERSION = "0.5.3";
    const prefix = "/usr/local";
    const exec_prefix = prefix;
    const datarootdir = b.fmt("{s}/share", .{prefix});
    // const bindir = b.fmt("{s}/bin", .{exec_prefix});
    const datadir = datarootdir;
    const localedir = b.fmt("{s}/locale", .{datadir});
    // const libdir = b.fmt("{s}/lib", .{exec_prefix});
    // const includedir = b.fmt("{s}/include", .{prefix});
    // const infodir = b.fmt("{s}/info", .{datarootdir});
    const libexecdir = b.fmt("{s}/libexec", .{exec_prefix});
    // const localstatedir = b.fmt("{s}/var", .{prefix});
    // const mandir = b.fmt("{s}/man", .{datarootdir});
    // const oldincludedir = "/usr/include";
    // const sbindir = b.fmt("{s}/sbin", .{exec_prefix});
    // const sharedstatedir = b.fmt("{s}/com", .{prefix});
    const sysconfdir = b.fmt("{s}/etc", .{prefix});
    const CGIBIN_DIR = b.fmt("{s}/{s}/cgi-bin", .{ libexecdir, PACKAGE });
    const AUXBIN_DIR = b.fmt("{s}/{s}", .{ libexecdir, PACKAGE });
    const HELP_DIR = b.fmt("{s}/w3m", .{datarootdir});
    const RC_DIR = "~/.w3m";
    const ETC_DIR = sysconfdir;
    const CONF_DIR = b.fmt("{s}/{s}", .{ sysconfdir, PACKAGE });

    const cflags = [_][]const u8{
        "-std=gnu23",
        "-DHAVE_CONFIG_H",
        "-D_GNU_SOURCE=1",
        b.fmt("-DAUXBIN_DIR=\"{s}\"", .{AUXBIN_DIR}),
        b.fmt("-DCGIBIN_DIR=\"{s}\"", .{CGIBIN_DIR}),
        b.fmt("-DHELP_DIR=\"{s}\"", .{HELP_DIR}),
        b.fmt("-DETC_DIR=\"{s}\"", .{ETC_DIR}),
        b.fmt("-DCONF_DIR=\"{s}\"", .{CONF_DIR}),
        b.fmt("-DRC_DIR=\"{s}\"", .{RC_DIR}),
        b.fmt("-DLOCALEDIR=\"{s}\"", .{localedir}),
    };

    const exe = b.addExecutable(.{
        .target = target,
        .optimize = optimize,
        .name = "w3m",
        .link_libc = true,
        // .root_source_file = b.path("main.zig"),
    });
    const lib = b.addStaticLibrary(.{
        .target = target,
        .optimize = optimize,
        .name = "termseq",
        .root_source_file = b.path("termseq/termseq.zig"),
    });
    exe.linkLibrary(lib);
    if (target.result.os.tag == .windows) {
        exe.addCSourceFiles(.{
            .files = &(build_src.SRCS ++ build_src.SRCS_WIN32),
            .flags = &cflags,
        });
        const ssl_dep = b.dependency("openssl_prebuilt", .{});
        exe.addIncludePath(ssl_dep.path("x64/include"));
        exe.addLibraryPath(ssl_dep.path("x64/lib"));
        exe.linkSystemLibrary("libcrypto");
        exe.linkSystemLibrary("libssl");
        const installDirectory = b.addInstallDirectory(.{
            .source_dir = ssl_dep.path("x64/bin"),
            .install_dir = .{ .prefix = void{} },
            .install_subdir = "bin",
        });
        exe.step.dependOn(&installDirectory.step);

        const gc_dep = b.dependency("gc", .{
            .target = target,
            .optimize = optimize,
            .BUILD_SHARED_LIBS = false,
        });
        exe.linkLibrary(gc_dep.artifact("gc"));
    } else {
        exe.addCSourceFiles(.{
            .files = &(build_src.SRCS ++ build_src.SRCS_POSIX),
            .flags = &cflags,
        });
        const ssl_dep = b.dependency("openssl", .{
            .target = target,
            .optimize = optimize,
        });
        exe.linkLibrary(ssl_dep.artifact("openssl"));

        const gc_dep = b.dependency("gc", .{ .target = target, .optimize = optimize });
        exe.linkLibrary(gc_dep.artifact("gc"));
    }
    exe.addIncludePath(b.path("."));

    b.installArtifact(exe);
    targets.append(exe) catch @panic("OOM");

    // link libs
    const uv_dep = b.dependency("zig_uv", .{
        .target = target,
        .optimize = optimize,
    });
    exe.linkLibrary(uv_dep.artifact("zig_uv"));

    const libuv_dep = uv_dep.builder.dependency(
        "zig_uv",
        .{},
    ).builder.dependency(
        "libuv",
        .{},
    );
    exe.addIncludePath(libuv_dep.path("include"));

    const widecharwidth_dep = b.dependency("widecharwidth", .{});
    exe.addIncludePath(widecharwidth_dep.path(""));

    // run
    const run_cmd = b.addRunArtifact(exe);
    run_cmd.step.dependOn(b.getInstallStep());
    if (b.args) |args| {
        run_cmd.addArgs(args);
    }
    b.step("run", "Run the app").dependOn(&run_cmd.step);

    const exe_unit_tests = b.addTest(.{
        .root_source_file = b.path("src/main.zig"),
        .target = target,
        .optimize = optimize,
    });
    const run_exe_unit_tests = b.addRunArtifact(exe_unit_tests);
    const test_step = b.step("test", "Run unit tests");
    test_step.dependOn(&run_exe_unit_tests.step);

    // add a step called "zcc" (Compile commands DataBase) for making
    // compile_commands.json. could be named anything. cdb is just quick to type
    const zcc = @import("compile_commands");
    zcc.createStep(b, "zcc", .{
        .targets = targets.toOwnedSlice() catch @panic("OOM"),
    });
}
