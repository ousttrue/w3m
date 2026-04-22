const std = @import("std");
const c = @import("c.zig").c;
const g = @import("global.zig");
pub const runtime = @import("runtime.zig");
pub const global = @import("global.zig");
const tty = @import("tty.zig");
const content_type = @import("content_type.zig");
const guessContentType = content_type.guessContentType;
const terminfo_entry = @import("terminfo_entry.zig");
const input_dispatcher = @import("input_dispatcher.zig");
const image = @import("image.zig");
const history = @import("history.zig");

pub export fn _dummy_() void {
    // export symbols ?
    std.log.debug("{}", .{global});
    std.log.debug("{}", .{terminfo_entry});
    std.log.debug("{}", .{content_type});
    std.log.debug("{}", .{image});
    std.log.debug("{}", .{history});
}

comptime {
    std.testing.refAllDecls(@This());
}

pub fn init(process_init: std.process.Init) void {
    runtime.init(process_init);
    tty.init(process_init.io);
}

pub fn deinit() void {
    input_dispatcher.deinit();
    tty.deinit();
    runtime.deinit();
}

extern fn checkDownloadList() bool;
extern fn submitCurrentBuffer(args: *c.CmdArgs) bool;
extern fn processCurrentEvent() bool;
extern fn processCurrentBufferEvent() bool;
extern fn processResizeAndImage(args: *c.CmdArgs) void;

export fn w3m_loop() c_int {
    input_dispatcher.init();

    while (g.is_running) {
        var args: c.CmdArgs = .{};

        if (checkDownloadList()) {
            c.ldDL(null);
        }
        if (processCurrentEvent()) {
            continue;
        }
        if (processCurrentBufferEvent()) {
            continue;
        }
        if (submitCurrentBuffer(&args)) {
            continue;
        }

        // TODO:
        // processResizeAndImage(&args);

        const has_input = tty.epoll.next(80) catch {
            // error ?
            break;
        };

        if (has_input) |_| {
            var buf: [1]u8 = undefined;
            const readsize = std.posix.read(
                tty.tty_in.handle,
                &buf,
            ) catch @panic("getch");
            std.debug.assert(readsize == 1);

            const ch: u8 = buf[0];
            input_dispatcher.dispatch_key(ch);
        } else {
            input_dispatcher.dispatch_timeout();
        }
    }

    return 0;
}

var fileToDelete: std.ArrayList([]const u8) = .initBuffer(&.{});

export fn addDeleteFile(file: [*c]const u8) void {
    const copy = runtime.allocator.dupe(u8, std.mem.span(file)) catch @panic("OOM");
    fileToDelete.append(runtime.allocator, copy) catch @panic("OOM");
}

export fn deleteFiles() void {
    const allocator = runtime.allocator;
    const io = runtime.io;

    for (fileToDelete.items) |file| {
        std.Io.Dir.cwd().deleteFile(io, file) catch {
            std.log.warn("fail to remove: {s}", .{file});
        };
        if (g.enable_inline_image == c.INLINE_IMG_SIXEL and std.mem.endsWith(u8, file, ".gif")) {
            const firstframe = std.fmt.allocPrint(allocator, "{s}-1", .{file}) catch @panic("OOM");
            defer allocator.free(firstframe);
            std.Io.Dir.cwd().deleteFile(io, firstframe) catch {
                std.log.warn("fail to remove: {s}", .{firstframe});
            };
        }
        allocator.free(file);
    }
    fileToDelete.deinit(allocator);
}

var tmpf_base: []const []const u8 = &.{
    "tmp",
    "src",
    "frame",
    "cache",
    "cookie",
    "hist",
};
var tmpf_seq = [1]usize{0} ** c.MAX_TMPF_TYPE;

export fn tmpfname(tmp_type: c.TmpFileType, _ext: ?[*:0]const u8) [*c]const u8 {
    const dir = if (tmp_type == c.TMPF_HIST) std.mem.span(g.rc_dir) else std.mem.span(g.tmp_dir);
    const tmpf = std.fmt.allocPrintSentinel(runtime.allocator, "{s}/w3m{s}{}-{}{s}", .{
        dir,
        tmpf_base[tmp_type],
        g.CurrentPid,
        tmpf_seq[tmp_type],
        if (_ext) |ext| ext else "",
    }, 0) catch @panic("OOM");
    tmpf_seq[tmp_type] += 1;
    addDeleteFile(tmpf);
    return &tmpf[0];
}
