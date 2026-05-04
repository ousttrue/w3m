const std = @import("std");
const c = @import("c.zig").c;
const g = @import("global.zig");
const runtime = @import("runtime.zig");

const CellProperty = std.enums.EnumFieldStruct(enum {
    S_STANDOUT,
    S_UNDERLINE,
    S_BOLD,
    S_GRAPHICS,
}, bool, false);

const CharMode = enum {
    C_ASCII,
    C_WCHAR1,
    C_WCHAR2,
};

pub const CellMode = struct {
    prop: CellProperty = .{},
    charmode: CharMode = .C_ASCII,
    fg: c.AnsiColor = c.ANSI_TERM,
    bg: c.AnsiColor = c.ANSI_TERM,
    S_DIRTY: bool = false,
    S_EOL: bool = false,
    C_CTRL: bool = false,

    pub fn is_mend(mode: @This()) bool {
        if (mode.prop.S_STANDOUT | mode.prop.S_UNDERLINE | mode.prop.S_BOLD | mode.prop.S_GRAPHICS) {
            return true;
        }
        if (mode.fg != c.ANSI_TERM or mode.bg != c.ANSI_TERM) {
            return true;
        }
        return false;
    }

    pub fn remove_mend(mode: *@This()) void {
        mode.prop = .{};
        mode.fg = c.ANSI_TERM;
        mode.bg = c.ANSI_TERM;
    }
};

pub const Cell = struct {
    bytes: [:0]const u8 = &.{},
    mode: CellMode = .{},

    pub fn deinit(this: *@This(), allocator: std.mem.Allocator) void {
        if (this.bytes.len > 0) {
            allocator.free(this.bytes);
        }
    }

    pub fn set(this: *@This(), allocator: std.mem.Allocator, str: []const u8, mode: CellMode) !void {
        this.deinit(allocator);
        this.bytes = try allocator.dupeZ(u8, str); //realloc((void*)cell.bytes, len + 1);
        this.mode = mode;
        this.mode.S_DIRTY = true;
    }

    pub fn need_redraw(cell: *@This(), c2: []const u8, pr2: CellMode) bool {
        if (cell.bytes.len == 0 or c2.len == 0 or !std.mem.eql(u8, cell.bytes, c2))
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
};

const LineFlags = std.enums.EnumFieldStruct(enum {
    L_DIRTY,
    L_NEED_CE,
    L_CLRTOEOL,
}, bool, false);

pub const ScreenLine = struct {
    cells: []Cell,
    isdirty: LineFlags = .{},
    eol: usize = 0,

    fn touch(this: *@This()) void {
        if (!this.isdirty.L_DIRTY) {
            var i: usize = 0;
            while (i < sc_cols()) : (i += 1)
                this.cells[i].mode.S_DIRTY = false;
            this.isdirty.L_DIRTY = true;
        }
    }
};

const SPACE = " ";

var tab_step: usize = 8;
pub var CurLine: usize = 0;
pub var CurColumn: usize = 0;
pub var CurrentMode: CellMode = .{};

// static struct Usize2 size = { .x = 0, .y = 0 };
var lines: std.ArrayList(ScreenLine) = .initBuffer(&.{});
var cells: std.ArrayList(Cell) = .initBuffer(&.{});

fn sc_cols() usize {
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
            .cells = cells.items[begin .. begin + size.x],
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
    CurrentMode.charmode = .C_ASCII;
}

pub fn sc_getline(i: usize) *ScreenLine {
    return &lines.items[i];
}

fn sc_wrap() void {
    if (CurLine == (lines.items.len - 1))
        return;
    CurLine += 1;
    CurColumn = 0;
}

export fn sc_addmch(_src: ?[*]const u8, len: usize) void {
    const src = _src orelse {
        return;
    };
    if (len == 0) {
        return;
    }

    const allocator = runtime.allocator;

    if (CurColumn == sc_cols())
        sc_wrap();
    if (CurColumn >= sc_cols())
        return;

    var cur_line = &lines.items[CurLine];
    const cur_cell = &cur_line.cells[CurColumn];
    if (cur_cell.mode.S_EOL) {
        if (src[0] == ' ') {
            if (!CurrentMode.prop.S_STANDOUT and
                !CurrentMode.prop.S_BOLD and
                !CurrentMode.prop.S_UNDERLINE and
                !CurrentMode.prop.S_GRAPHICS and
                CurrentMode.fg == c.ANSI_TERM and
                CurrentMode.bg == c.ANSI_TERM)
            {
                CurColumn += 1;
                return;
            }
        }
        var i = CurColumn;
        while (i >= 0 and cur_line.cells[i].mode.S_EOL) : (i -= 1) {
            const cell = &cur_line.cells[i];
            var mode = cell.mode;
            mode.prop = .{};
            mode.fg = c.ANSI_TERM;
            mode.bg = c.ANSI_TERM;
            mode.charmode = .C_ASCII;
            cell.set(allocator, SPACE, mode) catch @panic("sc_cell_set");
            if (i == 0) {
                break;
            }
        }
    }

    if (src[0] == '\t' or src[0] == '\n' or src[0] == '\r' or src[0] == 0x08) {
        CurrentMode.charmode = .C_ASCII;
        CurrentMode.C_CTRL = true;
    } else if (len > 1) {
        CurrentMode.charmode = .C_WCHAR1;
        CurrentMode.C_CTRL = false;
    } else if (0 == c.IS_CNTRL(src[0])) {
        CurrentMode.charmode = .C_ASCII;
        CurrentMode.C_CTRL = false;
    } else {
        return;
    }

    // Required to erase bold or underlined character for some * terminal emulators. */
    const width = c.wtf_width(c.WcOption, src[0]);
    var i = CurColumn + width - 1;
    if (i < sc_cols() and
        (((cur_line.cells[i].mode.prop.S_BOLD) and
            cur_line.cells[i].need_redraw(src[0..len], CurrentMode)) or
            ((cur_line.cells[i].mode.prop.S_UNDERLINE) and
                !(CurrentMode.prop.S_UNDERLINE))))
    {
        lines.items[CurLine].touch();
        i += 1;
        if (i < sc_cols()) {
            cur_line.cells[i].mode.S_DIRTY = true;
            if (cur_line.cells[i].mode.S_EOL) {
                const cell = &cur_line.cells[i];
                var mode = cell.mode;
                mode.prop = .{};
                mode.fg = c.ANSI_TERM;
                mode.bg = c.ANSI_TERM;
                mode.charmode = .C_ASCII;
                cell.set(allocator, SPACE, mode) catch @panic("sc_cell_set");
            } else {
                i += 1;
                while (i < sc_cols() and cur_line.cells[i].mode.charmode == .C_WCHAR2) : (i += 1)
                    cur_line.cells[i].mode.S_DIRTY = true;
            }
        }
    }

    if (@as(usize, CurColumn) + width > sc_cols()) {
        lines.items[CurLine].touch();
        i = CurColumn;
        while (i < sc_cols()) : (i += 1) {
            const cell = &cur_line.cells[i];
            var mode = cell.mode;
            mode.charmode = .C_ASCII;
            cell.set(allocator, SPACE, mode) catch @panic("sc_cell_set");
            cur_line.cells[i].mode.S_DIRTY = true;
        }
        sc_wrap();
        if (@as(usize, CurColumn) + width > sc_cols())
            return;
        cur_line = sc_getline(CurLine);
    }

    if (cur_line.cells[CurColumn].mode.charmode == .C_WCHAR2) {
        lines.items[CurLine].touch();
        i = CurColumn - 1;
        while (i >= 0) : (i -= 1) {
            const cell = &cur_line.cells[i];
            const l = cell.mode.charmode;
            var mode = cell.mode;
            mode.charmode = .C_ASCII;
            cell.set(allocator, SPACE, mode) catch @panic("sc_cell_set");
            cur_line.cells[i].mode.S_DIRTY = true;
            if (l != .C_WCHAR2)
                break;
            if (i == 0) {
                break;
            }
        }
    }

    if (!CurrentMode.C_CTRL) {
        if (cur_line.cells[CurColumn].need_redraw(src[0..len], CurrentMode)) {
            cur_line.cells[CurColumn].set(allocator, src[0..len], CurrentMode) catch @panic("sc_cell_set");
            cur_line.touch();
            cur_line.cells[CurColumn].mode.S_DIRTY = true;
            CurrentMode.charmode = .C_WCHAR2;
            i = CurColumn + 1;
            while (i < CurColumn + width) : (i += 1) {
                var mode = cur_line.cells[CurColumn].mode;
                mode.charmode = .C_WCHAR2;
                cur_line.cells[i].set(allocator, SPACE, mode) catch @panic("sc_cell_set");
            }
            while (i < sc_cols() and cur_line.cells[i].mode.charmode == .C_WCHAR2) : (i += 1) {
                const cell = &cur_line.cells[i];
                var mode = cell.mode;
                mode.charmode = .C_ASCII;
                cell.set(allocator, SPACE, mode) catch @panic("sc_cell_set");
            }
        }
        CurColumn += width;
    } else if (src[0] == '\t') {
        var dest = CurColumn + tab_step / tab_step * tab_step;
        if (dest >= sc_cols()) {
            sc_wrap();
            cur_line.touch();
            dest = tab_step;
            cur_line = sc_getline(CurLine);
        }
        i = CurColumn;
        while (i < dest) : (i += 1) {
            const cell = &cur_line.cells[i];
            if (cell.need_redraw(SPACE, CurrentMode)) {
                cell.set(allocator, SPACE, CurrentMode) catch @panic("sc_cell_set");
                cur_line.touch();
                cur_line.cells[i].mode.S_DIRTY = true;
            }
        }
        CurColumn = i;
    } else if (src[0] == '\n') {
        sc_wrap();
    } else if (src[0] == '\r') { // Carriage return
        CurColumn = 0;
    } else if (src[0] == 0x08 and CurColumn > 0) { // Backspace
        CurColumn -= 1;
        while (CurColumn > 0 and cur_line.cells[CurColumn].mode.charmode == .C_WCHAR2)
            CurColumn -= 1;
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
    const line = sc_getline(CurLine);

    if (line.cells[CurColumn].mode.S_EOL)
        return;

    if ((!line.isdirty.L_NEED_CE and !line.isdirty.L_CLRTOEOL) or line.eol > CurColumn)
        line.eol = CurColumn;

    line.isdirty.L_CLRTOEOL = true;
    line.touch();
    var i = CurColumn;
    while (i < sc_cols() and !line.cells[i].mode.S_EOL) : (i += 1) {
        line.cells[i].mode.S_EOL = true;
        line.cells[i].mode.S_DIRTY = true;
    }
}

fn clrtoeol_with_bcolor() void {
    if (CurrentMode.bg == c.ANSI_TERM) {
        sc_clrtoeol();
        return;
    }
    const cli = CurLine;
    const cco = CurColumn;
    const pr = CurrentMode;
    CurrentMode.prop = .{};
    CurrentMode.charmode = .C_ASCII;
    CurrentMode.fg = c.ANSI_TERM;
    CurrentMode.bg = c.ANSI_TERM;
    var i = CurColumn;
    while (i < sc_cols()) : (i += 1)
        c.sc_addch(' ');
    c.sc_move(cli, cco);
    CurrentMode = pr;
}

export fn sc_clrtoeolx() void {
    clrtoeol_with_bcolor();
}

fn clrtobot_eol(clrtoeol: anytype) void {
    const line = CurLine;
    const col = CurColumn;
    clrtoeol();
    CurColumn = 0;
    CurLine += 1;
    while (CurLine < lines.items.len) : (CurLine += 1)
        clrtoeol();
    CurLine = line;
    CurColumn = col;
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
    const line = sc_getline(CurLine);
    line.cells[CurColumn].mode.prop.S_STANDOUT = !line.cells[CurColumn].mode.prop.S_STANDOUT;
    if (line.cells[CurColumn].mode.charmode != .C_WCHAR2) {
        var i = CurColumn + 1;
        while (line.cells[i].mode.charmode == .C_WCHAR2) : (i += 1)
            line.cells[i].mode.prop.S_STANDOUT = !line.cells[i].mode.prop.S_STANDOUT;
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

export fn sc_addch(ch: u8) void {
    var buf: [2]u8 = .{ ch, 0 };
    sc_addmch(&buf, 1);
}

pub export fn sc_move(line: usize, column: usize) void {
    if (line < lines.items.len)
        CurLine = line;
    if (column < sc_cols())
        CurColumn = column;
}

pub fn sc_curline() usize {
    return CurLine;
}

pub fn sc_curcol() usize {
    return CurColumn;
}
