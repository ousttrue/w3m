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
        b.fmt("-DAUXBIN_DIR=\"{s}\"", .{AUXBIN_DIR}),
        b.fmt("-DCGIBIN_DIR=\"{s}\"", .{CGIBIN_DIR}),
        b.fmt("-DHELP_DIR=\"{s}\"", .{HELP_DIR}),
        b.fmt("-DETC_DIR=\"{s}\"", .{ETC_DIR}),
        b.fmt("-DCONF_DIR=\"{s}\"", .{CONF_DIR}),
        b.fmt("-DRC_DIR=\"{s}\"", .{RC_DIR}),
        b.fmt("-DLOCALEDIR=\"{s}\"", .{localedir}),
    };

    const exe = build_exe(b, target, optimize, &cflags);
    b.installArtifact(exe);
    targets.append(exe) catch @panic("OOM");

    // const termseq = build_termseq(b, target, optimize, &cflags);
    // exe.linkLibrary(termseq);
    exe.linkSystemLibrary("ncurses");

    // link libs
    const gc_dep = b.dependency("gc", .{ .target = target, .optimize = optimize });
    exe.linkLibrary(gc_dep.artifact("gc"));
    const ssl_dep = b.dependency("openssl", .{
        .target = target,
        .optimize = optimize,
    });
    exe.linkLibrary(ssl_dep.artifact("openssl"));

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

    {
        build_termcap_entry(b, target, optimize);
        const artifact = build_termcap_print(b, target, optimize, &cflags);
        const install = b.addInstallArtifact(artifact, .{});
        b.getInstallStep().dependOn(&install.step);

        const run = b.addRunArtifact(artifact);
        run.step.dependOn(&install.step);

        b.step(
            "run-termcap_print",
            "run termcap_print",
        ).dependOn(&run.step);
    }
}

fn build_termcap_print(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    cflags: []const []const u8,
) *std.Build.Step.Compile {
    const exe = b.addExecutable(.{
        .target = target,
        .optimize = optimize,
        .name = "termcap_print",
        .link_libc = true,
    });
    exe.addCSourceFiles(.{
        .root = b.path("termseq"),
        .files = &.{
            "termcap_print.c",
            "termcap_load.c",
        },
        .flags = cflags,
    });
    exe.linkSystemLibrary("ncurses");
    return exe;
}

fn build_termseq(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    cflags: []const []const u8,
) *std.Build.Step.Compile {
    const lib = b.addStaticLibrary(.{
        .name = "termseq",
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });
    lib.addCSourceFiles(.{
        .root = b.path("termseq"),
        .files = &.{
            // "termcap.c",
            // "termcap_getentry.c",
        },
        .flags = cflags,
    });
    return lib;
}

fn build_termcap_entry(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
) void {
    const exe = b.addExecutable(.{
        .name = "termcap_entry",
        .target = target,
        .optimize = optimize,
        .root_source_file = b.path("termcap_entry.zig"),
        .link_libc = true,
    });
    const install = b.addInstallArtifact(exe, .{});

    const run = b.addRunArtifact(exe);
    run.step.dependOn(&install.step);

    b.step("run-termcap", "run termcap_entry").dependOn(&run.step);
}

fn build_exe(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    cflags: []const []const u8,
) *std.Build.Step.Compile {
    const exe = b.addExecutable(.{
        .target = target,
        .optimize = optimize,
        .name = "w3m",
        .link_libc = true,
    });
    const lib = b.addStaticLibrary(.{
        .target = target,
        .optimize = optimize,
        .name = "termseq",
        .root_source_file = b.path("termseq/termcap_interface.zig"),
    });
    exe.linkLibrary(lib);
    exe.addCSourceFiles(.{
        .files = &.{
            "keybind.c",

            "main.c",
            "defun.c",
            "file.c",
            "digest_auth.c",
            "buffer.c",
            "display.c",
            "etc.c",
            "search.c",
            "linein.c",
            "table.c",
            "local.c",
            "form.c",
            "map.c",
            "frame.c",
            "rc.c",
            "menu.c",
            "mailcap.c",
            "image.c",
            "symbol.c",
            "entity.c",

            "tty.c",
            "scr.c",
            "terms.c",
            "termseq/termcap_load.c",
            // "termseq/termcap_interface.c",

            "url.c",
            "ftp.c",
            "mimehead.c",
            "regex.c",
            "news.c",
            "func.c",
            "cookie.c",
            "history.c",

            "anchor.c",
            "parsetagx.c",
            "tagtable.c",
            "istream.c",

            "Str.c",
            "indep.c",
            "textlist.c",
            "parsetag.c",
            "myctype.c",
            "hash.c",

            "version.c",
        },
        .flags = cflags,
    });
    exe.addIncludePath(b.path("."));
    return exe;
}
