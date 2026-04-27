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
pub fn span(this: *@This()) c.span {
    return .{
        .ptr = this.buf.items.ptr,
        .len = this.buf.items.len,
    };
}
