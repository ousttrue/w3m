const std = @import("std");
const c = @cImport({
    @cInclude("Str.h");
});
const GcAllocator = @import("GcAllocator.zig");

pub export fn Strnew() c.Str {
    const allocator = GcAllocator.allocator();
    const x = allocator.create(c._Str) catch @panic("OOM");
    x.* = .{
        .ptr = &(allocator.alloc(u8, c.INITIALStr_SIZE) catch @panic("OOM"))[0],
        .area_size = c.INITIALStr_SIZE,
        .length = 0,
    };
    x.*.ptr[0] = 0;
    return x;
}
