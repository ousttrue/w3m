const std = @import("std");
const zcc = @import("compile_commands");

const system_libs = [_][]const u8{
    "gpm", "ssl", "ncurses", "crypto",
};

const w3m_srcs = [_][]const u8{
    "signal_jmp.c",
    "tui.c",
    "w3m_runtime.c",
    "AlarmEvent.c",
    "keybind.c",
    "dns_order.c",
    "HttpRequest.c",
    "http_auth.c",
    "DownloadList.c",
    "Line.c",
    "mimetype.c",
    "screen.c",
    "term_entry.c",

    "main.c",
    "file.c",
    "buffer.c",
    "display.c",
    "etc.c",
    "search.c",
    "linein.c",
    "table.c",
    "local_cgi.c",
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
    "Url.c",
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

    "indep.c",
    "textlist.c",
    "parsetag.c",

    "version.c",
};

const libwc_srcs = [_][]const u8{
    "big5.c",
    "ces.c",
    "char_conv.c",
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

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const mod = b.addModule("w3m", .{
        .target = target,
        .optimize = optimize,
        .root_source_file = b.path("src/main.zig"),
        .link_libc = true,
    });
    const exe = b.addExecutable(.{
        .name = "w3m",
        .root_module = mod,
    });
    b.installArtifact(exe);

    const gcstr = build_gcstr(b, target, optimize);
    exe.linkLibrary(gcstr);

    const test_exe = b.addTest(.{
        .root_module = gcstr.root_module,
    });
    b.step("test", "gcstr test").dependOn(&b.addRunArtifact(test_exe).step);

    const lua_dep = b.dependency("zlua", .{
        .target = target,
        .optimize = optimize,
        .lang = .lua51,
    });
    exe.root_module.addImport("zlua", lua_dep.module("zlua"));

    exe.linkLibrary(buildCore(b, target, optimize));
    for (system_libs) |lib| {
        exe.linkSystemLibrary(lib);
    }

    const w3mimgdisplay = b.addExecutable(.{
        .name = "w3mimgdisplay",
        .root_module = b.addModule("w3mimgdisplay", .{
            .target = target,
            .optimize = optimize,
            .link_libc = true,
        }),
    });
    b.installArtifact(w3mimgdisplay);
    w3mimgdisplay.addCSourceFiles(.{
        .files = &.{
            "w3mimgdisplay.c",
            "w3mimg/w3mimg.c",
        },
    });
    w3mimgdisplay.addIncludePath(b.path(""));
}

fn genFuncTable(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
) std.Build.LazyPath {
    const funcname_tab = gen_funcname_tab(b);
    const funcname_gen = b.addLibrary(.{
        .name = "funcname_gen",
        .root_module = b.addModule("funcname_gen", .{
            .target = target,
        }),
    });
    funcname_gen.addCSourceFile(.{
        .file = b.path("dummy.c"),
    });
    const funcname_c = gen_funcname(b, funcname_tab.output, b.path("funcname0.awk"));
    funcname_gen.installHeader(funcname_c.output, "funcname.c");

    const funcname1_h = gen_funcname(b, funcname_tab.output, b.path("funcname1.awk"));
    funcname_gen.installHeader(funcname1_h.output, "funcname1.h");

    const funcname2_h = gen_funcname(b, funcname_tab.output, b.path("funcname2.awk"));
    funcname_gen.installHeader(funcname2_h.output, "funcname2.h");

    const funcheader_h = gen_funcname(b, funcname_tab.output, b.path("funcheader.awk"));
    funcname_gen.installHeader(funcheader_h.output, "funcheader.h");

    {
        const functable_tab = gen_funcname(b, funcname_tab.output, b.path("functable.awk"));
        const mktable = build_mktable(b, b.graph.host, .ReleaseSafe);
        mktable.addIncludePath(b.path(""));

        var run_mktable = b.addRunArtifact(mktable);
        run_mktable.addArg("100");
        run_mktable.addFileArg(functable_tab.output);
        funcname_gen.installHeader(run_mktable.captureStdOut(), "functable.c");
    }

    return funcname_gen.getEmittedIncludeTree();
}

fn buildCore(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
) *std.Build.Step.Compile {
    // const SHELL = "/bin/bash";
    const PACKAGE = "w3m";
    // const VERSION = "0.5.3";
    const prefix = "/usr";
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

    const lib = b.addLibrary(.{
        .name = "core",
        .root_module = b.addModule("core", .{
            .target = target,
            .optimize = optimize,
            .link_libc = true,
        }),
    });
    lib.addIncludePath(b.path("libwc"));
    lib.addIncludePath(b.path("."));

    const flags = [_][]const u8{
        "-Wno-implicit-int",
        "-Wno-int-conversion",
        "-DHAVE_CONFIG_H",
        b.fmt("-DAUXBIN_DIR=\"{s}\"", .{AUXBIN_DIR}),
        b.fmt("-DCGIBIN_DIR=\"{s}\"", .{CGIBIN_DIR}),
        b.fmt("-DHELP_DIR=\"{s}\"", .{HELP_DIR}),
        b.fmt("-DETC_DIR=\"{s}\"", .{ETC_DIR}),
        b.fmt("-DCONF_DIR=\"{s}\"", .{CONF_DIR}),
        b.fmt("-DRC_DIR=\"{s}\"", .{RC_DIR}),
        b.fmt("-DLOCALEDIR=\"{s}\"", .{localedir}),
    };
    lib.addCSourceFiles(.{
        .files = &w3m_srcs,
        .flags = &flags,
    });
    lib.addCSourceFiles(.{
        .root = b.path("libwc"),
        .files = &libwc_srcs,
        .flags = &.{
            "-DHAVE_CONFIG_H",
            "-DUSE_UNICODE",
        },
    });

    const include = genFuncTable(b, target);
    lib.addIncludePath(include);
    const install = b.addInstallDirectory(.{
        .source_dir = include,
        .install_dir = .header,
        .install_subdir = "",
    });

    var targets = std.ArrayListUnmanaged(*std.Build.Step.Compile){};
    targets.append(b.allocator, lib) catch @panic("OOM");
    const cdb = zcc.createStep(b, "cdb", targets.toOwnedSlice(b.allocator) catch @panic("OOM"));
    cdb.dependOn(&install.step);
    lib.step.dependOn(cdb);

    return lib;
}

fn build_gcstr(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
) *std.Build.Step.Compile {
    const lib = b.addLibrary(.{
        .name = "gcstr",
        .root_module = b.addModule("gcstr", .{
            .target = target,
            .optimize = optimize,
            .link_libc = true,
            .root_source_file = b.path("gcstr/Str.zig"),
        }),
        .linkage = .dynamic,
    });
    lib.addIncludePath(b.path("gcstr"));
    const gc_dep = b.dependency("gc", .{
        .target = target,
        .optimize = optimize,
    });
    const gc = gc_dep.artifact("gc");
    lib.linkLibrary(gc);
    lib.addCSourceFiles(.{
        .root = b.path("gcstr"),
        .files = &.{
            "gcstr.c",
            "alloc.c",
            "myctype.c",
            "myctype_table.c",
            "hash.c",
            "hash_mktable.c",
            "quote.c",
        },
    });
    lib.installHeadersDirectory(b.path("gcstr"), "gcstr", .{});
    return lib;
}

fn build_mktable(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
) *std.Build.Step.Compile {
    const mod = b.addModule("mktable", .{
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });
    const exe = b.addExecutable(.{
        .name = "mktable",
        .root_module = mod,
    });
    exe.addCSourceFiles(.{
        .files = &.{
            "mktable.c", "entity.c",
        },
        .flags = &.{
            "-DDUMMY",
        },
    });

    const gcstr = build_gcstr(b, target, optimize);
    exe.linkLibrary(gcstr);

    return exe;
}

fn gen_funcname(
    b: *std.Build,
    src: std.Build.LazyPath,
    awk_script: std.Build.LazyPath,
) struct {
    step: *std.Build.Step.Run,
    output: std.Build.LazyPath,
} {
    var sort = b.addSystemCommand(&.{"sort"});
    sort.addFileArg(src);

    var awk = b.addSystemCommand(&.{ "awk", "-f" });
    awk.addFileArg(awk_script);
    awk.setStdIn(.{
        .lazy_path = sort.captureStdOut(),
    });

    return .{
        .step = awk,
        .output = awk.captureStdOut(),
    };
}

fn gen_funcname_tab(b: *std.Build) struct {
    step: *std.Build.Step.Run,
    output: std.Build.LazyPath,
} {
    var sed = b.addSystemCommand(&.{
        "sed",
        "-e",
        "1i#define DEFUN(x,y,z) x y",
        "-ne",
        "/^DEFUN/{p;n;/^[ \t]/p;}",
    });
    sed.addFileArg(b.path("main.c"));
    sed.addFileArg(b.path("menu.c"));
    // {
    //     const install = b.addInstallFile(sed.captureStdOut(), "01_sed.txt");
    //     b.getInstallStep().dependOn(&install.step);
    // }

    var cpp = b.addSystemCommand(&.{ "gcc", "-E", "-" });
    cpp.setStdIn(.{
        .lazy_path = sed.captureStdOut(),
    });
    // {
    //     const install = b.addInstallFile(cpp.captureStdOut(), "02_gcc_e.txt");
    //     b.getInstallStep().dependOn(&install.step);
    // }

    var awk = b.addSystemCommand(&.{
        "awk",
        "$1 ~ /^[_A-Za-z]/ { for (i=2;i<=NF;i++) { print $i, $1} }",
    });
    awk.setStdIn(.{
        .lazy_path = cpp.captureStdOut(),
    });

    return .{
        .step = awk,
        .output = awk.captureStdOut(),
    };
}
