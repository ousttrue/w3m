const std = @import("std");
const c = @import("c.zig").c;
const runtime = @import("runtime.zig");

buf: std.ArrayList(u8) = .initBuffer(&.{}),

pub fn create(allocator: std.mem.Allocator) !*@This() {
    const this = try allocator.create(@This());
    this.* = .{};
    return this;
}

pub fn destroy(this: *@This(), allocator: std.mem.Allocator) void {
    this.buf.deinit(allocator);
    allocator.destroy(this);
}

/// exclude 0 terminator
pub fn strView(this: *@This()) c.str_view {
    var gv: c.str_view = .{
        .ptr = this.buf.items.ptr,
        .len = this.buf.items.len,
    };
    while (gv.len > 0 and gv.ptr[gv.len - 1] == 0) {
        gv.len -= 1;
    }
    return gv;
}
