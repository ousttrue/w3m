const std = @import("std");
const c = @import("c.zig").c;
const g = @import("global.zig");
const runtime = @import("runtime.zig");

const SPACE = " ";
var tab_step: usize = 8;

pub var CurrentMode: c.CellMode = .{};

pub fn is_mend(mode: c.CellMode) bool {
    if (mode.prop.S_STANDOUT | mode.prop.S_UNDERLINE | mode.prop.S_BOLD | mode.prop.S_GRAPHICS) {
        return true;
    }
    if (mode.fg != c.ANSI_TERM or mode.bg != c.ANSI_TERM) {
        return true;
    }
    return false;
}

pub fn remove_mend(mode: *c.CellMode) void {
    mode.prop = .{};
    mode.fg = c.ANSI_TERM;
    mode.bg = c.ANSI_TERM;
}

pub fn sc_cell_set(_cell: ?*c.Cell, ch: [*c]const u8, len: usize, mode: c.CellMode) void {
    const cell = _cell orelse @panic("null_cell");
    const allocator = runtime.allocator;

    cell.bytes = (allocator.dupeZ(u8, ch[0..len]) catch @panic("OOM")).ptr; //realloc((void*)cell.bytes, len + 1);
    // strncpy((char*)cell.bytes, (const char*)ch, len + 1);
    // mode.S_DIRTY = cell.mode.S_DIRTY;
    cell.mode = mode;
    cell.mode.S_DIRTY = true;
}

pub fn sc_cell_need_redraw(_cell: ?*c.Cell, c2: [*c]const u8, pr2: c.CellMode) bool {
    const cell = _cell orelse @panic("null _cell");
    if (cell.*.bytes == null or c2 == null or !std.mem.eql(u8, std.mem.span(cell.*.bytes), std.mem.span(c2)))
        return true;
    if (cell.*.bytes[0] == ' ') {
        if (!std.meta.eql(cell.mode.prop, pr2.prop)) {
            return true;
        }
        if (cell.mode.fg != pr2.fg) {
            return true;
        }
        if (cell.mode.bg != pr2.bg) {
            return true;
        }
        return false;
    }

    if (!std.meta.eql(cell.mode.prop, pr2.prop))
        return true;
    if (cell.mode.fg != pr2.fg) {
        return true;
    }
    if (cell.mode.bg != pr2.bg) {
        return true;
    }

    return false;
}
// static struct Usize2 size = { .x = 0, .y = 0 };
var lines: std.ArrayList(c.ScreenLine) = .initBuffer(&.{});
var cells: std.ArrayList(c.Cell) = .initBuffer(&.{});

fn touch_column(col: c_int) void {
    if (col >= 0 and col < sc_cols())
        sc_getline(@intCast(g.CurLine)).cells[@intCast(col)].mode.S_DIRTY = true;
}

export fn sc_cols() usize {
    return cells.items.len / lines.items.len;
}

export fn sc_init(size: c.Usize2) void {
    // size = _size;
    if (size.x == 0 or size.y == 0) {
        return;
    }

    const allocator = runtime.allocator;
    lines.resize(allocator, size.y) catch @panic("OOM");
    cells.resize(allocator, size.y * size.x) catch @panic("OOM");

    var begin: usize = 0;
    for (lines.items) |*line| {
        defer begin += size.x;
        line.* = .{
            .cells = &cells.items[begin],
        };
        for (0..size.x) |x| {
            line.cells[x] = .{
                .mode = .{
                    .S_EOL = true,
                },
            };
        }
    }

    c.sc_clear();
}

export fn sc_clear() void {
    c.sc_move(0, 0);
    for (lines.items) |*line| {
        line.isdirty = .{};
        for (0..sc_cols()) |x| {
            line.cells[x].mode.S_EOL = true;
        }
    }
    // CurrentMode.charmode = C_ASCII;
}

pub fn sc_getline(i: usize) *c.ScreenLine {
    return &lines.items[i];
}

fn sc_wrap() void {
    if (g.CurLine == (lines.items.len - 1))
        return;
    g.CurLine += 1;
    g.CurColumn = 0;
}

export fn sc_addmch(_src: ?[*]const u8, len: usize) void {
    const src = _src orelse {
        return;
    };
    if (len == 0) {
        return;
    }

    if (g.CurColumn == sc_cols())
        sc_wrap();
    if (g.CurColumn >= sc_cols())
        return;

    var cur_line = &lines.items[@intCast(g.CurLine)];
    const cur_cell = &cur_line.cells[@intCast(g.CurColumn)];
    if (cur_cell.mode.S_EOL) {
        if (src[0] == ' ') {
            if (!CurrentMode.prop.S_STANDOUT and
                !CurrentMode.prop.S_BOLD and
                !CurrentMode.prop.S_UNDERLINE and
                !CurrentMode.prop.S_GRAPHICS and
                CurrentMode.fg == c.ANSI_TERM and
                CurrentMode.bg == c.ANSI_TERM)
            {
                g.CurColumn += 1;
                return;
            }
        }
        var i = g.CurColumn;
        while (i >= 0 and cur_line.cells[@intCast(i)].mode.S_EOL) : (i -= 1) {
            const cell = &cur_line.cells[@intCast(i)];
            var mode = cell.mode;
            mode.prop = .{};
            mode.fg = c.ANSI_TERM;
            mode.bg = c.ANSI_TERM;
            mode.charmode = c.C_ASCII;
            sc_cell_set(cell, SPACE, 1, mode);
        }
    }

    if (src[0] == '\t' or src[0] == '\n' or src[0] == '\r' or src[0] == 0x08) {
        CurrentMode.charmode = c.C_ASCII;
        CurrentMode.C_CTRL = true;
    } else if (len > 1) {
        CurrentMode.charmode = c.C_WCHAR1;
        CurrentMode.C_CTRL = false;
    } else if (0 == c.IS_CNTRL(src[0])) {
        CurrentMode.charmode = c.C_ASCII;
        CurrentMode.C_CTRL = false;
    } else {
        return;
    }

    // Required to erase bold or underlined character for some * terminal emulators. */
    const width = c.wtf_width(c.WcOption, src[0]);
    var i = g.CurColumn + @as(c_int, @intCast(width - 1));
    if (i < sc_cols() and
        (((cur_line.cells[@intCast(i)].mode.prop.S_BOLD) and
            sc_cell_need_redraw(&cur_line.cells[@intCast(i)], src, CurrentMode)) or
            ((cur_line.cells[@intCast(i)].mode.prop.S_UNDERLINE) and
                !(CurrentMode.prop.S_UNDERLINE))))
    {
        sc_touch_line();
        i += 1;
        if (i < sc_cols()) {
            touch_column(i);
            if (cur_line.cells[@intCast(i)].mode.S_EOL) {
                const cell = &cur_line.cells[@intCast(i)];
                var mode = cell.mode;
                mode.prop = .{};
                mode.fg = c.ANSI_TERM;
                mode.bg = c.ANSI_TERM;
                mode.charmode = c.C_ASCII;
                sc_cell_set(cell, SPACE, 1, mode);
            } else {
                i += 1;
                while (i < sc_cols() and cur_line.cells[@intCast(i)].mode.charmode == c.C_WCHAR2) : (i += 1)
                    touch_column(i);
            }
        }
    }

    if (@as(usize, @intCast(g.CurColumn)) + width > sc_cols()) {
        sc_touch_line();
        i = g.CurColumn;
        while (i < sc_cols()) : (i += 1) {
            const cell = &cur_line.cells[@intCast(i)];
            var mode = cell.mode;
            mode.charmode = c.C_ASCII;
            sc_cell_set(cell, SPACE, 1, mode);
            touch_column(i);
        }
        sc_wrap();
        if (@as(usize, @intCast(g.CurColumn)) + width > sc_cols())
            return;
        cur_line = sc_getline(@intCast(g.CurLine));
    }

    if (cur_line.cells[@intCast(g.CurColumn)].mode.charmode == c.C_WCHAR2) {
        sc_touch_line();
        i = g.CurColumn - 1;
        while (i >= 0) : (i -= 1) {
            const cell = &cur_line.cells[@intCast(i)];
            const l = cell.mode.charmode;
            var mode = cell.mode;
            mode.charmode = c.C_ASCII;
            sc_cell_set(cell, SPACE, 1, mode);
            touch_column(i);
            if (l != c.C_WCHAR2)
                break;
        }
    }

    if (!CurrentMode.C_CTRL) {
        if (sc_cell_need_redraw(&cur_line.cells[@intCast(g.CurColumn)], src, CurrentMode)) {
            sc_cell_set(&cur_line.cells[@intCast(g.CurColumn)], src, len, CurrentMode);
            sc_touch_line();
            touch_column(g.CurColumn);
            CurrentMode.charmode = c.C_WCHAR2;
            i = g.CurColumn + 1;
            while (i < g.CurColumn + @as(c_int, @intCast(width))) : (i += 1) {
                var mode = cur_line.cells[@intCast(g.CurColumn)].mode;
                mode.charmode = c.C_WCHAR2;
                sc_cell_set(&cur_line.cells[@intCast(i)], SPACE, 1, mode);
                touch_column(i);
            }
            while (i < sc_cols() and cur_line.cells[@intCast(i)].mode.charmode == c.C_WCHAR2) : (i += 1) {
                const cell = &cur_line.cells[@intCast(i)];
                var mode = cell.mode;
                mode.charmode = c.C_ASCII;
                sc_cell_set(cell, SPACE, 1, mode);
                touch_column(i);
            }
        }
        g.CurColumn += @as(c_int, @intCast(width));
    } else if (src[0] == '\t') {
        var dest = (@as(usize, @intCast(g.CurColumn)) + tab_step) / tab_step * tab_step;
        if (dest >= sc_cols()) {
            sc_wrap();
            sc_touch_line();
            dest = tab_step;
            cur_line = sc_getline(@intCast(g.CurLine));
        }
        i = g.CurColumn;
        while (i < dest) : (i += 1) {
            const cell = &cur_line.cells[@intCast(i)];
            if (sc_cell_need_redraw(cell, SPACE, CurrentMode)) {
                sc_cell_set(cell, SPACE, 1, CurrentMode);
                sc_touch_line();
                touch_column(i);
            }
        }
        g.CurColumn = i;
    } else if (src[0] == '\n') {
        sc_wrap();
    } else if (src[0] == '\r') { // Carriage return
        g.CurColumn = 0;
    } else if (src[0] == 0x08 and g.CurColumn > 0) { // Backspace
        g.CurColumn -= 1;
        while (g.CurColumn > 0 and cur_line.cells[@intCast(g.CurColumn)].mode.charmode == c.C_WCHAR2)
            g.CurColumn -= 1;
    }
}

export fn sc_addnstr_sup(_s: ?[*:0]const u8, n: usize) void {
    var s = _s orelse {
        return;
    };
    if (n == 0) {
        return;
    }
    var i: usize = 0;
    while (s[0] != 0) {
        const width = c.wtf_width(c.WcOption, s[0]);
        if (i + width > n)
            break;
        const len = c.wtf_len(s);
        sc_addmch(s, len);
        s += len;
        i += width;
    }
    while (i < n) : (i += 1) {
        c.sc_addch(' ');
    }
}

export fn sc_addnstr(_s: ?[*:0]const u8, n: usize) void {
    var s = _s orelse {
        return;
    };
    if (n == 0) {
        return;
    }
    var i: usize = 0;
    while (s[0] != 0) {
        const width = c.wtf_width(c.WcOption, s[0]);
        if (i + width > n)
            break;
        const len = c.wtf_len(s);
        sc_addmch(s, len);
        s += len;
        i += width;
    }
}

export fn sc_addstr(_s: ?[*:0]const u8) void {
    var s = _s orelse {
        return;
    };
    while (s[0] != 0) {
        const len = c.wtf_len(s);
        sc_addmch(s, len);
        s += len;
    }
}

// XXX: conflicts with curses's clrtoeol(3) ?
fn sc_clrtoeol() void { // Clear to the end of line
    const line = sc_getline(@intCast(g.CurLine));

    if (line.cells[@intCast(g.CurColumn)].mode.S_EOL)
        return;

    if ((!line.isdirty.L_NEED_CE and !line.isdirty.L_CLRTOEOL) or line.eol > g.CurColumn)
        line.eol = @intCast(g.CurColumn);

    line.isdirty.L_CLRTOEOL = true;
    sc_touch_line();
    var i = g.CurColumn;
    while (i < sc_cols() and !line.cells[@intCast(i)].mode.S_EOL) : (i += 1) {
        line.cells[@intCast(i)].mode.S_EOL = true;
        line.cells[@intCast(i)].mode.S_DIRTY = true;
    }
}

fn clrtoeol_with_bcolor() void {
    if (CurrentMode.bg == c.ANSI_TERM) {
        sc_clrtoeol();
        return;
    }
    const cli = g.CurLine;
    const cco = g.CurColumn;
    const pr = CurrentMode;
    CurrentMode.prop = .{};
    CurrentMode.charmode = c.C_ASCII;
    CurrentMode.fg = c.ANSI_TERM;
    CurrentMode.bg = c.ANSI_TERM;
    var i = g.CurColumn;
    while (i < sc_cols()) : (i += 1)
        c.sc_addch(' ');
    c.sc_move(cli, cco);
    CurrentMode = pr;
}

export fn sc_clrtoeolx() void {
    clrtoeol_with_bcolor();
}

fn clrtobot_eol(clrtoeol: anytype) void {
    const line = g.CurLine;
    const col = g.CurColumn;
    clrtoeol();
    g.CurColumn = 0;
    g.CurLine += 1;
    while (g.CurLine < lines.items.len) : (g.CurLine += 1)
        clrtoeol();
    g.CurLine = line;
    g.CurColumn = col;
}

export fn sc_clrtobotx() void {
    clrtobot_eol(sc_clrtoeolx);
}

export fn sc_setfcolor(color: c.AnsiColor) void {
    CurrentMode.fg = color;
}

var seqbuf: [32]u8 = undefined;

pub fn sc_color_seq(colmode: c.AnsiColor, highIntensityColors: bool) [*c]const u8 {
    var val: c_int = @intCast(colmode);
    val += if (highIntensityColors) 90 else 30;
    return (std.fmt.bufPrintZ(&seqbuf, "\x1b[{}m", .{val}) catch @panic("bufPrintZ")).ptr;
}

export fn sc_setbcolor(color: c.AnsiColor) void {
    CurrentMode.bg = color;
}

pub fn sc_bcolor_seq(colmode: c.AnsiColor) [*c]const u8 {
    var val: c_int = @intCast(colmode);
    val += 40;
    return (std.fmt.bufPrintZ(&seqbuf, "\x1b[{}m", .{val}) catch @panic("bufPrintZ")).ptr;
}

export fn sc_standout() void {
    CurrentMode.prop.S_STANDOUT = true;
}

export fn sc_standend() void {
    CurrentMode.prop.S_STANDOUT = false;
}

export fn sc_toggle_stand() void {
    const line = sc_getline(@intCast(g.CurLine));
    line.cells[@intCast(g.CurColumn)].mode.prop.S_STANDOUT = !line.cells[@intCast(g.CurColumn)].mode.prop.S_STANDOUT;
    if (line.cells[@intCast(g.CurColumn)].mode.charmode != c.C_WCHAR2) {
        var i = g.CurColumn + 1;
        while (line.cells[@intCast(i)].mode.charmode == c.C_WCHAR2) : (i += 1)
            line.cells[@intCast(i)].mode.prop.S_STANDOUT = !line.cells[@intCast(i)].mode.prop.S_STANDOUT;
    }
}

export fn sc_bold() void {
    CurrentMode.prop.S_BOLD = true;
}

export fn sc_boldend() void {
    CurrentMode.prop.S_BOLD = false;
}

export fn sc_underline() void {
    CurrentMode.prop.S_UNDERLINE = true;
}

export fn sc_underlineend() void {
    CurrentMode.prop.S_UNDERLINE = false;
}

export fn sc_graphstart() void {
    CurrentMode.prop.S_GRAPHICS = true;
}

export fn sc_graphend() void {
    CurrentMode.prop.S_GRAPHICS = false;
}

fn sc_touch_line() void {
    if (!sc_getline(@intCast(g.CurLine)).isdirty.L_DIRTY) {
        var i: usize = 0;
        while (i < sc_cols()) : (i += 1)
            sc_getline(@intCast(g.CurLine)).cells[i].mode.S_DIRTY = false;
        sc_getline(@intCast(g.CurLine)).isdirty.L_DIRTY = true;
    }
}

export fn sc_addch(ch: u8) void {
    var buf: [2]u8 = .{ ch, 0 };
    sc_addmch(&buf, 1);
}

export fn sc_move(line: c_int, column: c_int) void {
    if (line >= 0 and line < lines.items.len)
        g.CurLine = line;
    if (column >= 0 and column < sc_cols())
        g.CurColumn = column;
}

pub fn sc_curline() c_int {
    return g.CurLine;
}

pub fn sc_curcol() c_int {
    return g.CurColumn;
}
