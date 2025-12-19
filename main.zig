const std = @import("std");
const c = @cImport({
    @cInclude("w3m_rc.h");
    @cInclude("termcap.h");
    @cInclude("image.h");
    @cInclude("terms.h");
    @cInclude("download.h");
});

const Term = @import("Term.zig");
var g_term: Term = undefined;

/// return ture if enter main loop
extern fn w3m_args(argc: c_int, argv: [*c]const [*:0]u8) bool;

extern fn w3m_idle() void;

pub fn main() void {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.detectLeaks();
    const allocator = gpa.allocator();

    g_term = Term.init(allocator, std.fs.File.stdin()) catch
        @panic("Term.init");
    defer g_term.deinit();

    if (!w3m_args(@intCast(std.os.argv.len), &std.os.argv[0])) {
        return;
    }

    while (true) {
        c.download_update();

        if (c.currentBufferSubmit()) {
            continue;
        }

        if (c.eventUpdate()) {
            continue;
        }

        // get keypress event
        const ch = g_term.getch(&w3m_idle);
        c.w3m_on_key(ch);
        c.w3m_end_frame();
    }
}

//
// input(rawmode)
//

export fn fmInitialized() bool {
    return g_term.is_rawmode;
}

export fn enterRawMode() void {
    if (!g_term.is_rawmode) {
        // term_raw();
        // term_noecho();
        g_term.enterRawMode() catch @panic("enterRawMode");
        c.initscr();
        c.initImage();
    }
}

export fn exitRawMode() void {
    if (g_term.is_rawmode) {
        c.screen_move(c.LASTLINE(), 0);
        c.screen_clrtoeolx();
        c.refresh();
        c.loadImage(null, c.IMG_FLAG_STOP);
        reset_tty();
    }
}

export fn reset_tty() void {
    const g_runtime: *c.Runtime = c.getRuntime() orelse {
        unreachable;
    };
    writestr(g_runtime.T_op); // turn off
    writestr(g_runtime.T_me);
    if (g_runtime.Do_not_use_ti_te) {
        if (g_runtime.T_te != null and g_runtime.T_te[0] != 0) {
            writestr(g_runtime.T_te);
        } else {
            writestr(g_runtime.T_cl);
        }
    }
    writestr(g_runtime.T_se); // reset terminal
    flush_tty();
    // tcsetattr(g_runtime.tty_input, TCSANOW, &d_ioval);
    g_term.exitRawMode();
}

export fn tty_add_ISIG() void {
    // ttymode_set(ISIG, 0);
}

export fn tty_remove_ISIG() void {
    // ttymode_reset(ISIG, 0);
}

// void ttymode_set(int mode, int imode)
// {
//     struct termios ioval;
//     tcgetattr(g_runtime.tty_input, &ioval);
//     ioval.c_lflag |= mode;
//     ioval.c_iflag |= imode;
//     while (tcsetattr(g_runtime.tty_input, TCSANOW, &ioval) == -1) {
//         if (errno == EINTR || errno == EAGAIN)
//             continue;
//         printf("Error occurred while set %x: errno=%d\n", mode, errno);
//         reset_error_exit(SIGNAL_ARGLIST);
//     }
// }
//
// void ttymode_reset(int mode, int imode)
// {
//     struct termios ioval;
//     tcgetattr(g_runtime.tty_input, &ioval);
//     ioval.c_lflag &= ~mode;
//     ioval.c_iflag &= ~imode;
//     while (tcsetattr(g_runtime.tty_input, TCSANOW, &ioval) == -1) {
//         if (errno == EINTR || errno == EAGAIN)
//             continue;
//         printf("Error occurred while reset %x: errno=%d\n", mode, errno);
//         reset_error_exit(SIGNAL_ARGLIST);
//     }
// }
//
// void set_cc(int spec, int val)
// {
//     struct termios ioval;
//     tcgetattr(g_runtime.tty_input, &ioval);
//     ioval.c_cc[spec] = val;
//     while (tcsetattr(g_runtime.tty_input, TCSANOW, &ioval) == -1) {
//         if (errno == EINTR || errno == EAGAIN)
//             continue;
//         printf("Error occurred: errno=%d\n", errno);
//         reset_error_exit(SIGNAL_ARGLIST);
//     }
// }

// void crmode(void)
// {
//     ttymode_reset(ICANON, IXON);
//     ttymode_set(ISIG, 0);
//     set_cc(VMIN, 1);
// }
//
// void nocrmode(void)
// {
//     ttymode_set(ICANON, 0);
//     set_cc(VMIN, 4);
// }
//
// void term_echo(void)
// {
//     ttymode_set(ECHO, 0);
// }
//
// void term_noecho(void)
// {
//     ttymode_reset(ECHO, 0);
// }

// #define TTY_MODE ISIG | ICANON | ECHO | IEXTEN
// export fn term_raw() void {
//     ttymode_reset(TTY_MODE, IXON | IXOFF | INLCR | IGNCR | ICRNL);
//     set_cc(VMIN, 1);
// }

// void term_cooked(void)
// {
//     ttymode_set(TTY_MODE, 0);
//     set_cc(VMIN, 4);
// }

// export fn term_cbreak() void {
// term_cooked();
// term_noecho();
// }

export fn getch() c_int {
    return @intCast(g_term.getch(&w3m_idle));
}

//
// output
//
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

//
// size
//

export fn setlinescols() void {
    // struct winsize wins;
    // int i = ioctl(g_runtime.tty_input, TIOCGWINSZ, &wins);
    // if (i >= 0 && wins.ws_row != 0 && wins.ws_col != 0) {
    //     g_runtime.lines = wins.ws_row;
    //     g_runtime.cols = wins.ws_col;
    // }

    if (g_term.getWinsize()) |ws| {
        const rt = c.getRuntime();
        rt.*.lines = ws.row;
        rt.*.cols = ws.col;
    } else |e| {
        @panic(@errorName(e));
    }
}

//
// image
//

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
