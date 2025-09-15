const std = @import("std");

const public_headers = [_][]const u8{
    "alloc.h",
    "Str.h",
    "myctype.h",
    "wc.h",
    "wtf.h",
    "wc_types.h",
    "ces.h",
    "ccs.h",
    "priv.h",
    "iso2022.h",
    "ucs.h",
    "utf8.h",
    "regex.h",
    "quote.h",
    "textlist.h",
    "base64.h",
    "ctrlcode.h",
    "hash.h",
    "entity.h",
    "str_util.h",
    "geometry.h",
    "symbol.h",
    "TermEntry.h",
    "graphicchar.h",
    "html_form.h",
    "html_quote.h",
    "url_scheme.h",
    "url.h",
    "runtime.h",
    "convertline.h",
    "ContentType.h",
    "http_message.h",
    "CharSlice.h",
};

const srcs = [_][]const u8{
    "CharSlice.c",
    "ContentType.c",
    "http_message.c",
    "convertline.c",
    "runtime.c",
    "url_scheme.c",
    "url.c",
    "html_quote.c",
    "html_form.c",
    "TermEntry.c",
    "graphicchar.c",
    "symbol.c",
    "str_util.c",
    "entity.c",
    "hash.c",
    "base64.c",
    "textlist.c",
    "quote.c",
    "quote_map.c",
    "regex.c",
    "alloc.c",
    "Str.c",
    "myctype.c",
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

    const mod = b.addModule("gcstring", .{
        .target = target,
        .optimize = optimize,
    });
    const lib = b.addLibrary(.{
        .name = "gcstring",
        .root_module = mod,
    });
    b.installArtifact(lib);
    lib.linkLibC();
    lib.addIncludePath(b.path("libwc"));

    const PACKAGE = "w3m";
    const prefix = "/usr/local";
    const exec_prefix = prefix;
    const libexecdir = b.fmt("{s}/libexec", .{exec_prefix});
    const datarootdir = b.fmt("{s}/share", .{prefix});
    const sysconfdir = b.fmt("{s}/etc", .{prefix});
    const CGIBIN_DIR = b.fmt("{s}/{s}/cgi-bin", .{ libexecdir, PACKAGE });
    const AUXBIN_DIR = b.fmt("{s}/{s}", .{ libexecdir, PACKAGE });
    const HELP_DIR = b.fmt("{s}/w3m", .{datarootdir});
    const RC_DIR = "~/.w3m";
    const ETC_DIR = sysconfdir;
    const CONF_DIR = b.fmt("{s}/{s}", .{ sysconfdir, PACKAGE });

    lib.addCSourceFiles(.{
        .root = b.path("libwc"),
        .files = &srcs,
        .flags = &.{
            "-DUSE_UNICODE",
            b.fmt("-DAUXBIN_DIR=\"{s}\"", .{AUXBIN_DIR}),
            b.fmt("-DCGIBIN_DIR=\"{s}\"", .{CGIBIN_DIR}),
            b.fmt("-DETC_DIR=\"{s}\"", .{ETC_DIR}),
            b.fmt("-DCONF_DIR=\"{s}\"", .{CONF_DIR}),
            b.fmt("-DHELP_DIR=\"{s}\"", .{HELP_DIR}),
            b.fmt("-DRC_DIR=\"{s}\"", .{RC_DIR}),
        },
    });

    for (public_headers) |header| {
        lib.installHeader(b.path("libwc").path(b, header), header);
    }

    const gc_dep = b.dependency("gc", .{
        .target = target,
        .optimize = optimize,
        .BUILD_SHARED_LIBS = false,
    });
    const gc = gc_dep.artifact("gc");
    // const gc_include_dir = gc.installed_headers_include_tree orelse {
    //     @panic("no gc header");
    // };
    // const gc_include_dir = gc_dep.path("include");
    // std.log.debug("{s}", .{gc_include_dir.getDisplayName()});
    lib.linkLibrary(gc);

    lib.installHeadersDirectory(gc_dep.path("include"), "", .{});
}
