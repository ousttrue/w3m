const std = @import("std");
const c = @cImport({
    @cInclude("Str.h");
    @cInclude("gc.h");
});
const GcAllocator = @import("GcAllocator.zig");

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

fn sizeFromLen(len: c_int) usize {
    if (len < c.INITIALStr_SIZE) {
        return c.INITIALStr_SIZE;
    }
    if (len >= c.STR_SIZE_MAX) {
        return c.STR_SIZE_MAX;
    }
    return @intCast(len);
}

fn makeStrBuf(size: usize) []u8 {
    std.debug.assert(size > 0);
    const p: [*c]u8 = @ptrCast(c.GC_MALLOC_ATOMIC(size));
    const ptr: [*]u8 = p orelse @panic("OOM");
    const buf = ptr[0..size];
    buf[0] = 0;
    return buf;
}

/// buf must 0 terminated
fn makeStr(buf: []u8) c.Str {
    const allocator = GcAllocator.allocator();
    const x = allocator.create(c._Str) catch @panic("OOM");
    x.* = .{
        .ptr = buf.ptr,
        .area_size = buf.len,
        .length = std.mem.indexOf(u8, buf, &.{0}) orelse @panic("must 0 terminated"),
    };
    return x;
}

export fn Strnew() c.Str {
    return makeStr(makeStrBuf(c.INITIALStr_SIZE));
}

export fn Strnew_size(len: c_int) c.Str {
    return makeStr(makeStrBuf(sizeFromLen(len)));
}
