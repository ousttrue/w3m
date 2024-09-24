const std = @import("std");

const cursor = "\x1b[%i%p1%d;%p2%dH";
var buf: [1024]u8 = undefined;

export fn tgoto(cap: [*c]const u8, col: c_int, row: c_int) *u8 {
    if (cap == null) {
        unreachable;
    }
    if (std.mem.eql(u8, cursor, std.mem.span(cap))) {
        _ = std.fmt.bufPrintZ(&buf, "\x1b[{};{}H", .{ row, col }) catch {
            unreachable;
        };
    } else {
        unreachable;
    }
    return &buf[0];
}
