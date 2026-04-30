const std = @import("std");

var buf: [128]u8 = undefined;
var pos: usize = undefined;

pub fn init() void {
    buf = std.mem.zeroes(@TypeOf(buf));
    pos = 0;
}

pub fn ptr() [*c]const u8 {
    return &buf[0];
}

pub export fn putc(ch: c_int) c_int {
    buf[pos] = @intCast(ch);
    pos += 1;
    return 1;
}
