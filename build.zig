const std = @import("std");
const zcc = @import("compile_commands");

const system_libs = [_][]const u8{
    "gc", "gpm", "ssl", "ncurses", "crypto",
};

const w3m_srcs = [_][]const u8{
    "growbuf.c",
    "alloc.c",
    "line.c",
    "quote.c",
    "signal_util.c",
    "wc_util.c",
    "keybind_mod.c",
    "defun_impl.c",
    "util.c",
    "downloadlist.c",
    "tab.c",
    "proxy.c",
    "http_request.c",
    "url_scheme.c",
    "html.c",
    "html_tag.c",
    "html_token.c",
    "siteconf.c",

    "main.c",
    "file.c",
    "buffer.c",
    "display.c",
    "etc.c",
    "search.c",
    "line_input.c",
    "table.c",
    "local.c",
    "form.c",
    "maparea.c",
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
    "tagtable.c",
    "istream.c",

    "Str.c",
    "indep.c",
    "textlist.c",
    "parsetag.c",
    "myctype.c",
    "hash.c",
};
const flags = [_][]const u8{
    "-DOPENSSL_API_COMPAT=0x010101000L",
    // "-Wno-implicit-int",
    // "-Wno-int-conversion",
    // "-DHAVE_CONFIG_H",
    // "-Wall",
    "-std=c23",
};

pub fn build(b: *std.Build) void {
    var targets: std.ArrayList(*std.Build.Step.Compile) = .initBuffer(&.{});

    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const mod = b.addModule("w3m", .{
        .target = target,
        .optimize = optimize,
        .root_source_file = b.path("main.zig"),
        .link_libc = true,
    });
    const exe = b.addExecutable(.{
        .name = "w3m",
        .root_module = mod,
        .use_llvm = true,
        .use_lld = true,
    });
    targets.append(b.allocator, exe) catch @panic("OOM");
    b.installArtifact(exe);
    exe.root_module.addIncludePath(b.path("."));

    const defun_mod = b.addModule("defun", .{
        .target = target,
        .optimize = optimize,
        .root_source_file = b.path("defun.zig"),
        .link_libc = true,
    });
    defun_mod.addIncludePath(b.path(""));
    const defun_lib = b.addLibrary(.{
        .name = "defun",
        .root_module = defun_mod,
    });
    exe.root_module.linkLibrary(defun_lib);

    const w3m_dep = b.dependency("w3m", .{
        .target = target,
        .optimize = optimize,
    });
    const gen_run = b.addRunArtifact(w3m_dep.artifact("global_gen"));
    exe.step.dependOn(&gen_run.step);
    exe.root_module.addIncludePath(b.path("."));
    exe.root_module.addIncludePath(b.path("w3m"));

    const w3m_lib = w3m_dep.artifact("w3m");
    exe.root_module.addImport("w3m", w3m_lib.root_module);
    // exe.root_module.addIncludePath(w3m_lib.getEmittedIncludeTree());
    // exe.root_module.linkLibrary(w3m_lib);

    exe.root_module.addCSourceFiles(.{
        .files = &w3m_srcs,
        .flags = &flags,
    });

    const wc_dep = b.dependency("wc", .{
        .target = target,
        .optimize = optimize,
    });
    const libwc = wc_dep.artifact("wc");
    exe.root_module.linkLibrary(libwc);

    for (system_libs) |lib| {
        exe.root_module.linkSystemLibrary(lib, .{});
    }

    const co = build_coroutine(b, target, optimize);
    exe.root_module.addImport("co", co);
    // const cdb = zcc.createStep(b, targets.toOwnedSlice(b.allocator) catch @panic("OOM"));
    // b.getInstallStep().dependOn(&cdb.step);
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
    sed.addFileArg(b.path("defun.c"));
    // {
    //     const install = b.addInstallFile(sed.captureStdOut(), "01_sed.txt");
    //     b.getInstallStep().dependOn(&install.step);
    // }

    var cpp = b.addSystemCommand(&.{ "gcc", "-E", "-" });
    cpp.setStdIn(.{
        .lazy_path = sed.captureStdOut(.{}),
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
        .lazy_path = cpp.captureStdOut(.{}),
    });

    return .{
        .step = awk,
        .output = awk.captureStdOut(.{}),
    };
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
