const std = @import("std");
const c = @cImport({
    @cInclude("Str.h");
    @cInclude("gc.h");
});
const GcAllocator = @import("GcAllocator.zig");

/// use GC_MALLOC_ATOMIC
export fn allocStr(_s: [*c]const u8, _len: c_int) [*c]u8 {
    const s = _s orelse return null;

    var len: usize = if (_len >= 0) @intCast(_len) else std.mem.len(s);
    if (len >= c.STR_SIZE_MAX)
        len = c.STR_SIZE_MAX - 1;

    const p: [*c]u8 = @ptrCast(c.GC_MALLOC_ATOMIC(len + 1));
    const ptr = p orelse @panic("OOM");
    std.mem.copyForwards(u8, ptr[0..len], s[0..len]);
    ptr[len] = 0;
    return ptr;
}

/// use GC_MALLOC_ATOMIC
fn allocStrBuf(_size: usize) []u8 {
    std.debug.assert(_size > 0);
    const size = @max(@min(_size, c.STR_SIZE_MAX), c.INITIALStr_SIZE);
    const p: [*c]u8 = @ptrCast(c.GC_MALLOC_ATOMIC(size));
    const ptr: [*]u8 = p orelse @panic("OOM");
    const buf = ptr[0..size];
    buf[0] = 0;
    return buf;
}

fn createStr(
    buf: []u8,
) c.Str {
    const allocator = GcAllocator.allocator();
    const x = allocator.create(c._Str) catch @panic("OOM");
    x.* = .{
        .ptr = buf.ptr,
        .area_size = buf.len,
        .length = std.mem.indexOf(u8, buf, &.{0}) orelse 0,
    };
    return x;
}

export fn Strnew() c.Str {
    return createStr(allocStrBuf(c.INITIALStr_SIZE));
}

test Strnew {
    const x = Strnew();
    try std.testing.expectEqualSlices(u8, "", std.mem.sliceTo(x.*.ptr, 0));
    try std.testing.expectEqual(0, x.*.length);
    try std.testing.expectEqual(c.INITIALStr_SIZE, x.*.area_size);
}

export fn Strnew_size(len: c_int) c.Str {
    return createStr(allocStrBuf(if (len < 0) 0 else @intCast(len)));
}

test Strnew_size {
    {
        const x = Strnew_size(4);
        try std.testing.expectEqualSlices(u8, "", std.mem.sliceTo(x.*.ptr, 0));
        try std.testing.expectEqual(0, x.*.length);
        try std.testing.expectEqual(c.INITIALStr_SIZE, x.*.area_size);
    }
    {
        const x = Strnew_size(40);
        try std.testing.expectEqualSlices(u8, "", std.mem.sliceTo(x.*.ptr, 0));
        try std.testing.expectEqual(0, x.*.length);
        try std.testing.expectEqual(40, x.*.area_size);
    }
    {
        // INT_MAX / 32
        const x = Strnew_size(@divTrunc(std.math.maxInt(c_int), 32) + 256);
        try std.testing.expectEqualSlices(u8, "", std.mem.sliceTo(x.*.ptr, 0));
        try std.testing.expectEqual(0, x.*.length);
        try std.testing.expectEqual(c.STR_SIZE_MAX, x.*.area_size);
    }
}

export fn Strnew_charp(_p: [*c]const u8) c.Str {
    const p = _p orelse return Strnew();

    const len = std.mem.len(p);
    const buf = allocStrBuf(len + 1);
    const copy_len = @min(len, buf.len - 1);
    std.mem.copyForwards(u8, buf[0..copy_len], p[0..copy_len]);
    buf[copy_len] = 0;

    const allocator = GcAllocator.allocator();
    const x = allocator.create(c._Str) catch @panic("OOM");
    x.* = .{
        .ptr = buf.ptr,
        .area_size = buf.len,
        .length = copy_len,
    };

    return x;
}

test Strnew_charp {
    const x = Strnew_charp("abc");
    try std.testing.expectEqualSlices(u8, "abc", std.mem.sliceTo(x.*.ptr, 0));
    try std.testing.expectEqual(3, x.*.length);
    try std.testing.expectEqual(c.INITIALStr_SIZE, x.*.area_size);
}
