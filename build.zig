const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const funcname_tab = gen_funcname_tab(b);
    {
        const install = b.addInstallFile(funcname_tab.output, "gen/funcname.tab");
        b.getInstallStep().dependOn(&install.step);
    }

    const funcname_c = gen_funcname(b, funcname_tab.output, b.path("funcname0.awk"));
    {
        const install = b.addInstallFile(funcname_c.output, "gen/funcname.c");
        b.getInstallStep().dependOn(&install.step);
    }

    const funcname1_h = gen_funcname(b, funcname_tab.output, b.path("funcname1.awk"));
    {
        const install = b.addInstallFile(funcname1_h.output, "gen/funcname1.h");
        b.getInstallStep().dependOn(&install.step);
    }

    const funcname2_h = gen_funcname(b, funcname_tab.output, b.path("funcname2.awk"));
    {
        const install = b.addInstallFile(funcname2_h.output, "gen/funcname2.h");
        b.getInstallStep().dependOn(&install.step);
    }

    const functable_tab = gen_funcname(b, funcname_tab.output, b.path("functable.awk"));
    {
        const install = b.addInstallFile(functable_tab.output, "gen/functable.tab");
        b.getInstallStep().dependOn(&install.step);
    }

    const mktable = build_mktable(b, target, optimize, &.{"gc"});
    {
        b.installArtifact(mktable);
    }
    var run_mktable = b.addRunArtifact(mktable);
    run_mktable.setCwd(b.path("zig-out/gen"));
    run_mktable.addArg("100");
    // run_mktable.addFileArg(functable_tab.output);
    run_mktable.addArg("functable.tab");
    {
        const install = b.addInstallFile(run_mktable.captureStdOut(), "gen/functable.c");
        b.getInstallStep().dependOn(&install.step);
    }
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
            "mktable.c", "entity.c", "Str.c", "hash.c", "myctype.c",
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
