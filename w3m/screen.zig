const std = @import("std");
const c = @import("c.zig").c;
const runtime = @import("runtime.zig");

export fn sc_cell_set(cell: *c.Cell, ch: [*c]const u8, len: usize, mode: c.CellMode) void {
    const allocator = runtime.allocator;

    cell.bytes = (allocator.dupeZ(u8, ch[0..len]) catch @panic("OOM")).ptr; //realloc((void*)cell.bytes, len + 1);
    // strncpy((char*)cell.bytes, (const char*)ch, len + 1);
    // mode.S_DIRTY = cell.mode.S_DIRTY;
    cell.mode = mode;
    cell.mode.S_DIRTY = true;
}

pub export fn sc_cell_need_redraw(_cell: ?*c.Cell, c2: [*c]const u8, pr2: c.CellMode) bool {
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

fn cols() usize {
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
        line.isdirty = 0;
        for (0..cols()) |x| {
            line.cells[x].mode.S_EOL = true;
        }
    }
    // CurrentMode.charmode = C_ASCII;
}

export fn sc_getline(i: usize) *c.ScreenLine {
    return &lines.items[i];
}
