const std = @import("std");
const c = @import("c.zig").c;
const g = @import("global.zig");
const lib = @import("lib.zig");
const tty = @import("tty.zig");

const MoveStatus = enum {
    RF_NEED_TO_MOVE,
    RF_CR_OK,
    RF_NONEED_TO_MOVE,
};

pline: usize,
moved: MoveStatus = .RF_NEED_TO_MOVE,
mode: c.CellProperty,
color: c.CellProperty,
bcolor: c.CellProperty,

pub fn init() @This() {
    return .{
        .pline = @intCast(c.sc_curline()),
        .mode = 0,
        .color = c.COL_FTERM,
        .bcolor = c.COL_BTERM,
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

const SPACE: [*c]const u8 = " ";

pub fn render_line(this: *@This(), i: usize) void {
    const line: *c.ScreenLine = c.sc_lines()[i];
    var dirty = line.isdirty;

    if (dirty & c.L_DIRTY != 0) {
        dirty &= ~c.L_DIRTY;
        const cells = line.cells;
        var col: usize = 0;
        while (col < g.COLS and 0 == (cells[col].prop & c.S_EOL)) : (col += 1) {
            if (dirty & c.L_NEED_CE != 0 and col >= line.eol) {
                if (c.sc_need_redraw(&cells[col], SPACE, 0))
                    break;
            } else {
                if (cells[col].prop & c.S_DIRTY != 0)
                    break;
            }
        }
        var pcol: usize = undefined;
        if (dirty & (c.L_NEED_CE | c.L_CLRTOEOL) != 0) {
            pcol = @intCast(line.eol);
            if (pcol >= g.COLS) {
                dirty &= ~(c.L_NEED_CE | c.L_CLRTOEOL);
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
        if (dirty & (c.L_NEED_CE | c.L_CLRTOEOL) != 0) {
            lib.es_writestr(c.terminfo.T_ce);
            if (col != pcol) {
                tty_write_str(lib.es_move(&c.terminfo, @intCast(i), @intCast(col)));
            }
        }
        this.pline = i;
        pcol = col;
        while (col < g.COLS) : (col += 1) {
            if (cells[col].prop & c.S_EOL != 0)
                break;

            // some terminal emulators do linefeed when a
            // character is put on COLS-th column. this behavior
            // is different from one of vt100, but such terminal
            // emulators are used as vt100-compatible
            // emulators. This behaviour causes scroll when a
            // character is drawn on (COLS-1,LINES-1) point.  To
            // avoid the scroll, I prohibit to draw character on
            // (COLS-1,LINES-1).
            if ((0 == (cells[col].prop & c.S_STANDOUT) and (this.mode & c.S_STANDOUT) != 0) or
                (0 == (cells[col].prop & c.S_UNDERLINE) and (this.mode & c.S_UNDERLINE) != 0) or
                (0 == (cells[col].prop & c.S_BOLD) and (this.mode & c.S_BOLD) != 0) or
                (0 == (cells[col].prop & c.S_COLORED) and (this.mode & c.S_COLORED) != 0) or
                (0 == (cells[col].prop & c.S_BCOLORED) and (this.mode & c.S_BCOLORED) != 0) or
                (0 == (cells[col].prop & c.S_GRAPHICS) and (this.mode & c.S_GRAPHICS) != 0))
            {
                if ((this.mode & c.S_COLORED) != 0 or (this.mode & c.S_BCOLORED) != 0)
                    lib.es_writestr(c.terminfo.T_op);
                if (this.mode & c.S_GRAPHICS != 0)
                    lib.es_writestr(c.terminfo.T_ae);
                lib.es_writestr(c.terminfo.T_me);
                this.mode &= ~c.M_MEND;
            }
            if (if (dirty & c.L_NEED_CE != 0 and col >= line.eol)
                c.sc_need_redraw(&cells[col], SPACE, 0)
            else
                (cells[col].prop & c.S_DIRTY) != 0)
            {
                if (col >= 1 and pcol == col - 1) {
                    lib.es_writestr(c.terminfo.T_nd);
                } else if (pcol != col) {
                    tty_write_str(lib.es_move(&c.terminfo, @intCast(i), @intCast(col)));
                }

                if ((cells[col].prop & c.S_STANDOUT) != 0 and 0 == (this.mode & c.S_STANDOUT)) {
                    lib.es_writestr(c.terminfo.T_so);
                    this.mode |= c.S_STANDOUT;
                }
                if ((cells[col].prop & c.S_UNDERLINE) != 0 and 0 == (this.mode & c.S_UNDERLINE)) {
                    lib.es_writestr(c.terminfo.T_us);
                    this.mode |= c.S_UNDERLINE;
                }
                if ((cells[col].prop & c.S_BOLD) != 0 and 0 == (this.mode & c.S_BOLD)) {
                    lib.es_writestr(c.terminfo.T_md);
                    this.mode |= c.S_BOLD;
                }
                if ((cells[col].prop & c.S_COLORED) != 0 and (cells[col].prop ^ this.mode) & c.COL_FCOLOR != 0) {
                    this.color = (cells[col].prop & c.COL_FCOLOR);
                    this.mode = ((this.mode & ~c.COL_FCOLOR) | this.color);
                    lib.es_writestr(c.sc_color_seq(this.color));
                }
                if ((cells[col].prop & c.S_BCOLORED) != 0 and (cells[col].prop ^ this.mode) & c.COL_BCOLOR != 0) {
                    this.bcolor = (cells[col].prop & c.COL_BCOLOR);
                    this.mode = ((this.mode & ~c.COL_BCOLOR) | this.bcolor);
                    lib.es_writestr(c.sc_bcolor_seq(this.bcolor));
                }
                if ((cells[col].prop & c.S_GRAPHICS) != 0 and 0 == (this.mode & c.S_GRAPHICS)) {
                    const span = c.wc_putc_end();
                    tty.tty_write(span.ptr, span.len);
                    if (!graph_enabled) {
                        graph_enabled = true;
                        lib.es_writestr(c.terminfo.T_eA);
                    }
                    lib.es_writestr(c.terminfo.T_as);
                    this.mode |= c.S_GRAPHICS;
                }
                if (cells[col].prop & c.S_GRAPHICS != 0) {
                    const buf: [2]u8 = .{
                        graphchar(cells[col].bytes[0]),
                        0,
                    };
                    lib.es_writestr(&buf);
                } else if (c.CHMODE(cells[col].prop) != c.C_WCHAR2) {
                    const span = c.wc_putc(c.WcOption, cells[col].bytes);
                    tty.tty_write(span.ptr, span.len);
                }
                pcol = col + 1;
            }
        }
        if (col == g.COLS)
            this.moved = .RF_NEED_TO_MOVE;
        while (col < g.COLS and 0 == (cells[col].prop & c.S_EOL)) : (col += 1) {
            cells[col].prop |= c.S_EOL;
        }
    }
    line.isdirty = dirty & ~(c.L_NEED_CE | c.L_CLRTOEOL);

    if (this.mode & c.M_MEND != 0) {
        if (this.mode & (c.S_COLORED | c.S_BCOLORED) != 0)
            lib.es_writestr(c.terminfo.T_op);
        if (this.mode & c.S_GRAPHICS != 0) {
            lib.es_writestr(c.terminfo.T_ae);
            c.wc_putc_clear_status();
        }
        lib.es_writestr(c.terminfo.T_me);
        this.mode &= ~c.M_MEND;
    }
}
