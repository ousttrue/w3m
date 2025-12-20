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

pub fn main() !void {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.detectLeaks();
    const allocator = gpa.allocator();

    g_term = Term.init(allocator, std.fs.File.stdin()) catch
        @panic("Term.init");
    defer g_term.deinit();

    const ws = try g_term.getWinsize();
    c.screen_setup(ws.row, ws.col);

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
        c.tty_refresh();
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

export fn tty_cbreak(enable: bool) void {
    if (enable) {
        g_term.cbreakMode() catch @panic("tty_cbreak");
    } else {
        g_term.enterRawMode() catch @panic("tty_cbreak");
    }
}

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
    // if (i >= 0 and wins.ws_row != 0 and wins.ws_col != 0) {
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

export fn get_pixel_per_cell(ppc: *c_int, ppl: *c_int) bool {
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
    //     if (ioctl(g_runtime.tty_input, TIOCGWINSZ, &ws) == 0 and ws.ws_ypixel > 0 and ws.ws_row > 0 and ws.ws_xpixel > 0 and ws.ws_col > 0) {
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
    //             if (wp > 0 and wc > 0 and hp > 0 and hc > 0) {
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
    return false;
}

//
// screen
//

var g_screen: c.Screen = .{
    .lines = null,
    .line_count = 0,
    .line_capacity = 0,
    .col_count = 0,
    .col_capacity = 0,
    .tab_step = 8,
    .y = 0,
    .x = 0,
    .mode = 0,
};
export fn screen_get() *c.Screen {
    return &g_screen;
}

const RefreshStatus = enum {
    RF_NEED_TO_MOVE,
    RF_CR_OK,
    RF_NONEED_TO_MOVE,
};

const M_MEND = (c.S_STANDOUT | c.S_UNDERLINE | c.S_BOLD | c.S_COLORED | c.S_BCOLORED | c.S_GRAPHICS);

fn color_seq(buf: []u8, colmode: c_int, highIntensityColors: bool) [:0]const u8 {
    // static char seqbuf[32];
    // sprintf(seqbuf, "\033[%dm", ((colmode >> 8) & 7) + (highIntensityColors ? 90 : 30));
    // return seqbuf;
    var val: c_int = (if (highIntensityColors) 90 else 30);
    val += ((colmode >> 8) & 7);
    return std.fmt.bufPrintZ(buf, "\x1b[{}m", .{val}) catch @panic("color_seq");
}

fn bcolor_seq(buf: []u8, colmode: c_int) [:0]const u8 {
    // static char seqbuf[32];
    // sprintf(seqbuf, "\033[%dm", ((colmode >> 12) & 7) + 40);
    // return seqbuf;
    const val = ((colmode >> 12) & 7) + 40;
    return std.fmt.bufPrintZ(buf, "\x1b[{}m", .{val}) catch @panic("bcolor_seq");
}

export fn tty_refresh() void {
    const sc: *c.Screen = c.screen_get();

    // int line, col;
    var pline: usize = sc.y;
    var moved = RefreshStatus.RF_NEED_TO_MOVE;
    var mode: c.ScreenCellProperty = 0;
    var color = c.COL_FTERM;
    var bcolor = c.COL_BTERM;
    var graph_enabled = false;

    const g_runtime: *c.Runtime = c.getRuntime();
    c.wc_putc_init(g_runtime.InnerCharset, g_runtime.DisplayCharset);

    for (0..sc.line_count) |line| {
        const pLine = &sc.lines[line];
        const p = pLine.cells;
        const dirty = &pLine.isdirty;
        if (dirty.* & c.L_DIRTY != 0) {
            dirty.* &= ~c.L_DIRTY;
            var col: usize = 0;
            while (col < sc.col_count) : (col += 1) {
                if (p[col].prop & c.S_EOL != 0) {
                    break;
                }
                if (dirty.* & c.L_NEED_CE != 0 and col >= sc.lines[line].eol) {
                    if (c.screen_need_redraw(
                        &p[col].str[0],
                        p[col].prop,
                        @ptrCast(&c.SCREEN_SPACE[0]),
                        0,
                    ))
                        break;
                } else {
                    if (p[col].prop & c.S_DIRTY != 0)
                        break;
                }
            }
            var pcol: usize = undefined;
            if (dirty.* & (c.L_NEED_CE | c.L_CLRTOEOL) != 0) {
                pcol = sc.lines[line].eol;
                if (pcol >= sc.col_count) {
                    dirty.* &= ~(c.L_NEED_CE | c.L_CLRTOEOL);
                    pcol = col;
                }
            } else {
                pcol = col;
            }
            if (line < sc.line_count - 2 and (line > 0 and pline == line - 1) and pcol == 0) {
                switch (moved) {
                    .RF_NEED_TO_MOVE => {
                        c.tty_MOVE(@intCast(line), 0);
                        moved = .RF_CR_OK;
                    },
                    .RF_CR_OK => {
                        _ = write1('\n');
                        _ = write1('\r');
                    },
                    .RF_NONEED_TO_MOVE => {
                        moved = .RF_CR_OK;
                    },
                }
            } else {
                c.tty_MOVE(@intCast(line), @intCast(pcol));
                moved = .RF_CR_OK;
            }
            if (dirty.* & (c.L_NEED_CE | c.L_CLRTOEOL) != 0) {
                writestr(g_runtime.T_ce);
                if (col != pcol)
                    c.tty_MOVE(@intCast(line), @intCast(col));
            }
            pline = line;
            pcol = col;
            while (col < g_runtime.cols) : (col += 1) {
                if (p[col].prop & c.S_EOL != 0)
                    break;

                // some terminal emulators do linefeed when a
                // character is put on COLS-th column. this behavior
                // is different from one of vt100, but such terminal
                // emulators are used as vt100-compatible
                // emulators. This behaviour causes scroll when a
                // character is drawn on (COLS-1,LINES-1) point.  To
                // avoid the scroll, I prohibit to draw character on
                // (COLS-1,LINES-1).
                if ((0 == (p[col].prop & c.S_STANDOUT) and (mode & c.S_STANDOUT) != 0) //
                or (0 == (p[col].prop & c.S_UNDERLINE) and (mode & c.S_UNDERLINE) != 0) //
                or (0 == (p[col].prop & c.S_BOLD) and (mode & c.S_BOLD) != 0) //
                or (0 == (p[col].prop & c.S_COLORED) and (mode & c.S_COLORED) != 0) //
                or (0 == (p[col].prop & c.S_BCOLORED) and (mode & c.S_BCOLORED) != 0) //
                or (0 == (p[col].prop & c.S_GRAPHICS) and (mode & c.S_GRAPHICS) != 0)) {
                    if ((mode & c.S_COLORED) != 0 or (mode & c.S_BCOLORED) != 0)
                        writestr(g_runtime.T_op);
                    if (mode & c.S_GRAPHICS != 0)
                        writestr(g_runtime.T_ae);
                    writestr(g_runtime.T_me);
                    mode &= ~M_MEND;
                }
                if (if (dirty.* & c.L_NEED_CE != 0 and col >= sc.lines[line].eol)
                    c.screen_need_redraw(
                        &p[col].str[0],
                        p[col].prop,
                        c.SCREEN_SPACE,
                        0,
                    )
                else
                    (p[col].prop & c.S_DIRTY != 0))
                {
                    if (col > 0 and pcol == col - 1) {
                        writestr(g_runtime.T_nd);
                    } else if (pcol != col) {
                        c.tty_MOVE(@intCast(line), @intCast(col));
                    }

                    if ((p[col].prop & c.S_STANDOUT != 0) and 0 == (mode & c.S_STANDOUT)) {
                        writestr(g_runtime.T_so);
                        mode |= c.S_STANDOUT;
                    }
                    if ((p[col].prop & c.S_UNDERLINE != 0) and 0 == (mode & c.S_UNDERLINE)) {
                        writestr(g_runtime.T_us);
                        mode |= c.S_UNDERLINE;
                    }
                    if ((p[col].prop & c.S_BOLD != 0) and 0 == (mode & c.S_BOLD)) {
                        writestr(g_runtime.T_md);
                        mode |= c.S_BOLD;
                    }
                    if ((p[col].prop & c.S_COLORED != 0) and (p[col].prop ^ mode) & c.COL_FCOLOR != 0) {
                        color = (p[col].prop & c.COL_FCOLOR);
                        mode = ((mode & ~c.COL_FCOLOR) | color);
                        var buf: [32]u8 = undefined;
                        writestr(color_seq(&buf, color, g_runtime.highIntensityColors != 0).ptr);
                    }
                    if ((p[col].prop & c.S_BCOLORED != 0) and (p[col].prop ^ mode) & c.COL_BCOLOR != 0) {
                        bcolor = (p[col].prop & c.COL_BCOLOR);
                        mode = ((mode & ~c.COL_BCOLOR) | bcolor);
                        var buf: [32]u8 = undefined;
                        writestr(bcolor_seq(&buf, bcolor).ptr);
                    }

                    if ((p[col].prop & c.S_GRAPHICS != 0) and 0 == (mode & c.S_GRAPHICS)) {
                        c.wc_putc_end(getOutputHandle());
                        if (!graph_enabled) {
                            graph_enabled = true;
                            writestr(g_runtime.T_eA);
                        }
                        writestr(g_runtime.T_as);
                        mode |= c.S_GRAPHICS;
                    }
                    if (p[col].prop & c.S_GRAPHICS != 0) {
                        _ = write1(c.graphchar(p[col].str[0]));
                    } else if (c.CHAR_MODE(p[col].prop) != c.C_WCHAR2) {
                        c.wc_putc(&p[col].str[0], getOutputHandle());
                    }
                    pcol = col + 1;
                }
            }
            if (col == g_runtime.cols)
                moved = .RF_NEED_TO_MOVE;
            while (col < g_runtime.cols and 0 == (p[col].prop & c.S_EOL)) : (col += 1) {
                p[col].prop |= c.S_EOL;
            }
        }
        dirty.* &= ~(c.L_NEED_CE | c.L_CLRTOEOL);
        if (mode & M_MEND != 0) {
            if (mode & (c.S_COLORED | c.S_BCOLORED) != 0)
                writestr(g_runtime.T_op);
            if (mode & c.S_GRAPHICS != 0) {
                writestr(g_runtime.T_ae);
                c.wc_putc_clear_status();
            }
            writestr(g_runtime.T_me);
            mode &= ~M_MEND;
        }
    }
    c.wc_putc_end(getOutputHandle());
    c.tty_MOVE(@intCast(sc.y), @intCast(sc.x));

    flush_tty();
}
