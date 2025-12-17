const std = @import("std");
const c = @cImport({
    @cInclude("w3m_runtime.h");
    @cInclude("termcap.h");
});
const vaxis = @import("vaxis");

const RawMode = struct {
    const Event = union(enum) {
        key_press: vaxis.Key,
        winsize: vaxis.Winsize,
        // focus_in,
        // foo: u8,
    };

    buffer: [1024]u8 = undefined,
    tty: vaxis.Tty = undefined,
    vx: vaxis.Vaxis = undefined,
    loop: vaxis.Loop(Event) = undefined,
    fn create(allocator: std.mem.Allocator) !*@This() {
        const this = try allocator.create(@This());
        this.* = .{};
        this.tty = try vaxis.Tty.init(&this.buffer);
        this.vx = try vaxis.init(allocator, .{});
        this.loop = .{ .tty = &this.tty, .vaxis = &this.vx };
        try this.loop.init();
        try this.loop.start();
        return this;
    }

    fn destroy(this: *@This(), allocator: std.mem.Allocator) void {
        this.loop.stop();
        this.vx.deinit(allocator, this.tty.writer());
        this.tty.deinit();
    }
};

const Term = struct {
    allocator: std.mem.Allocator,
    rawmode: ?*RawMode = null,

    fn init(allocator: std.mem.Allocator) @This() {
        return .{
            .allocator = allocator,
        };
    }

    fn deinit(this: *const @This()) void {
        _ = this;
    }

    fn enterRawMode(this: *@This()) !void {
        if (this.rawmode == null) {
            this.rawmode = try RawMode.create(this.allocator);
        }
    }

    fn exitRawMode(this: *@This()) void {
        if (this.rawmode) |rawmode| {
            rawmode.destroy(this.allocator);
            this.rawmode = null;
        }
    }
};
var g_term: Term = undefined;

/// return ture if enter main loop
extern fn w3m_args(argc: c_int, argv: [*c]const [*:0]u8) bool;

extern fn w3m_loop() void;

pub fn main() void {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.detectLeaks();
    const allocator = gpa.allocator();

    g_term = Term.init(allocator);
    defer g_term.deinit();

    if (w3m_args(@intCast(std.os.argv.len), &std.os.argv[0])) {
        w3m_loop();
    }
}

export fn getOutputHandle() c_int {
    return std.fs.File.stdout().handle;
}

export fn flush_tty() void {
    // if (tty_output_f){
    //     fflush(tty_output_f);
    // }
}

export fn write1(ch: c_int) c_int {
    // putc(c, tty_output_f);
    // return write(g_runtime.tty_output, &c, 1);
    const p: [*]const u8 = @ptrCast(&ch);
    const result = std.fs.File.stdout().write(p[0..1]) catch @panic("write1");
    return @intCast(result);
}

// size_t writeN(const uint8_t *p, size_t n)
// {
//     // return fwrite(p, 1, n, tty_output_f);
//     return write(tty_output, p, n);
// }

export fn writestr(s: [*c]const u8) void {
    _ = c.tputs(s, 1, c.write1);
}

export fn get_pixel_per_cell(ppc: *c_int, ppl: *c_int) c_int {
    _ = ppc;
    _ = ppl;
    //     fd_set rfd;
    //     struct timeval tval;
    //     char buf[100];
    //     char* p;
    //     ssize_t len;
    //     ssize_t left;
    //     int wp, hp, wc, hc;
    //     int i;
    //
    //     struct winsize ws;
    //     if (ioctl(g_runtime.tty_input, TIOCGWINSZ, &ws) == 0 && ws.ws_ypixel > 0 && ws.ws_row > 0 && ws.ws_xpixel > 0 && ws.ws_col > 0) {
    //         *ppc = ws.ws_xpixel / ws.ws_col;
    //         *ppl = ws.ws_ypixel / ws.ws_row;
    //         return 1;
    //     }
    //
    //     const char* str = "\x1b[14t\x1b[18t";
    //     // fputs(, tty_output_f);
    //     write(g_runtime.tty_output, str, strlen(str));
    //     flush_tty();
    //
    //     p = buf;
    //     left = sizeof(buf) - 1;
    //     for (i = 0; i < 10; i++) {
    //         tval.tv_usec = 200000; /* 0.2 sec * 10 */
    //         tval.tv_sec = 0;
    //         FD_ZERO(&rfd);
    //         FD_SET(g_runtime.tty_input, &rfd);
    //         if (select(g_runtime.tty_input + 1, &rfd, NULL, NULL, &tval) <= 0 || !FD_ISSET(g_runtime.tty_input, &rfd))
    //             continue;
    //
    //         if ((len = read(g_runtime.tty_input, p, left)) <= 0)
    //             continue;
    //         p[len] = '\0';
    //
    //         if (sscanf(buf, "\x1b[4;%d;%dt\x1b[8;%d;%dt", &hp, &wp, &hc, &wc) == 4) {
    //             if (wp > 0 && wc > 0 && hp > 0 && hc > 0) {
    //                 *ppc = wp / wc;
    //                 *ppl = hp / hc;
    //                 return 1;
    //             } else {
    //                 return 0;
    //             }
    //         }
    //         p += len;
    //         left -= len;
    //     }
    //
    return 0;
}
