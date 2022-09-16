const std = @import("std");

const W3m_SOURCES = [_][]const u8{
    "libwc/big5.cpp",
    "libwc/ces.cpp",
    "libwc/char_conv.cpp",
    "libwc/charset.cpp",
    "libwc/combining.cpp",
    "libwc/conv.cpp",
    "libwc/detect.cpp",
    "libwc/gb18030.cpp",
    "libwc/gbk.cpp",
    "libwc/hkscs.cpp",
    "libwc/hz.cpp",
    "libwc/iso2022.cpp",
    "libwc/jis.cpp",
    "libwc/johab.cpp",
    "libwc/priv.cpp",
    "libwc/putc.cpp",
    "libwc/search.cpp",
    "libwc/sjis.cpp",
    "libwc/status.cpp",
    "libwc/ucs.cpp",
    "libwc/uhc.cpp",
    "libwc/utf7.cpp",
    "libwc/utf8.cpp",
    "libwc/viet.cpp",
    "libwc/wtf.cpp",
    "libwc/Str.cpp",
    "libwc/myctype.cpp",
    //
    "src/indep.cpp",
    "src/regex.cpp",
    "src/textlist.cpp",
    "src/parsetag.cpp",
    "src/hash.cpp",
    //
    "src/main.cpp",
    "src/Args.cpp",
    "src/App.cpp",
    "src/core.cpp",
    "src/defun.cpp",
    "src/file.cpp",
    "src/buffer.cpp",
    "src/display.cpp",
    "src/etc.cpp",
    "src/search.cpp",
    "src/linein.cpp",
    "src/table.cpp",
    "src/local.cpp",
    "src/form.cpp",
    "src/map.cpp",
    "src/frame.cpp",
    "src/rc.cpp",
    "src/menu.cpp",
    "src/mailcap.cpp",
    "src/image.cpp",
    "src/symbol.cpp",
    "src/entity.cpp",
    "src/terms.cpp",
    "src/url.cpp",
    "src/ftp.cpp",
    "src/mimehead.cpp",
    //
    "src/func.cpp",
    "src/cookie.cpp",
    "src/history.cpp",
    "src/backend.cpp",
    //
    "src/keybind.cpp",
    "src/anchor.cpp",
    "src/parsetagx.cpp",
    //
    "src/tagtable.cpp",
    "src/istream.cpp",
    "src/version.c",
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
    exe.addIncludePath("src");
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
