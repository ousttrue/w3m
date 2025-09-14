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
};

const srcs = [_][]const u8{
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
    lib.addCSourceFiles(.{
        .root = b.path("libwc"),
        .files = &srcs,
        .flags = &.{
            "-DUSE_UNICODE",
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
