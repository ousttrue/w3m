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
const std = @import("std");

pub fn build(b: *std.Build) void {
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

    const exe = b.addExecutable(.{
        .name = "w3m",
        // .root_source_file = b.path("src/main.zig"),
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });
    exe.addCSourceFiles(.{
        .files = &.{
            "keybind.c",

            "main.c",
            "file.c",
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
            "terms.c",
            "url.c",
            "ftp.c",
            "mimehead.c",
            "regex.c",
            "news.c",
            "func.c",
            "cookie.c",
            "history.c",
            "backend.c",

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
        .flags = &.{
            "-DHAVE_CONFIG_H",
            b.fmt("-DAUXBIN_DIR=\"{s}\"", .{AUXBIN_DIR}),
            b.fmt("-DCGIBIN_DIR=\"{s}\"", .{CGIBIN_DIR}),
            b.fmt("-DHELP_DIR=\"{s}\"", .{HELP_DIR}),
            b.fmt("-DETC_DIR=\"{s}\"", .{ETC_DIR}),
            b.fmt("-DCONF_DIR=\"{s}\"", .{CONF_DIR}),
            b.fmt("-DRC_DIR=\"{s}\"", .{RC_DIR}),
            b.fmt("-DLOCALEDIR=\"{s}\"", .{localedir}),
        },
    });
    exe.addIncludePath(b.path("."));
    b.installArtifact(exe);

    const gc_dep = b.dependency("gc", .{ .target = target, .optimize = optimize });
    exe.linkLibrary(gc_dep.artifact("gc"));

    const ssl_dep = b.dependency("openssl", .{ .target = target, .optimize = optimize });
    // exe.linkLibrary(ssl_dep.artifact("crypto"));
    exe.linkLibrary(ssl_dep.artifact("openssl"));

    exe.linkSystemLibrary("ncurses");

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
}
