const std = @import("std");
const zcc = @import("compile_commands.zig");

//
// output
//
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

//
// content
//
const content_public_headers = [_][]const u8{
    "geometry.h",
    "growbuf.h",
    "runtime.h",
    "KeyValue.h",
    //
    "html_form.h",
    "proxy.h",
    "network.h",
    "url_scheme.h",
    "url.h",
    "mimehead.h",
    "cookie.h",
    "HttpRequestMethod.h",
    "HttpRequest.h",
    "convertline.h",
    "ssl_util.h",
    "UserInteraction.h",
    "auth.h",
    "subprocess.h",
    "local_cgi.h",
    "html_quote.h",
    "mailcap.h",
    "http_message.h",
    "istream.h",
    "compression.h",
    "ContentType.h",
    "CharSlice.h",
    "Content.h",
    "HttpResponse.h",
    "time_util.h",
    "HttpClient.h",
};
const content_srcs = [_][]const u8{
    "HttpClient.c",
    "time_util.c",
    "HttpResponse.c",
    "Content.c",
    "CharSlice.c",
    "ContentType.c",
    "compression.c",
    "istream.c",
    "http_message.c",
    "mailcap.c",
    "html_quote.c",
    "local_cgi.c",
    "subprocess.c",
    "auth.c",
    "ssl_util.c",
    "convertline.c",
    "HttpRequest.c",
    "KeyValue.c",
    "cookie.c",
    "runtime.c",
    "mimehead.c",
    "growbuf.c",
    "url.c",
    "url_scheme.c",
    "network.c",
    "proxy.c",
};

//
// document
//
const document_public_headers = [_][]const u8{
    "token.h",
    "Line.h",
};
const document_srcs = [_][]const u8{
    "Line.c",
    "token.c",
};

//
// w3m
//
const system_libs = [_][]const u8{
    "tinfo",
    "ssl",
    "crypto",
    "z",
};
const w3m_srcs = [_][]const u8{
    "defun.c",

    "page_info.c",
    "follow_anchor.c",
    "buffer_list.c",
    "Document.c",
    "LinkList.c",
    "tty.c",
    "keybind.c",
    "util.c",
    "w3m.c",
    "screen.c",
    "screen_effects.c",
    "term_size.c",
    "term_renderer.c",
    "putc.c",
    "downloadlist.c",
    "keymap.c",
    "progress.c",
    "html_title.c",
    "readbuffer.c",
    "html_tag_info.c",
    "html_tag_attribute_info.c",
    "HtmlTagParsed.c",
    "buffer_loader.c",

    "ui.c",
    "LineEditor.c",

    "buffer.c",
    "display.c",
    "search.c",
    "linein.c",
    "table.c",
    "form.c",
    "maparea.c",
    "rc.c",
    "menu.c",
    "image_loader.c",
    "symbol.c",
    "history.c",

    "Anchor.c",
    "AnchorList.c",
    "tagtable.c",

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

    const test_mod = b.addModule("w3m_test", .{
        .target = target,
        .optimize = optimize,
        .root_source_file = b.path("src/test.zig"),
    });
    const exe_tests = b.addTest(.{
        .root_module = test_mod,
    });
    exe_tests.addCSourceFiles(.{
        .files = &.{
            "src/content/http_message.c",
        },
    });
    exe_tests.addIncludePath(b.path(""));
    b.installArtifact(exe_tests);

    targets.append(exe) catch @panic("OOM");
    exe.linkLibC();

    const flags = [_][]const u8{
        "-std=c2x",
        // "-Wall",
        // "-Werror",
        "-Wno-invalid-source-encoding",
        // https://www.ibm.com/docs/ja/zos/2.5.0?topic=files-feature-test-macros
        // "-D_POSIX_C_SOURCE=200112L",
        "-D_POSIX_C_SOURCE=200809L",
        // "-D_XOPEN_SOURCE=1",
        // "-Wno-implicit-int",
        // "-Wno-int-conversion",
        "-DHAVE_CONFIG_H",
        b.fmt("-DRC_DIR=\"{s}\"", .{RC_DIR}),
        b.fmt("-DLOCALEDIR=\"{s}\"", .{localedir}),
        b.fmt("-DETC_DIR=\"{s}\"", .{ETC_DIR}),
        b.fmt("-DCONF_DIR=\"{s}\"", .{CONF_DIR}),
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
        for (system_libs) |lib| {
            exe.linkSystemLibrary(lib);
            exe_tests.linkSystemLibrary(lib);
        }
    }

    const gcs_dep = b.dependency("gcstring", .{
        .target = target,
        .optimize = optimize,
    });
    const gcs = gcs_dep.artifact("gcstring");
    exe.linkLibrary(gcs);
    exe_tests.linkLibrary(gcs);

    {
        const lib = build_lib(
            b,
            target,
            optimize,
            "output",
            b.path("src/output"),
            &output_srcs,
            &output_public_headers,
            &.{},
        );
        exe.linkLibrary(lib);
        exe_tests.linkLibrary(lib);
    }

    {
        const lib = build_lib(
            b,
            target,
            optimize,
            "content",
            b.path("src/content"),
            &content_srcs,
            &content_public_headers,
            &.{
                b.fmt("-DAUXBIN_DIR=\"{s}\"", .{AUXBIN_DIR}),
                b.fmt("-DCGIBIN_DIR=\"{s}\"", .{CGIBIN_DIR}),
                b.fmt("-DETC_DIR=\"{s}\"", .{ETC_DIR}),
                b.fmt("-DCONF_DIR=\"{s}\"", .{CONF_DIR}),
                b.fmt("-DHELP_DIR=\"{s}\"", .{HELP_DIR}),
                b.fmt("-DRC_DIR=\"{s}\"", .{RC_DIR}),
            },
        );
        lib.linkLibrary(gcs);
        exe.linkLibrary(lib);
        exe_tests.linkLibrary(lib);
    }

    {
        const lib = build_lib(
            b,
            target,
            optimize,
            "document",
            b.path("src/document"),
            &document_srcs,
            &document_public_headers,
            &.{
                // b.fmt("-DAUXBIN_DIR=\"{s}\"", .{AUXBIN_DIR}),
                // b.fmt("-DCGIBIN_DIR=\"{s}\"", .{CGIBIN_DIR}),
                // b.fmt("-DETC_DIR=\"{s}\"", .{ETC_DIR}),
                // b.fmt("-DCONF_DIR=\"{s}\"", .{CONF_DIR}),
                // b.fmt("-DHELP_DIR=\"{s}\"", .{HELP_DIR}),
                // b.fmt("-DRC_DIR=\"{s}\"", .{RC_DIR}),
            },
        );
        lib.linkLibrary(gcs);
        exe.linkLibrary(lib);
        exe_tests.linkLibrary(lib);
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

    const run_exe_tests = b.addRunArtifact(exe_tests);
    const test_step = b.step("test", "Run tests");
    test_step.dependOn(&run_exe_tests.step);
}

fn build_lib(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    name: []const u8,
    root: std.Build.LazyPath,
    files: []const []const u8,
    public_headers: []const []const u8,
    flags: []const []const u8,
) *std.Build.Step.Compile {
    const mod = b.addModule("output", .{
        .target = target,
        .optimize = optimize,
    });
    const lib = b.addLibrary(.{
        .name = name,
        .root_module = mod,
    });
    lib.linkLibC();
    lib.addCSourceFiles(.{
        .root = root,
        .files = files,
        .flags = flags,
    });
    for (public_headers) |header| {
        lib.installHeader(root.path(b, header), header);
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
        "$1 ~ /^[_A-Za-z]/ { print \"void \" $1 \"(struct UI ui);\" }",
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
    sed.addFileArg(b.path("src/core/defun.c"));
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
