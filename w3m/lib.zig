const std = @import("std");
const c = @import("c.zig").c;
const g = @import("global.zig");
pub const runtime = @import("runtime.zig");
pub const global = @import("global.zig");
const tty = @import("tty.zig");
const content_type = @import("content_type.zig");
const guessContentType = content_type.guessContentType;
const input_dispatcher = @import("input_dispatcher.zig");
const image = @import("image.zig");
const history = @import("history.zig");
const LineInput = @import("LineInput.zig");
const Growbuf = @import("Growbuf.zig");
const ScreenRenderer = @import("ScreenRenderer.zig");
const screen = @import("screen.zig");

export var terminfo: c.TermInfo = .{};

pub export fn _dummy_() void {
    // export symbols ?
    std.log.debug("{}", .{global});
    std.log.debug("{}", .{content_type});
    std.log.debug("{}", .{image});
    std.log.debug("{}", .{history});
    std.log.debug("{}", .{LineInput});
    std.log.debug("{}", .{Growbuf});
    std.log.debug("{}", .{screen});
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

const PutC = fn (c_int) callconv(.c) c_int;

export fn terminfo_reset(ti: *c.TermInfo, do_not_use_ti_te: bool) void {
    // turn off
    es_writestr(ti.T_op);
    es_writestr(ti.T_me);
    if (!do_not_use_ti_te) {
        if (ti.T_te != null and ti.T_te[0] != 0) {
            es_writestr(ti.T_te);
        } else {
            es_writestr(ti.T_cl);
        }
    }
    // reset terminal
    es_writestr(ti.T_se);
}

// export fn MOVE(f: c.PutC, ti: *c.TermInfo, line: c_int, column: c_int) void {
//     _ = c.tputs(c.tgoto(ti.T_cm, column, line), 1, f);
// }

const fixed_putc = @import("fixed_putc.zig");

extern fn tputs(str: [*c]const u8, affcnt: c_int, putc: *const PutC) c_int;
extern fn tgoto(cm: [*c]const u8, destcol: c_int, destline: c_int) [*c]const u8;
extern fn tgetent(bp: [*c]u8, name: [*c]const u8) c_int;
// extern int tgetnum(char*);
extern fn tgetflag(name: [*c]const u8) c_int;
extern fn tgetstr(name: [*c]const u8, bp: [*c]u8) [*c]u8;

pub export fn es_writestr(s: [*c]const u8) void {
    fixed_putc.init();
    _ = tputs(s, 1, &fixed_putc.putc);
    tty.puts(std.mem.span(fixed_putc.ptr())) catch @panic("es_writestr");
}
export fn es(str: [*c]const u8) [*c]const u8 {
    fixed_putc.init();
    _ = tputs(str, 1, &fixed_putc.putc);
    return fixed_putc.ptr();
}
pub export fn es_move(ti: *c.TermInfo, line: c_int, column: c_int) [*c]const u8 {
    return es(tgoto(ti.T_cm, column, line));
}

export fn graph_ok(_ti: ?*c.TermInfo) bool {
    if (g.UseGraphicChar != c.GRAPHIC_CHAR_DEC)
        return false;
    const ti = _ti orelse {
        return false;
    };
    return ti.T_as[0] != 0 and ti.T_ae[0] != 0 and ti.T_ac[0] != 0;
}

fn setgraphchar(ti: *c.TermInfo) void {
    for (0..96) |i| {
        ti.gcmap[i] = @intCast(i + ' ');
    }

    if (ti.T_ac != null) {
        // TODO:
        //         int n = strlen(ti.T_ac);
        //         for (int i = 0; i < n - 1; i += 2) {
        //             uint8_t c = (uint8_t)ti.T_ac[i] - ' ';
        //             if (c >= 0 && c < 96)
        //                 ti.gcmap[c] = ti.T_ac[i + 1];
        //         }
    }
}

export fn getTCstr(ti: *c.TermInfo) void {
    const ent = runtime.environ_map.get("TERM") orelse {
        @panic("TERM is not set");
    };

    const r = tgetent(&ti.bp[0], ent.ptr);
    if (r != 1) {
        // Can't find termcap entry
        @panic("Can't find termcap entry");
    }

    var pt = ti.funcstr;
    ti.T_ce = tgetstr("ce", &pt);
    ti.T_cd = tgetstr("cd", &pt);
    ti.T_kr = tgetstr("nd", &pt);
    if (null == ti.T_kr)
        ti.T_kr = tgetstr("kr", &pt);
    if (0 != tgetflag("bs")) {
        ti.T_kl = 8; //"\b";
    } else {
        ti.T_kl = tgetstr("le", &pt);
        if (null == ti.T_kl)
            ti.T_kl = tgetstr("kb", &pt);
        if (null == ti.T_kl)
            ti.T_kl = tgetstr("kl", &pt);
    }
    ti.T_cr = tgetstr("cr", &pt);
    ti.T_ta = tgetstr("ta", &pt);
    ti.T_sc = tgetstr("sc", &pt);
    ti.T_rc = tgetstr("rc", &pt);
    ti.T_so = tgetstr("so", &pt);
    ti.T_se = tgetstr("se", &pt);
    ti.T_us = tgetstr("us", &pt);
    ti.T_ue = tgetstr("ue", &pt);
    ti.T_md = tgetstr("md", &pt);
    ti.T_me = tgetstr("me", &pt);
    ti.T_cl = tgetstr("cl", &pt);
    ti.T_cm = tgetstr("cm", &pt);
    ti.T_al = tgetstr("al", &pt);
    ti.T_sr = tgetstr("sr", &pt);
    ti.T_ti = tgetstr("ti", &pt);
    ti.T_te = tgetstr("te", &pt);
    ti.T_nd = tgetstr("nd", &pt);
    ti.T_eA = tgetstr("eA", &pt);
    ti.T_as = tgetstr("as", &pt);
    ti.T_ae = tgetstr("ae", &pt);
    ti.T_ac = tgetstr("ac", &pt);
    ti.T_op = tgetstr("op", &pt);

    setgraphchar(ti);
}

export fn tty_write_sc() void {
    var r: ScreenRenderer = .init();
    c.wc_putc_init(c.WcOption, c.InnerCharset, c.DisplayCharset);
    for (0..@as(usize, @intCast((g.LINES - 1)))) |i| {
        r.render_line(i, c.sc_getline(i));
    }
    const span = c.wc_putc_end();
    if (span.ptr != null and span.len > 0) {
        tty.tty_write(span.ptr, span.len);
    }
    const str = std.mem.span(es_move(&c.terminfo, c.sc_curline(), c.sc_curcol()));
    tty.tty_write(str.ptr, str.len);
    tty.tty_flush();
}

export fn set_int() void {
    //     signal(SIGHUP, reset_exit);
    //     signal(SIGINT, reset_exit);
    //     signal(SIGQUIT, reset_exit);
    //     signal(SIGTERM, reset_exit);
    //     signal(SIGILL, error_dump);
    //     signal(SIGIOT, error_dump);
    //     signal(SIGFPE, error_dump);
    // #ifdef SIGBUS
    //     signal(SIGBUS, error_dump);
    // #endif /* SIGBUS */
    //     /* signal(SIGSEGV, error_dump); */
}

export fn initscr() void {
    set_int();
    getTCstr(&c.terminfo);
    if (c.terminfo.T_ti != null and 0 == g.Do_not_use_ti_te) {
        es_writestr(c.terminfo.T_ti);
    }
    c.sc_init(.{ .x = @intCast(g.COLS), .y = @intCast(g.LINES) });
}

export fn tty_reset() void {
    terminfo_reset(&c.terminfo, g.Do_not_use_ti_te != 0);
    tty.tty_clear();
}
