const std = @import("std");
const zcc = @import("compile_commands.zig");

const output_public_headers = [_][]const u8{
    "writer.h",
    "TermEntry.h",
    "graphicchar.h",
    "frame.h",
    "line_prop.h",
};
const output_srcs = [_][]const u8{
    "writer.c",
    "TermEntry.c",
    "graphicchar.c",
};

const w3m_srcs = [_][]const u8{
    "tty.c",
    "keybind.c",
    "util.c",
    "w3m.c",
    "parseArgs.c",
    "screen.c",
    "screen_effects.c",
    "term_size.c",
    "term_renderer.c",
    "putc.c",
    "auth.c",
    "proxy.c",
    "mysignal.c",
    "http.c",
    "downloadlist.c",
    "growbuf.c",
    "quote.c",

    "ui.c",
    "LineEditor.c",

    "url_scheme.c",
    "ssl_util.c",
    "str_util.c",
    "line.c",

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
    "rc.c",
    "menu.c",
    "mailcap.c",
    "image.c",
    "symbol.c",
    "entity.c",
    "url.c",
    "mimehead.c",
    "regex.c",
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
    "hash.c",

    "version.c",
};

pub fn build(b: *std.Build) void {
    var targets = std.array_list.Managed(*std.Build.Step.Compile).init(b.allocator);
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

    const mod = b.addModule("w3m", .{
        .target = target,
        .optimize = optimize,
        .root_source_file = b.path("src/main.zig"),
    });
    const exe = b.addExecutable(.{
        .name = "w3m",
        .root_module = mod,
    });
    b.installArtifact(exe);
    targets.append(exe) catch @panic("OOM");
    exe.linkLibC();

    const flags = [_][]const u8{
        "-std=c2x",
        // "-Wno-implicit-int",
        // "-Wno-int-conversion",
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
        .root = b.path("src/core"),
        .files = &w3m_srcs,
        .flags = &flags,
    });
    exe.addIncludePath(b.path("src/core"));

    if (target.result.os.tag == .windows) {
        const ssl = b.dependency("ssl_prebuilt", .{});
        exe.addLibraryPath(ssl.path("x64/lib"));
        exe.addIncludePath(ssl.path("x64/include"));
    } else {
        const system_libs = [_][]const u8{
            "ncurses",
            "ssl",
            "crypto",
        };
        for (system_libs) |lib| {
            exe.linkSystemLibrary(lib);
        }
    }

    const gcs_dep = b.dependency("gcstring", .{
        .target = target,
        .optimize = optimize,
    });
    const gcs = gcs_dep.artifact("gcstring");
    exe.linkLibrary(gcs);

    const output = build_output(b, target, optimize);
    exe.linkLibrary(output);

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

    // {
    //     const mktable = build_mktable(b, b.graph.host, optimize, &.{});
    //     // {
    //     //     b.installArtifact(mktable);
    //     // }
    //     var run_mktable = b.addRunArtifact(mktable);
    //     run_mktable.setCwd(wf.getDirectory());
    //     run_mktable.addArg("100");
    //     // run_mktable.addFileArg(functable_tab.output);
    //     run_mktable.addArg("functable.tab");
    //     const install = b.addInstallFile(run_mktable.captureStdOut(), "include/functable.c");
    //     b.getInstallStep().dependOn(&install.step);
    //
    //     exe.step.dependOn(&install.step);
    // }

    _ = zcc.createStep(b, "cdb", targets.toOwnedSlice() catch @panic("OOM"));
}

fn build_output(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
) *std.Build.Step.Compile {
    const mod = b.addModule("output", .{
        .target = target,
        .optimize = optimize,
    });
    const lib = b.addLibrary(.{
        .name = "output",
        .root_module = mod,
    });
    lib.linkLibC();
    lib.addCSourceFiles(.{
        .root = b.path("src/output"),
        .files = &output_srcs,
        // .flags = &flags,
    });
    for (output_public_headers) |header| {
        lib.installHeader(b.path("src/output").path(b, header), header);
    }
    return lib;
}

// 0. gcc -E
//   nulcmd NOTHING NULL @ @ @
//
// 1.
//   awk "$1 ~ /^[_A-Za-z]/ { for (i=2;i<=NF;i++) { print $i, $1} }" > funcname.tab
//   NOTHING nulcmd
//   NULL nulcmd
//   @ nulcmd
//   @ nulcmd
//   @ nulcmd
//
// 2.
//   awk -f funcname0.awk > funcname.c => func.c
//   FuncList w3mFuncList[] = { /*0*/ {"@",nulcmd},};
//
//   awk -f funcname1.awk > funcname1.h => fm.1
//   #define FUNCNAME_nulcmd 0
//
//   awk -f funcname2.awk > funcname2.h => keybind.h
//   #define nulcmd 0
//
//   awk -f functable.awk > functable.tab
//   @ FUNCNAME_nulcmd
//
//   awk -f defun.awk > defun.h
//   void nulcmd();
//
// 3.
//   mktable 100 functable.tab > functable.c => func.c
//   static HashItem_si MyHashItem[] = { /* 0 */ {"SUSPEND", FUNCNAME_susp, &MyHashItem[1]}, };
//
fn gen_functable(b: *std.Build) *std.Build.Step.WriteFile {
    const wf = b.addWriteFiles();

    const gcc_e = gen_gcc_e(b);
    _ = wf.addCopyFile(gcc_e.output, "gcc_e.txt");

    const funcname_tab = run_awk_cmd(
        b,
        gcc_e.output,
        "$1 ~ /^[_A-Za-z]/ { for (i=2;i<=NF;i++) { print $i, $1} }",
    );
    _ = wf.addCopyFile(funcname_tab.output, "funcname.tab");

    const defun_h = run_awk_cmd(
        b,
        gcc_e.output,
        "$1 ~ /^[_A-Za-z]/ { print \"void \" $1 \"();\" }",
    );
    _ = wf.addCopyFile(defun_h.output, "defun.h");

    const funcname_c = gen_funcname(b, funcname_tab.output, b.path("funcnamemap.awk"));
    _ = wf.addCopyFile(funcname_c.output, "funcnamemap.h");

    // const funcname_c = gen_funcname(b, funcname_tab.output, b.path("funcname0.awk"));
    // _ = wf.addCopyFile(funcname_c.output, "funcname.c");

    // const funcname1_h = gen_funcname(b, funcname_tab.output, b.path("funcname1.awk"));
    // _ = wf.addCopyFile(funcname1_h.output, "funcname1.h");
    //
    // const funcname2_h = gen_funcname(b, funcname_tab.output, b.path("funcname2.awk"));
    // _ = wf.addCopyFile(funcname2_h.output, "funcname2.h");
    //
    // const functable_tab = gen_funcname(b, funcname_tab.output, b.path("functable.awk"));
    // _ = wf.addCopyFile(functable_tab.output, "functable.tab");

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
        // .root = b.path("src"),
        .files = &.{
            "src/funcname/mktable.c",
            "src/core/hash.c",
            "gcstring/libwc/Str.c",
            "gcstring/libwc/myctype.c",
        },
        .flags = &.{
            "-DDUMMY",
        },
    });
    exe.addIncludePath(b.path("src/core"));
    exe.addIncludePath(b.path("gcstring/libwc"));
    exe.linkLibC();
    exe.linkLibCpp();
    for (libs) |lib| {
        exe.linkSystemLibrary(lib);
    }

    // build for mktable
    const gc_dep = b.dependency("gc", .{
        .target = target,
        .optimize = optimize,
        .BUILD_SHARED_LIBS = false,
    });
    const gc = gc_dep.artifact("gc");
    exe.linkLibrary(gc);
    exe.addIncludePath(gc_dep.path("include"));

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

// txt > awk
fn run_awk_cmd(
    b: *std.Build,
    src: std.Build.LazyPath,
    script: []const u8,
) struct {
    step: *std.Build.Step.Run,
    output: std.Build.LazyPath,
} {
    var awk = b.addSystemCommand(&.{ "awk", script });
    awk.setStdIn(.{ .lazy_path = src });
    return .{
        .step = awk,
        .output = awk.captureStdOut(),
    };
}

// sed > gcc _e > txt
fn gen_gcc_e(b: *std.Build) struct {
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
    sed.addFileArg(b.path("src/core/w3m.c"));
    sed.addFileArg(b.path("src/core/menu.c"));
    // {
    //     const install = b.addInstallFile(sed.captureStdOut(), "01_sed.txt");
    //     b.getInstallStep().dependOn(&install.step);
    // }

    // var cpp = b.addSystemCommand(&.{ "gcc", "-E", "-" });
    var cpp = b.addSystemCommand(&.{ "zig", "cc", "-E", "-" });
    cpp.setStdIn(.{
        .lazy_path = sed.captureStdOut(),
    });
    // {
    //     const install = b.addInstallFile(cpp.captureStdOut(), "02_gcc_e.txt");
    //     b.getInstallStep().dependOn(&install.step);
    // }}

    return .{
        .step = cpp,
        .output = cpp.captureStdOut(),
    };
}
