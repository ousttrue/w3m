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
};

const srcs = [_][]const u8{
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

    const mod = b.addModule("wc", .{
        .target = target,
        .optimize = optimize,
    });
    const lib = b.addLibrary(.{
        .name = "wc",
        .root_module = mod,
    });
    b.installArtifact(lib);
    lib.linkLibC();
    lib.addIncludePath(b.path(""));
    lib.addCSourceFiles(.{
        // .root = b.path("libwc"),
        .files = &srcs,
        .flags = &.{
            "-DUSE_UNICODE",
        },
    });

    for (public_headers) |header| {
        lib.installHeader(b.path(header), header);
    }
}
