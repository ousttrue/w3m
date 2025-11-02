const std = @import("std");
const c = @cImport({
    @cInclude("Str.h");
});
const GcAllocator = @import("GcAllocator.zig");

fn sizeFromLen(len: c_int) usize {
    if (len < c.INITIALStr_SIZE) {
        return c.INITIALStr_SIZE - 1;
    }
    if (len >= c.STR_SIZE_MAX) {
        return c.STR_SIZE_MAX - 1;
    }
    return @intCast(len);
}

export fn allocStr(_s: [*c]const u8, _len: c_int) [*c]u8 {
    const s = _s orelse return null;

    var len: usize = if (_len >= 0) @intCast(_len) else std.mem.len(s);
    if (len >= c.STR_SIZE_MAX)
        len = c.STR_SIZE_MAX - 1;

    const allocator = GcAllocator.allocator();
    const ptr = allocator.alloc(u8, len + 1) catch @panic("OOM");
    std.mem.copyForwards(u8, ptr, s[0..len]);
    ptr[len] = 0;
    return ptr.ptr;
}

/// size contains 0 terminate
fn emptyStrFromSize(size: usize) c.Str {
    std.debug.assert(size > 0);
    const allocator = GcAllocator.allocator();
    const x = allocator.create(c._Str) catch @panic("OOM");
    x.* = .{
        .ptr = &(allocator.alloc(u8, size) catch @panic("OOM"))[0],
        .area_size = size,
        .length = 0,
    };
    x.ptr[0] = 0;
    return x;
}

export fn Strnew() c.Str {
    return emptyStrFromSize(c.INITIALStr_SIZE);
}

export fn Strnew_size(len: c_int) c.Str {
    return emptyStrFromSize(sizeFromLen(len));
}
