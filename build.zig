const std = @import("std");
const zcc = @import("compile_commands");

const system_libs = [_][]const u8{
    "gc", "ssl", "crypto",

    "ncurses",
    // "termcap",
};

const w3m_srcs = [_][]const u8{
    "w3m_rc.c",

    "tab.c",
    "buffer.c",
    "download.c",
    "keybind.c",
    "util.c",
    "html_form.c",
    "line.c",

    "urlscheme.c",
    "siteconf.c",
    "http_request.c",
    "main.c",
    "file.c",
    "display.c",
    "etc.c",
    "search.c",
    "linein.c",
    "html_table.c",
    "local.c",
    "maparea.c",
    "frame.c",
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

    "indep.c",
    "textlist.c",
    "parsetag.c",
    "myctype.c",
    "hash.c",

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
    var targets = std.ArrayListUnmanaged(*std.Build.Step.Compile){};

    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    // ./configure --prefix=$HOME/local --enable-image=fb --disable-mouse

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

    // w3mbookmark
    const libexecdir = b.fmt("{s}/lib", .{exec_prefix});

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

    const mod = b.addModule("w3m", .{
        .target = target,
        .optimize = optimize,
        .root_source_file = b.path("main.zig"),
    });
    const exe = b.addExecutable(.{
        .name = "w3m",
        .root_module = mod,
        .use_llvm = true,
    });
    targets.append(b.allocator, exe) catch @panic("OOM");
    b.installArtifact(exe);
    exe.linkLibC();
    exe.addIncludePath(b.path("libwc"));
    exe.addIncludePath(b.path("."));

    const Str_mod = b.addModule("Str", .{
        .target = target,
        .optimize = optimize,
        .root_source_file = b.path("Str.zig"),
        .link_libc = true,
    });
    Str_mod.addIncludePath(b.path(""));
    const Str_lib = b.addLibrary(.{
        .name = "Str",
        .root_module = Str_mod,
    });
    exe.linkLibrary(Str_lib);

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
    exe.addCSourceFiles(.{
        .files = &w3m_srcs,
        .flags = &flags,
    });
    exe.addCSourceFiles(.{
        .root = b.path("libwc"),
        .files = &libwc_srcs,
        .flags = &.{
            "-DHAVE_CONFIG_H",
            "-DUSE_UNICODE",
        },
    });
    for (system_libs) |lib| {
        exe.linkSystemLibrary(lib);
    }

    const wf = gen_functable(b);
    {
        const install = b.addInstallDirectory(.{
            .source_dir = wf.getDirectory(),
            .install_dir = .header,
            .install_subdir = "",
        });
        b.getInstallStep().dependOn(&install.step);

        exe.step.dependOn(&install.step);
        exe.addIncludePath(b.path("zig-out/include"));
    }

    {
        const mktable = build_mktable(b, b.graph.host, .ReleaseSafe, &.{"gc"});
        mktable.addIncludePath(b.path("libwc"));
        mktable.addIncludePath(b.path("."));
        mktable.linkLibrary(Str_lib);

        // {
        //     b.installArtifact(mktable);
        // }
        var run_mktable = b.addRunArtifact(mktable);
        run_mktable.setCwd(wf.getDirectory());
        run_mktable.addArg("100");
        // run_mktable.addFileArg(functable_tab.output);
        run_mktable.addArg("functable.tab");
        const install = b.addInstallFile(run_mktable.captureStdOut(), "include/functable.c");
        b.getInstallStep().dependOn(&install.step);

        exe.step.dependOn(&install.step);
    }

    _ = zcc.createStep(b, "cdb", targets.toOwnedSlice(b.allocator) catch @panic("OOM"));
}

fn gen_functable(b: *std.Build) *std.Build.Step.WriteFile {
    const wf = b.addWriteFiles();

    const funcname_tab = gen_funcname_tab(b);
    _ = wf.addCopyFile(funcname_tab.output, "funcname.tab");

    const funcname_c = gen_funcname(b, funcname_tab.output, b.path("funcname0.awk"));
    _ = wf.addCopyFile(funcname_c.output, "funcname.c");

    const funcname1_h = gen_funcname(b, funcname_tab.output, b.path("funcname1.awk"));
    _ = wf.addCopyFile(funcname1_h.output, "funcname1.h");

    const funcname2_h = gen_funcname(b, funcname_tab.output, b.path("funcname2.awk"));
    _ = wf.addCopyFile(funcname2_h.output, "funcname2.h");

    const functable_tab = gen_funcname(b, funcname_tab.output, b.path("functable.awk"));
    _ = wf.addCopyFile(functable_tab.output, "functable.tab");

    return wf;
}

fn build_mktable(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    libs: []const []const u8,
) *std.Build.Step.Compile {
    const mod = b.addModule("mktable", .{
        .target = target,
        .optimize = optimize,
    });
    const exe = b.addExecutable(.{
        .name = "mktable",
        .root_module = mod,
    });
    exe.addCSourceFiles(.{
        .files = &.{
            "mktable.c", "entity.c", "hash.c", "myctype.c",
        },
        .flags = &.{
            "-DDUMMY",
        },
    });
    exe.linkLibC();
    for (libs) |lib| {
        exe.linkSystemLibrary(lib);
    }
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
