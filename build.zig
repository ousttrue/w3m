//
// ./configure
//   --disable-m17n
//   --disable-nls
//   --disable-rpath
//   --disable-color
//   --disable-menu
//   --disable-mouse
//   --disable-alarm
//   --disable-nntp
//   --disable-gopher
//   --disable-external-uri-loader
//   --disable-w3mmailer
//   --without-x
//   --without-imagelib
//
//   config.h
//
// make
//   entity.h
//   funcname.tab
//   funcname.c
//   funcname1.h
//   funcname2.h
//   functable.c
//   tagtable.c

//
const std = @import("std");
const build_mktable = @import("build_mktable.zig");

pub fn build(b: *std.Build) void {
    var targets = std.ArrayList(*std.Build.Step.Compile).init(b.allocator);

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

    const cflags = [_][]const u8{
        "-std=c99",
        "-DHAVE_CONFIG_H",
        b.fmt("-DAUXBIN_DIR=\"{s}\"", .{AUXBIN_DIR}),
        b.fmt("-DCGIBIN_DIR=\"{s}\"", .{CGIBIN_DIR}),
        b.fmt("-DHELP_DIR=\"{s}\"", .{HELP_DIR}),
        b.fmt("-DETC_DIR=\"{s}\"", .{ETC_DIR}),
        b.fmt("-DCONF_DIR=\"{s}\"", .{CONF_DIR}),
        b.fmt("-DRC_DIR=\"{s}\"", .{RC_DIR}),
        b.fmt("-DLOCALEDIR=\"{s}\"", .{localedir}),
    };

    const exe = build_exe(b, target, optimize, &cflags);
    b.installArtifact(exe);
    targets.append(exe) catch @panic("OOM");

    // link libs
    const gc_dep = b.dependency("gc", .{ .target = target, .optimize = optimize });
    exe.linkLibrary(gc_dep.artifact("gc"));
    const ssl_dep = b.dependency("openssl", .{ .target = target, .optimize = optimize });
    exe.linkLibrary(ssl_dep.artifact("openssl"));
    exe.linkSystemLibrary("ncurses");

    // // code generation
    // const wf = b.addWriteFiles();
    // exe.step.dependOn(&wf.step);
    // exe.addIncludePath(wf.getDirectory());
    //
    // // funcname.tab: $(DEFUNS)
    // // 	(echo '#define DEFUN(x,y,z) x y';\
    // // 	 sed -ne '/^DEFUN/{p;n;/^[ 	]/p;}' $(DEFUNS)) | $(CPP) $(CPPFLAGS) - | \
    // // 	 awk '$$1 ~ /^[_A-Za-z]/ { \
    // // 	       for (i=2;i<=NF;i++) { print $$i, $$1} \
    // // 	 }' > $@.tmp
    // {}
    //
    // // funcname1.h: funcname.tab
    // // 	sort funcname.tab | $(AWK) -f $(top_srcdir)/funcname1.awk > $@
    //
    // const sort = b.addSystemCommand(&.{ "sort", "funcname.tab" });
    // const sort_output = sort.captureStdOut();
    //
    // const mkTable = build_mktable.build(b, b.host, .Debug);
    // mkTable.linkLibrary(gc_dep.artifact("gc"));
    //
    // // tagtable.c: tagtable.tab mktable$(EXT) html.h
    // // 	./mktable$(EXT) 100 $(srcdir)/tagtable.tab > $@
    // {
    //     const run_mktable_tagtable_c = b.addRunArtifact(mkTable);
    //     run_mktable_tagtable_c.addArg("100");
    //     run_mktable_tagtable_c.addArg("tagtable.tab");
    //     const tagtable_c_output = run_mktable_tagtable_c.captureStdOut();
    //     exe.addCSourceFile(.{
    //         // tagtable.c
    //         .file = wf.addCopyFile(tagtable_c_output, "tagtable.c"),
    //         .flags = &cflags,
    //     });
    // }
    //
    // // functable.c: funcname.tab mktable$(EXT)
    // // 	sort funcname.tab | $(AWK) -f $(top_srcdir)/functable.awk > functable.tab
    // // 	./mktable$(EXT) 100 functable.tab > $@
    // {
    //     const awk_functable = b.addSystemCommand(&.{ "gawk", "-f", "functable.awk" });
    //     awk_functable.setStdIn(.{ .lazy_path = sort_output });
    //     const awk_functable_output = awk_functable.captureStdOut();
    //
    //     const run_mktable_functable_c = b.addRunArtifact(mkTable);
    //     run_mktable_functable_c.addArg("100");
    //     run_mktable_functable_c.addFileArg(awk_functable_output);
    //     const functalbe_c_output = run_mktable_functable_c.captureStdOut();
    //     _ = wf.addCopyFile(functalbe_c_output, "functable.c");
    // }
    //
    // // entity.h: entity.tab mktable$(EXT)
    // // 	echo '/* $$I''d$$ */' > $@
    // // 	./mktable$(EXT) 100 $(srcdir)/entity.tab >> $@
    // {
    //     const run_mktable_entity_h = b.addRunArtifact(mkTable);
    //     run_mktable_entity_h.addArg("100");
    //     run_mktable_entity_h.addArg("entity.tab");
    //     const entity_h_output = run_mktable_entity_h.captureStdOut();
    //     _ = wf.addCopyFile(entity_h_output, "entity.h");
    // }
    //
    // // funcname2.h: funcname.tab
    // const awk_funcname2_h = b.addSystemCommand(&.{
    //     "gawk",
    //     "-f",
    //     "funcname2.awk",
    // });
    // awk_funcname2_h.setStdIn(.{ .lazy_path = sort_output });
    // _ = wf.addCopyFile(awk_funcname2_h.captureStdOut(), "funcname2.h");
    //
    // // funcname.c: funcname.tab
    // const awk_funcname_c = b.addSystemCommand(&.{
    //     "gawk",
    //     "-f",
    //     "funcname0.awk",
    // });
    // _ = wf.addCopyFile(awk_funcname_c.captureStdOut(), "funcname.c");

    // run
    const run_cmd = b.addRunArtifact(exe);
    run_cmd.step.dependOn(b.getInstallStep());
    if (b.args) |args| {
        run_cmd.addArgs(args);
    }
    b.step("run", "Run the app").dependOn(&run_cmd.step);

    const exe_unit_tests = b.addTest(.{
        .root_source_file = b.path("src/main.zig"),
        .target = target,
        .optimize = optimize,
    });
    const run_exe_unit_tests = b.addRunArtifact(exe_unit_tests);
    const test_step = b.step("test", "Run unit tests");
    test_step.dependOn(&run_exe_unit_tests.step);

    // add a step called "zcc" (Compile commands DataBase) for making
    // compile_commands.json. could be named anything. cdb is just quick to type
    const zcc = @import("compile_commands");
    zcc.createStep(b, "zcc", .{
        .targets = targets.toOwnedSlice() catch @panic("OOM"),
    });
}

fn build_exe(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    cflags: []const []const u8,
) *std.Build.Step.Compile {
    const exe = b.addExecutable(.{
        .name = "w3m",
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });
    exe.addCSourceFiles(.{
        .files = &.{
            "keybind.c",

            "main.c",
            "defun.c",
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

            "anchor.c",
            "parsetagx.c",
            "tagtable.c",
            "istream.c",

            "Str.c",
            "indep.c",
            "textlist.c",
            "parsetag.c",
            "myctype.c",
            "hash.c",

            "version.c",
        },
        .flags = cflags,
    });
    exe.addIncludePath(b.path("."));
    return exe;
}
