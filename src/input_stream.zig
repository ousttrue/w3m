const std = @import("std");
const gcstr = @import("gcstr");
const c = @import("w3m.zig").c;

pub fn init_buffer(base: *c.base_stream, _buf: ?[*]u8, bufsize: usize) void {
    const sb = &base.stream;
    sb.size = @intCast(bufsize);
    sb.cur = 0;
    sb.buf = @ptrCast(c.xmalloc(bufsize));
    if (_buf) |buf| {
        std.mem.copyForwards(u8, sb.buf[0..bufsize], buf[0..bufsize]);
        sb.next = @intCast(bufsize);
    } else {
        sb.next = 0;
    }
    base.iseos = 0; //false;
    base.unclose = false;
}
