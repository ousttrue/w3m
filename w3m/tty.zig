const std = @import("std");
const c = @import("c.zig").c;
const g = @import("global.zig");
const runtime = @import("runtime.zig");
const TtyLinux = @import("TtyLinux.zig");
const Epoll = @import("Epoll.zig");
const lib = @import("lib.zig");

var tty: TtyLinux = undefined;
var tty_writer: std.Io.File.Writer = undefined;
var write_buf: [256]u8 = undefined;
pub var tty_in: std.Io.File = undefined;
var peek_queue: std.Deque(u8) = .initBuffer(&.{});
pub var epoll: Epoll = undefined;

pub fn init(io: std.Io) void {
    // tty = .create(
    //     runtime.allocator,
    //     runtime.evented.io(),
    //     std.Io.File.stdin(),
    //     std.Io.File.stdout(),
    // );
    tty = .init(std.Io.File.stdin());
    tty_writer = std.Io.File.stdout().writer(io, &write_buf);
    tty_in = std.Io.File.stdin();
    epoll = .init();
    epoll.add_fd(tty_in.handle);
}

pub fn deinit() void {
    tty_flush();
}

pub fn ttyname_tty() [*c]const u8 {
    return c.ttyname(tty.stdin.handle);
}

pub fn putc(ch: u8) !void {
    try tty_writer.interface.writeByte(ch);
}

pub fn puts(str: []const u8) !void {
    return try tty_writer.interface.writeAll(str);
}

pub export fn tty_flush() void {
    tty_writer.flush() catch {};
}

pub export fn tty_clear() void {
    lib.es_writestr(c.terminfo.T_cl);
    tty_flush();
    tty.restore();
}

export fn tty_bell() void {
    putc(7) catch @panic("putc");
}

pub export fn tty_write(_str: ?[*]const u8, len: usize) void {
    const str = _str orelse {
        return;
    };
    if (len == 0) {
        return;
    }
    puts(str[0..len]) catch {};
    tty_writer.flush() catch {};
}

export fn unget(ch: c_int) void {
    if (ch > 0) {
        peek_queue.pushBack(runtime.allocator, @intCast(ch)) catch @panic("peek_queue.pushBack");
    }
}

pub fn getTermSize() !c.winsize {
    var wins: c.winsize = undefined;
    const i = c.ioctl(tty.stdin.handle, c.TIOCGWINSZ, &wins);
    if (i >= 0 and wins.ws_row != 0 and wins.ws_col != 0) {
        return wins;
    } else {
        return error.TIOCGWINSZ;
    }
}

export fn tty_linescols() void {
    const wins = getTermSize() catch @panic("getTermSize");
    g.LINES = wins.ws_row;
    g.COLS = wins.ws_col;
}

export fn tty_crmode() void {
    tty.crmode(true) catch {};
}

export fn tty_echo() void {
    tty.echo(true) catch {};
}

export fn tty_noecho() void {
    tty.echo(false) catch {};
}

export fn tty_raw() void {
    tty.raw() catch {};
}

export fn tty_cbreak() void {
    tty.cooked(.{ .echo = false }) catch {};
}

export fn term_title(s: [*c]const u8) void {
    // @panic("not impl");
    _ = s;
    //     if (!fmInitialized)
    //         return;
    //     if (title_str != NULL) {
    //         fprintf(ttyf, title_str, s);
    //     }
}

export fn ttymode_add(mode: c_int, imode: c_int) void {
    _ = mode;
    _ = imode;
}
export fn ttymode_remove(mode: c_int, imode: c_int) void {
    _ = mode;
    _ = imode;
}

pub export fn tty_pixel_per_cell(ppc: *c_int, ppl: *c_int) bool {
    if (getTermSize()) |ws| {
        if (ws.ws_ypixel > 0 and ws.ws_row > 0 and ws.ws_xpixel > 0 and ws.ws_col > 0) {
            ppc.* = ws.ws_xpixel / ws.ws_col;
            ppl.* = ws.ws_ypixel / ws.ws_row;
            return true;
        }
    } else |_| {
        @panic("getTermSize");
    }

    // XTWINOPS
    //
    // fd_set rfd;
    // struct timeval tval;
    // char buf[100];
    // char* p;
    // ssize_t len;
    // ssize_t left;
    // int wp, hp, wc, hc;
    // int i;
    //
    // fputs("\x1b[14t\x1b[18t", ttyf);
    // tty_flush();
    //
    // p = buf;
    // left = sizeof(buf) - 1;
    // for (i = 0; i < 10; i++) {
    //     tval.tv_usec = 200000; /* 0.2 sec * 10 */
    //     tval.tv_sec = 0;
    //     FD_ZERO(&rfd);
    //     FD_SET(tty, &rfd);
    //     if (select(tty + 1, &rfd, NULL, NULL, &tval) <= 0 || !FD_ISSET(tty, &rfd))
    //         continue;
    //
    //     if ((len = read(tty, p, left)) <= 0)
    //         continue;
    //     p[len] = '\0';
    //
    //     if (sscanf(buf, "\x1b[4;%d;%dt\x1b[8;%d;%dt", &hp, &wp, &hc, &wc) == 4) {
    //         if (wp > 0 && wc > 0 && hp > 0 && hc > 0) {
    //             *ppc = wp / wc;
    //             *ppl = hp / hc;
    //             return 1;
    //         } else {
    //             return 0;
    //         }
    //     }
    //     p += len;
    //     left -= len;
    // }

    return false;
}


