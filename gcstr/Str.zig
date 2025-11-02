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

export fn Strcat_charp_n(x: c.Str, _y: [*c]const u8, len: c_int) void {
    const y = _y orelse return;
    const copy_len: usize = if (len < 0) std.mem.len(y) else @intCast(len);
    const new_len = x.*.length + copy_len;
    const buf = if (new_len + 1 > x.*.area_size) blk: {
        const allocator = GcAllocator.allocator();
        break :blk allocator.realloc(x.*.ptr[0..x.*.area_size], new_len + 1) catch @panic("OOM");
    } else x.*.ptr[0..x.*.area_size];
    std.mem.copyForwards(u8, buf[x.*.length..new_len], y[0..copy_len]);
    x.* = .{
        .ptr = buf.ptr,
        .area_size = buf.len,
        .length = new_len,
    };
    buf[new_len] = 0;
}
test Strcat_charp_n {
    const x = Strnew_charp("abc");
    Strcat_charp_n(x, "def", 2);
    try std.testing.expectEqualSlices(u8, "abcde", std.mem.sliceTo(x.*.ptr, 0));
    try std.testing.expectEqual(5, x.*.length);
    try std.testing.expectEqual(c.INITIALStr_SIZE, x.*.area_size);
}

export fn Strcat(x: c.Str, y: c.Str) void {
    Strcat_charp_n(x, y.*.ptr, @intCast(y.*.length));
}
test Strcat {
    const x = Strnew_charp("abc");
    Strcat(x, Strnew_charp("de"));
    try std.testing.expectEqualSlices(u8, "abcde", std.mem.sliceTo(x.*.ptr, 0));
    try std.testing.expectEqual(5, x.*.length);
    try std.testing.expectEqual(c.INITIALStr_SIZE, x.*.area_size);
}

export fn Strcat_charp(x: c.Str, _y: [*c]const u8) void {
    if (_y) |y| {
        Strcat_charp_n(x, y, @intCast(std.mem.len(y)));
    }
}

test Strcat_charp {
    const x = Strnew_charp("abc");
    Strcat_charp(x, "de");
    try std.testing.expectEqualSlices(u8, "abcde", std.mem.sliceTo(x.*.ptr, 0));
    try std.testing.expectEqual(5, x.*.length);
    try std.testing.expectEqual(c.INITIALStr_SIZE, x.*.area_size);
}

export fn Strcat_m_charp(x: c.Str, ...) void {
    var ap = @cVaStart();
    defer @cVaEnd(&ap);

    while (@cVaArg(&ap, [*c]const u8)) |p| {
        Strcat_charp_n(x, p, @intCast(std.mem.len(p)));
    }
}
test Strcat_m_charp {
    const x = Strnew_charp("abc");
    // over 32 cause realloc
    Strcat_m_charp(x, "abcdefghij", "abcdefghij", "abcdefghij", @as([*c]const u8, @ptrFromInt(0)));
    try std.testing.expectEqualSlices(u8, "abcabcdefghijabcdefghijabcdefghij", std.mem.sliceTo(x.*.ptr, 0));
    try std.testing.expectEqual(33, x.*.length);
    try std.testing.expectEqual(33 + 1, x.*.area_size);
}

export fn Strnew_m_charp(p0: [*c]const u8, ...) c.Str {
    var ap = @cVaStart();
    defer @cVaEnd(&ap);

    const r = Strnew_charp(p0);
    while (@cVaArg(&ap, [*c]const u8)) |p| {
        c.Strcat_charp(r, p);
    }
    return r;
}
test Strnew_m_charp {
    const x = Strnew_m_charp("abc", "de", "fg");
    try std.testing.expectEqualSlices(u8, "abcdefg", std.mem.sliceTo(x.*.ptr, 0));
    try std.testing.expectEqual(7, x.*.length);
    try std.testing.expectEqual(c.INITIALStr_SIZE, x.*.area_size);
}
