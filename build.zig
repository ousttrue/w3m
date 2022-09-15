const std = @import("std");

const W3m_SOURCES = [_][]const u8{
    "libwc/big5.c",
    "libwc/ces.c",
    "libwc/char_conv.c",
    "libwc/charset.c",
    "libwc/combining.c",
    "libwc/conv.c",
    "libwc/detect.c",
    "libwc/gb18030.c",
    "libwc/gbk.c",
    "libwc/hkscs.c",
    "libwc/hz.c",
    "libwc/iso2022.c",
    "libwc/jis.c",
    "libwc/johab.c",
    "libwc/priv.c",
    "libwc/putc.c",
    "libwc/search.c",
    "libwc/sjis.c",
    "libwc/status.c",
    "libwc/ucs.c",
    "libwc/uhc.c",
    "libwc/utf7.c",
    "libwc/utf8.c",
    "libwc/viet.c",
    "libwc/wtf.c",
    //
    "Str.c",
    "indep.cpp",
    "regex.c",
    "textlist.c",
    "parsetag.c",
    "myctype.c",
    "hash.cpp",
    //
    "main.cpp",
    "Args.cpp",
    "App.cpp",
    "core.cpp",
    "defun.cpp",
    "file.cpp",
    "buffer.cpp",
    "display.cpp",
    "etc.cpp",
    "search.c",
    "linein.cpp",
    "table.c",
    "local.c",
    "form.cpp",
    "map.c",
    "frame.cpp",
    "rc.c",
    "menu.c",
    "mailcap.c",
    "image.cpp",
    "symbol.c",
    "entity.cpp",
    "terms.c",
    "url.c",
    "ftp.cpp",
    "mimehead.c",
    "news.c",
    //
    "func.cpp",
    "cookie.cpp",
    "history.cpp",
    "backend.cpp",
    //
    "keybind.cpp",
    "anchor.cpp",
    "parsetagx.c",
    //
    "tagtable.c",
    "istream.cpp",
    "version.c",
};

const PREFIX = "~/local";

const W3M_FLAGS = [_][]const u8{
    "-DHAVE_CONFIG_H",
    "-DAUXBIN_DIR=\"" ++ PREFIX ++ "/libexec/w3m\"",
    "-DCGIBIN_DIR=\"" ++ PREFIX ++ "/libexec/w3m/cgi-bin\"",
    "-DHELP_DIR=\"" ++ PREFIX ++ "/share/w3m\"",
    "-DETC_DIR=\"" ++ PREFIX ++ "/etc\"",
    "-DCONF_DIR=\"" ++ PREFIX ++ "/etc/w3m\"",
    "-DRC_DIR=\"~/.w3m\"",
    "-DLOCALEDIR=\"" ++ PREFIX ++ "/share/locale\"",
    "-DUSE_UNICODE",
};

const W3M_LIBS = [_][]const u8{
    "m",
    "nsl",
    "dl",
    "gc",
    "ssl",
    "crypto",
    "gpm",
    "tinfo",
};

pub fn build(b: *std.build.Builder) void {
    // Standard target options allows the person running `zig build` to choose
    // what target to build for. Here we do not override the defaults, which
    // means any target is allowed, and the default is native. Other options
    // for restricting supported target set are available.
    const target = b.standardTargetOptions(.{});

    // Standard release options allow the person running `zig build` to select
    // between Debug, ReleaseSafe, ReleaseFast, and ReleaseSmall.
    const mode = b.standardReleaseOptions();

    const exe = b.addExecutable("w3m", "src/main.zig");
    exe.setTarget(target);
    exe.setBuildMode(mode);

    exe.linkLibC();
    exe.linkLibCpp();
    exe.addIncludePath(".");
    exe.addIncludePath("libwc");
    exe.addIncludePath("/usr/include");
    exe.addCSourceFiles(&W3m_SOURCES, &W3M_FLAGS);
    for (W3M_LIBS) |lib| {
        exe.linkSystemLibrary(lib);
    }
    exe.install();

    const run_cmd = exe.run();
    run_cmd.step.dependOn(b.getInstallStep());
    if (b.args) |args| {
        run_cmd.addArgs(args);
    }

    const run_step = b.step("run", "Run the app");
    run_step.dependOn(&run_cmd.step);

    const exe_tests = b.addTest("src/main.zig");
    exe_tests.setTarget(target);
    exe_tests.setBuildMode(mode);

    const test_step = b.step("test", "Run unit tests");
    test_step.dependOn(&exe_tests.step);
}
