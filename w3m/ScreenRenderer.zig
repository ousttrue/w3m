const std = @import("std");
const c = @import("c.zig").c;
const g = @import("global.zig");
const lib = @import("lib.zig");
const tty = @import("tty.zig");
const screen = @import("screen.zig");

const MoveStatus = enum {
    RF_NEED_TO_MOVE,
    RF_CR_OK,
    RF_NONEED_TO_MOVE,
};

pline: usize,
moved: MoveStatus = .RF_NEED_TO_MOVE,
mode: screen.CellMode,

pub fn init() @This() {
    return .{
        .pline = @intCast(screen.sc_curline()),
        .mode = .{},
    };
}

var graph_enabled = false;

fn graphchar(ch: u8) u8 {
    return if (ch >= ' ' and ch < 128)
        c.terminfo.gcmap[ch - ' ']
    else
        @intCast(ch);
}

fn tty_write_str(str: [*c]const u8) void {
    const s = std.mem.span(str);
    tty.tty_write(s.ptr, s.len);
}

const SPACE = " ";

pub fn render_line(this: *@This(), i: usize, line: *screen.ScreenLine) void {
    var dirty = line.isdirty;

    if (dirty.L_DIRTY) {
        dirty.L_DIRTY = false;
        const cells = line.cells;
        var col: usize = 0;
        while (col < g.COLS and !cells[col].mode.S_EOL) : (col += 1) {
            if (dirty.L_NEED_CE and col >= line.eol) {
                if (screen.sc_cell_need_redraw(&cells[col], SPACE, .{}))
                    break;
            } else {
                if (cells[col].mode.S_DIRTY)
                    break;
            }
        }
        var pcol: usize = undefined;
        if (dirty.L_NEED_CE or dirty.L_CLRTOEOL) {
            pcol = @intCast(line.eol);
            if (pcol >= g.COLS) {
                dirty.L_NEED_CE = false;
                dirty.L_CLRTOEOL = false;
                pcol = col;
            }
        } else {
            pcol = col;
        }
        if (g.LINES >= 2 and this.pline >= 1 and i < g.LINES - 2 and i >= 1 and this.pline == i - 1 and pcol == 0) {
            switch (this.moved) {
                .RF_NEED_TO_MOVE => {
                    tty_write_str(lib.es_move(&c.terminfo, @intCast(i), @intCast(0)));
                    this.moved = .RF_CR_OK;
                },
                .RF_CR_OK => {
                    tty_write_str("\n");
                    tty_write_str("\r");
                },
                .RF_NONEED_TO_MOVE => {
                    this.moved = .RF_CR_OK;
                },
            }
        } else {
            tty_write_str(lib.es_move(&c.terminfo, @intCast(i), @intCast(pcol)));
            this.moved = .RF_CR_OK;
        }
        if (dirty.L_NEED_CE or dirty.L_CLRTOEOL) {
            lib.es_writestr(c.terminfo.T_ce);
            if (col != pcol) {
                tty_write_str(lib.es_move(&c.terminfo, @intCast(i), @intCast(col)));
            }
        }
        this.pline = i;
        pcol = col;
        while (col < g.COLS) : (col += 1) {
            if (cells[col].mode.S_EOL)
                break;

            // some terminal emulators do linefeed when a
            // character is put on COLS-th column. this behavior
            // is different from one of vt100, but such terminal
            // emulators are used as vt100-compatible
            // emulators. This behaviour causes scroll when a
            // character is drawn on (COLS-1,LINES-1) point.  To
            // avoid the scroll, I prohibit to draw character on
            // (COLS-1,LINES-1).
            if ((!cells[col].mode.prop.S_STANDOUT and this.mode.prop.S_STANDOUT) or
                (!cells[col].mode.prop.S_UNDERLINE and this.mode.prop.S_UNDERLINE) or
                (!cells[col].mode.prop.S_BOLD and this.mode.prop.S_BOLD) or
                (cells[col].mode.fg == c.ANSI_TERM and this.mode.fg != c.ANSI_TERM) or
                (cells[col].mode.bg == c.ANSI_TERM and this.mode.bg != c.ANSI_TERM) or
                (!cells[col].mode.prop.S_GRAPHICS and this.mode.prop.S_GRAPHICS))
            {
                if (this.mode.fg != c.ANSI_TERM or this.mode.bg != c.ANSI_TERM)
                    lib.es_writestr(c.terminfo.T_op);
                if (this.mode.prop.S_GRAPHICS)
                    lib.es_writestr(c.terminfo.T_ae);
                lib.es_writestr(c.terminfo.T_me);
                screen.remove_mend(&this.mode);
            }
            if (if (dirty.L_NEED_CE and col >= line.eol)
                screen.sc_cell_need_redraw(&cells[col], SPACE, .{})
            else
                (cells[col].mode.S_DIRTY))
            {
                if (col >= 1 and pcol == col - 1) {
                    lib.es_writestr(c.terminfo.T_nd);
                } else if (pcol != col) {
                    tty_write_str(lib.es_move(&c.terminfo, @intCast(i), @intCast(col)));
                }

                if (cells[col].mode.prop.S_STANDOUT and !this.mode.prop.S_STANDOUT) {
                    lib.es_writestr(c.terminfo.T_so);
                    this.mode.prop.S_STANDOUT = true;
                }
                if (cells[col].mode.prop.S_UNDERLINE and !this.mode.prop.S_UNDERLINE) {
                    lib.es_writestr(c.terminfo.T_us);
                    this.mode.prop.S_UNDERLINE = true;
                }
                if (cells[col].mode.prop.S_BOLD and !this.mode.prop.S_BOLD) {
                    lib.es_writestr(c.terminfo.T_md);
                    this.mode.prop.S_BOLD = true;
                }
                if (cells[col].mode.fg != c.ANSI_TERM and cells[col].mode.fg != this.mode.fg) {
                    this.mode.fg = cells[col].mode.fg;
                    lib.es_writestr(screen.sc_color_seq(this.mode.fg, g.highIntensityColors != 0));
                }
                if (cells[col].mode.bg != c.ANSI_TERM and cells[col].mode.bg != this.mode.bg) {
                    this.mode.bg = cells[col].mode.bg;
                    lib.es_writestr(screen.sc_bcolor_seq(this.mode.bg));
                }
                if (cells[col].mode.prop.S_GRAPHICS and !this.mode.prop.S_GRAPHICS) {
                    const span = c.wc_putc_end();
                    tty.tty_write(span.ptr, span.len);
                    if (!graph_enabled) {
                        graph_enabled = true;
                        lib.es_writestr(c.terminfo.T_eA);
                    }
                    lib.es_writestr(c.terminfo.T_as);
                    this.mode.prop.S_GRAPHICS = true;
                }
                if (cells[col].mode.prop.S_GRAPHICS) {
                    const buf: [2]u8 = .{
                        graphchar(cells[col].bytes[0]),
                        0,
                    };
                    lib.es_writestr(&buf);
                } else if (cells[col].mode.charmode != .C_WCHAR2) {
                    const span = c.wc_putc(c.WcOption, cells[col].bytes.ptr);
                    tty.tty_write(span.ptr, span.len);
                }
                pcol = col + 1;
            }
        }
        if (col == g.COLS)
            this.moved = .RF_NEED_TO_MOVE;
        while (col < g.COLS and !cells[col].mode.S_EOL) : (col += 1) {
            cells[col].mode.S_EOL = true;
        }
    }
    dirty.L_NEED_CE = false;
    dirty.L_CLRTOEOL = false;
    line.isdirty = dirty;

    if (screen.is_mend(this.mode)) {
        if (this.mode.fg != c.ANSI_TERM or this.mode.bg != c.ANSI_TERM)
            lib.es_writestr(c.terminfo.T_op);
        if (this.mode.prop.S_GRAPHICS) {
            lib.es_writestr(c.terminfo.T_ae);
            c.wc_putc_clear_status();
        }
        lib.es_writestr(c.terminfo.T_me);
        screen.remove_mend(&this.mode);
    }
}
