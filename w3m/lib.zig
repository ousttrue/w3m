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
const LineInput = @import("LineInput.zig");
const Growbuf = @import("Growbuf.zig");

pub export fn _dummy_() void {
    // export symbols ?
    std.log.debug("{}", .{global});
    std.log.debug("{}", .{terminfo_entry});
    std.log.debug("{}", .{content_type});
    std.log.debug("{}", .{image});
    std.log.debug("{}", .{history});
    std.log.debug("{}", .{LineInput});
    std.log.debug("{}", .{Growbuf});
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

        const may_event = tty.epoll.next(80) catch {
            // error ?
            break;
        };

        if (may_event) |event| {
            if (event.data.fd == tty.epoll.signal_fd) {
                // singal
                var info: std.os.linux.signalfd_siginfo = undefined;
                const readsize = std.posix.read(tty.epoll.signal_fd, @ptrCast(&info)) catch
                    @panic("posix.read(signal_fd)");
                std.debug.assert(readsize == 1);

                std.log.info("resizeed", .{});
            } else {
                // stdin
                var buf: [1]u8 = undefined;
                const readsize = std.posix.read(
                    tty.tty_in.handle,
                    &buf,
                ) catch @panic("posix.read(stdin)");
                std.debug.assert(readsize == 1);

                const ch: u8 = buf[0];
                input_dispatcher.dispatch_key(ch);
            }
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

// #define CLEN (COLS - 2)

export fn do_lineinput(
    args: ?*c.CmdArgs,
    _prompt: ?[*:0]const u8,
    _def_str: ?[*:0]const u8,
    flag: c.InputLineFlags,
    hist: c.HistoryType,
    incrfunc: c.IncrFunc,
) c.LineInputResult {
    var li = LineInput.init(
        runtime.allocator,
        if (_def_str) |def_str| std.mem.span(def_str) else "",
        flag,
        hist,
    ) catch @panic("LineInput.init");
    defer li.deinit();

    li.process(args.?, if (_prompt) |prompt| std.mem.span(prompt) else "", flag, incrfunc);

    var p: []const u8 = std.mem.span(li.strBuf.*.ptr);
    if (flag & (c.IN_FILENAME | c.IN_COMMAND) != 0) {
        p = std.mem.trimStart(u8, p, &std.ascii.whitespace);
    }

    if (hist != c.HistoryNone and 0 == (flag & c.IN_URL) and p.len > 0) {
        const q = history.lastHist(hist);
        if (q == null or std.mem.eql(u8, std.mem.span(q), p)) {
            history.pushHist(hist, p.ptr);
        }
    }

    const pz: [:0]const u8 = if (p.len > 0)
        li.allocator.dupeZ(u8, p) catch @panic("OOM")
    else
        "";
    return .{
        .str = if (flag & c.IN_FILENAME != 0)
            c.expandPath(pz.ptr)
        else
            c.allocStr(pz.ptr, -1),
        .i_broken = li.i_broken,
        .need_redraw = li.need_redraw,
    };
}

export fn growbuf_create() ?*Growbuf {
    const gb = Growbuf.create(runtime.allocator) catch {
        return null;
    };
    return gb;
}

export fn growbuf_destroy(_gb: ?*Growbuf) void {
    const gb = _gb orelse {
        return;
    };
    gb.destroy(runtime.allocator);
}

export fn growbuf_clear(_gb: ?*Growbuf) void {
    const gb = _gb orelse {
        return;
    };
    gb.buf.append(runtime.allocator, 0) catch {};
    gb.buf.items[0] = 0;
    gb.buf.clearRetainingCapacity();
}

export fn growbuf_reserve(_gb: ?*Growbuf, leastarea: usize) void {
    const gb = _gb orelse {
        return;
    };
    gb.buf.ensureTotalCapacity(runtime.allocator, leastarea) catch {};
}

export fn growbuf_span(_gb: ?*Growbuf) c.span {
    const gb = _gb orelse {
        return .{};
    };
    return gb.span();
}

export fn growbuf_add_char(_gb: ?*Growbuf, ch: c_int) void {
    const gb = _gb orelse {
        return;
    };
    gb.buf.append(runtime.allocator, @intCast(ch)) catch {};
}

export fn growbuf_append(_gb: ?*Growbuf, _src: ?[*]const u8, len: usize) void {
    const gb = _gb orelse {
        return;
    };
    if (len == 0) {
        return;
    }
    const src = _src orelse {
        return;
    };
    gb.buf.appendSlice(runtime.allocator, src[0..len]) catch {};
}

export fn MoveFile(path1: [*c]const u8, path2: [*c]const u8) bool {
    var f1 = std.Io.Dir.cwd().openFile(runtime.io, std.mem.span(path1), .{}) catch {
        return false;
    };
    defer f1.close(runtime.io);
    var read_buf: [128]u8 = undefined;
    var r = f1.reader(runtime.io, &read_buf);

    var f2 = std.Io.Dir.cwd().openFile(runtime.io, std.mem.span(path2), .{ .mode = .write_only }) catch {
        return false;
    };
    defer f2.close(runtime.io);
    var write_buf: [128]u8 = undefined;
    var w = f2.writer(runtime.io, &write_buf);
    defer w.flush() catch {};

    var buf: [128]u8 = undefined;
    var write_size: usize = 0;
    while (true) {
        if (r.interface.readSliceShort(&buf)) |size| {
            w.interface.writeAll(buf[0..size]) catch {
                return false;
            };
            if (size < buf.len) {
                return true;
            }
            write_size += size;
            // c.showProgress(&linelen, &trbyte);
        } else |_| {
            return false;
        }
    }

    unreachable;
}

export fn checkOverWrite(args: ?*c.CmdArgs, path: [*c]const u8) bool {
    if (std.Io.Dir.cwd().statFile(runtime.io, std.mem.span(path), .{})) |_| {
        return false;
    } else |_| {}
    const _ans = c.inputAnswer(args, "File exists. Overwrite? (y/n)");
    const ans: [*:0]const u8 = _ans orelse {
        return false;
    };
    if (ans[0] != 'y') {
        return false;
    }
    return true;
}
