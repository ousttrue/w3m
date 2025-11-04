const std = @import("std");
const c = @cImport({
    @cInclude("Str.h");
    @cInclude("gc.h");
    @cInclude("myctype.h");
});
const GcAllocator = @import("GcAllocator.zig");

fn StrLastChar(s: c.Str) ?u8 {
    if (s.*.length > 0) {
        return s.*.ptr[s.*.length - 1];
    } else {
        return null;
    }
}

const StrIterator = struct {
    str: c.Str,
    pos: usize = 0,

    fn init(s: c.Str) @This() {
        return .{
            .str = s,
        };
    }

    fn next(this: *@This()) ?*u8 {
        if (this.pos < this.str.*.length) {
            defer this.pos += 1;
            return &this.str.*.ptr[this.pos];
        } else {
            return null;
        }
    }
};

const StrReverseIterator = struct {
    str: c.Str,
    pos: i32,

    fn init(s: c.Str) @This() {
        return .{
            .str = s,
            .pos = @as(i32, @intCast(s.*.length)) - 1,
        };
    }

    fn next(this: *@This()) ?*u8 {
        if (this.pos >= 0) {
            defer this.pos -= 1;
            return &this.str.*.ptr[@intCast(this.pos)];
        } else {
            return null;
        }
    }
};

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
    const size = @max(@min(_size, c.STR_SIZE_MAX), c.INITIALStr_SIZE);
    const p: [*c]u8 = @ptrCast(c.GC_MALLOC_ATOMIC(size));
    const ptr: [*]u8 = p orelse @panic("OOM");
    const buf = ptr[0..size];
    buf[0] = 0;
    return buf;
}

fn reallocStrBuf(x: c.Str, _size: usize) []u8 {
    if (x.*.area_size >= _size) {
        return x.*.ptr[0..x.*.area_size];
    }
    std.debug.assert(_size > 0);
    const size = @max(_size, c.INITIALStr_SIZE);
    const p: [*c]u8 = @ptrCast(c.GC_REALLOC(x.*.ptr, size));
    const ptr: [*]u8 = p orelse @panic("OOM");
    const buf = ptr[0..size];
    buf[0] = 0;
    return buf;
}

fn allocStrBufFrom_charp_n(p: [*c]const u8, len: c_int) []u8 {
    const p_len = std.mem.len(p);
    if (len < 0) {
        const buf = allocStrBuf(p_len + 1);
        std.mem.copyForwards(u8, buf, p[0..p_len]);
        buf[p_len] = 0;
        return buf;
    } else {
        const size: usize = @intCast(len);
        const buf = allocStrBuf(size + 1);
        const ptr: [*]const u8 = @ptrCast(p);
        const copy_len = @min(p_len, size);
        std.mem.copyForwards(u8, buf[0..copy_len], ptr[0..copy_len]);
        buf[copy_len] = 0;
        return buf;
    }
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
    return createStr(allocStrBuf(if (len <= 0) 0 else @intCast(len)));
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

export fn Strnew_charp_n(_p: [*c]const u8, len: c_int) c.Str {
    const p = _p orelse return Strnew();
    const buf = allocStrBufFrom_charp_n(p, len);
    return createStr(buf);
}
test Strnew_charp_n {
    {
        const x = Strnew_charp_n("abc", 3);
        try std.testing.expectEqualSlices(u8, "abc", std.mem.sliceTo(x.*.ptr, 0));
        try std.testing.expectEqual(3, x.*.length);
        try std.testing.expectEqual(c.INITIALStr_SIZE, x.*.area_size);
    }
    {
        const x = Strnew_charp_n("abc", 2);
        try std.testing.expectEqualSlices(u8, "ab", std.mem.sliceTo(x.*.ptr, 0));
        try std.testing.expectEqual(2, x.*.length);
        try std.testing.expectEqual(c.INITIALStr_SIZE, x.*.area_size);
    }
    {
        // INT_MAX / 32
        const x = Strnew_charp_n("abc", @divTrunc(std.math.maxInt(c_int), 32) + 256);
        try std.testing.expectEqualSlices(u8, "abc", std.mem.sliceTo(x.*.ptr, 0));
        try std.testing.expectEqual(3, x.*.length);
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
    while (x.*.length + copy_len > x.*.area_size) {
        Strgrow(x);
    }
    std.mem.copyForwards(u8, x.*.ptr[x.*.length..new_len], y[0..copy_len]);
    x.*.length = new_len;
    x.*.ptr[new_len] = 0;
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

export fn Strcat_char(x: c.Str, y: u8) void {
    Strcat_charp_n(x, &y, 1);
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

export fn Strcopy(dst: c.Str, src: c.Str) void {
    const copy_size = src.*.length;
    const buf = reallocStrBuf(dst, copy_size + 1);
    std.mem.copyForwards(u8, buf[0..copy_size], src.*.ptr[0..copy_size]);
    buf[copy_size] = 0;
    dst.* = .{
        .ptr = buf.ptr,
        .area_size = buf.len,
        .length = copy_size,
    };
}

export fn Strdup(s: c.Str) c.Str {
    const n = Strnew_size(@intCast(s.*.length));
    Strcopy(n, s);
    return n;
}

export fn Strcopy_charp(dst: c.Str, _src: [*c]const u8) void {
    const src = _src orelse {
        dst.*.length = 0;
        dst.*.ptr[0] = 0;
        return;
    };

    const copy_size = std.mem.len(src);
    const buf = reallocStrBuf(dst, copy_size + 1);
    std.mem.copyForwards(u8, buf[0..copy_size], src[0..copy_size]);
    buf[copy_size] = 0;
    dst.* = .{
        .ptr = buf.ptr,
        .area_size = buf.len,
        .length = copy_size,
    };
}

export fn Strcopy_charp_n(dst: c.Str, _src: [*c]const u8, n: c_int) void {
    const src = _src orelse {
        dst.*.length = 0;
        dst.*.ptr[0] = 0;
        return;
    };

    const copy_size: usize = if (n < 0)
        std.mem.len(src)
    else
        @min(@as(usize, @intCast(n)), std.mem.len(src));
    const buf = reallocStrBuf(dst, copy_size + 1);
    std.mem.copyForwards(u8, buf[0..copy_size], src[0..copy_size]);
    buf[copy_size] = 0;
    dst.* = .{
        .ptr = buf.ptr,
        .area_size = buf.len,
        .length = copy_size,
    };
}

export fn Strclear(s: c.Str) void {
    s.*.length = 0;
    s.*.ptr[0] = 0;
}

export fn Strfree(x: c.Str) void {
    c.GC_free(x.*.ptr);
    c.GC_free(x);
}

export fn Strgrow(x: c.Str) void {
    const addlen: usize = if (x.*.area_size < 8192)
        x.*.area_size
    else
        @divTrunc(x.*.area_size, 2);
    const newlen = x.*.area_size + addlen;
    if (x.*.area_size < newlen) {
        const buf = allocStrBuf(newlen);
        std.mem.copyForwards(u8, buf, x.*.ptr[0..x.*.length]);
        x.*.ptr = buf.ptr;
        x.*.area_size = newlen;
    }
    x.*.ptr[x.*.length] = 0;
}

export fn Strsubstr(s: c.Str, begin: usize, len: usize) c.Str {
    const new_s = Strnew();
    for (0..len) |i| {
        if (begin + i >= s.*.length) {
            break;
        }
        Strcat_char(new_s, s.*.ptr[begin + i]);
    }
    return new_s;
}
test Strsubstr {
    {
        const x0 = Strnew_charp("abc");
        const x1 = Strsubstr(x0, 1, 3);
        try std.testing.expectEqualSlices(u8, "bc", std.mem.sliceTo(x1.*.ptr, 0));
    }
    {
        const x0 = Strnew_charp("abc");
        const x1 = Strsubstr(x0, 5, 3);
        try std.testing.expectEqualSlices(u8, "", std.mem.sliceTo(x1.*.ptr, 0));
    }
}

fn Strslice(s: c.Str) []u8 {
    return s.*.ptr[0..s.*.length];
}

export fn Strlower(s: c.Str) void {
    for (Strslice(s)) |*ch| {
        ch.* = c.TOLOWER(ch.*);
    }
}

export fn Strupper(s: c.Str) void {
    for (Strslice(s)) |*ch| {
        ch.* = c.TOUPPER(ch.*);
    }
}

export fn Strchop(s: c.Str) void {
    while (StrLastChar(s)) |ch| {
        if (ch == '\n' or ch == '\r') {
            s.*.length -= 1;
        } else {
            break;
        }
    }
    s.*.ptr[s.*.length] = 0;
}

export fn Strinsert_char(s: c.Str, pos: usize, ch: u8) void {
    if (pos < 0 or s.*.length < pos)
        return;
    if (s.*.length + 2 > s.*.area_size)
        Strgrow(s);
    if (s.*.length < pos)
        return;

    @memmove(s.*.ptr[pos + 1 .. s.*.length + 1], s.*.ptr[pos..s.*.length]);
    s.*.length += 1;
    s.*.ptr[s.*.length] = 0;
    s.*.ptr[pos] = ch;
}
test Strinsert_char {
    const x = Strnew_charp("abc");
    Strinsert_char(x, 2, 'x');
    try std.testing.expectEqualSlices(u8, "abxc", Strslice(x));
}

export fn Strinsert_charp(s: c.Str, pos: usize, p: [*c]const u8) void {
    for (pos.., std.mem.span(p)) |i, ch| {
        Strinsert_char(s, i, ch);
    }
}
test Strinsert_charp {
    const x = Strnew_charp("abc");
    Strinsert_charp(x, 2, "xyz");
    try std.testing.expectEqualSlices(u8, "abxyzc", Strslice(x));
}

fn subBeginToEnd(p: *u8, begin: usize, end: usize) []u8 {
    return p[begin..end];
}

export fn Strdelete(s: c.Str, pos: usize, n: c_int) void {
    if (pos < 0 or s.*.length < pos)
        return;

    const size: usize = if (n < 0)
        s.*.length - pos
    else
        @intCast(n);

    const new_length = if (s.*.length <= pos + size) pos else blk: {
        const src = s.*.ptr[pos + size .. s.*.length];
        const dst = s.*.ptr[pos .. pos + src.len];
        @memmove(dst, src);
        break :blk pos + src.len;
    };
    s.*.ptr[new_length] = 0;
    s.*.length = new_length;
}
test Strdelete {
    {
        const x = Strnew_charp("abcdefg");
        Strdelete(x, 0, 3);
        try std.testing.expectEqualSlices(u8, "defg", Strslice(x));
    }
    {
        const x = Strnew_charp("abcdefg");
        Strdelete(x, 3, -1);
        try std.testing.expectEqualSlices(u8, "abc", Strslice(x));
    }
    {
        const x = Strnew_charp("abcdefg");
        Strdelete(x, 3, 4);
        try std.testing.expectEqualSlices(u8, "abc", Strslice(x));
    }
    {
        const x = Strnew_charp("abcdefg");
        Strdelete(x, 3, 2);
        try std.testing.expectEqualSlices(u8, "abcfg", Strslice(x));
    }
}

export fn Strtruncate(s: c.Str, n: c_int) void {
    if (n < 0 or s.*.length < n)
        return;
    const i: usize = @intCast(n);
    s.*.ptr[i] = 0;
    s.*.length = i;
}

export fn Strshrink(s: c.Str, n: c_int) void {
    if (n >= s.*.length) {
        s.*.length = 0;
        s.*.ptr[0] = 0;
    } else if (n > 0) {
        const i: usize = @intCast(n);
        s.*.length -= i;
        s.*.ptr[s.*.length] = 0;
    }
}

export fn Strremovefirstspaces(s: c.Str) void {
    var it = StrIterator.init(s);
    var delete_len: usize = 0;
    while (it.next()) |ch| {
        if (!c.IS_SPACE(ch.*)) {
            break;
        }
        delete_len += 1;
    }
    if (delete_len > 0) {
        Strdelete(s, 0, @intCast(delete_len));
    }
}
test Strremovefirstspaces {
    {
        const x = Strnew_charp("abc");
        Strremovefirstspaces(x);
        try std.testing.expectEqualSlices(u8, "abc", Strslice(x));
    }
    {
        const x = Strnew_charp("\r\n abc \r\n");
        Strremovefirstspaces(x);
        try std.testing.expectEqualSlices(u8, "abc \r\n", Strslice(x));
    }
}

export fn Strremovetrailingspaces(s: c.Str) void {
    var it = StrReverseIterator.init(s);
    var new_length = s.*.length;
    while (it.next()) |ch| {
        if (!c.IS_SPACE(ch.*)) {
            break;
        }
        new_length -= 1;
    }
    s.*.length = new_length;
    s.*.ptr[new_length] = 0;
}
test Strremovetrailingspaces {
    {
        const x = Strnew_charp("abc");
        Strremovetrailingspaces(x);
        try std.testing.expectEqualSlices(u8, "abc", Strslice(x));
    }
    {
        const x = Strnew_charp("\r\n abc \r\n");
        Strremovetrailingspaces(x);
        try std.testing.expectEqualSlices(u8, "\r\n abc", Strslice(x));
    }
}

/// for only bytelength equals column width
export fn Stralign_left(s: c.Str, width: usize) c.Str {
    if (s.*.length >= width)
        return Strdup(s);
    const n = Strnew_size(@intCast(width));
    Strcopy(n, s);
    for (s.*.length..width) |_| {
        Strcat_char(n, ' ');
    }
    return n;
}

/// for only bytelength equals column width
export fn Stralign_right(s: c.Str, width: usize) c.Str {
    if (s.*.length >= width)
        return Strdup(s);
    const n = Strnew_size(@intCast(width));
    for (s.*.length..width) |_| {
        Strcat_char(n, ' ');
    }
    Strcat(n, s);
    return n;
}

/// for only bytelength equals column width
export fn Stralign_center(s: c.Str, width: usize) c.Str {
    if (s.*.length >= width)
        return Strdup(s);
    const n = Strnew_size(@intCast(width));
    const w = @divTrunc(width - s.*.length, 2);
    for (0..w) |_| {
        Strcat_char(n, ' ');
    }
    Strcat(n, s);
    for (w + s.*.length..width) |_| {
        Strcat_char(n, ' ');
    }
    return n;
}

export fn Sprintf(fmt: [*c]const u8, ...) c.Str {
    const len = blk: {
        var ap = @cVaStart();
        defer @cVaEnd(&ap);
        break :blk c.vscpf(fmt, @ptrCast(&ap));
    };

    {
        const s = Strnew_size(len * 2);
        var ap = @cVaStart();
        defer @cVaEnd(&ap);
        _ = c.vsprintf(s.*.ptr, fmt, @ptrCast(&ap));
        s.*.length = std.mem.len(s.*.ptr);
        if (s.*.length > len * 2) {
            @panic("Sprintf: string too long");
        }
        return s;
    }
}
test Sprintf {
    const x = Sprintf("%d => hello %s", @as(c_int, 10), "world");
    try std.testing.expectEqualSlices(u8, "10 => hello world", Strslice(x));
}

export fn Strfgets(f: *c.FILE) c.Str {
    const s = Strnew();
    while (true) {
        const ch = c.fgetc(f);
        if (ch == c.EOF) {
            break;
        }
        Strcat_char(s, @intCast(ch));
        if (ch == '\n') {
            break;
        }
    }
    return s;
}

export fn Strfgetall(f: *c.FILE) c.Str {
    const s = Strnew();
    while (true) {
        const ch = c.fgetc(f);
        if (ch != c.EOF) {
            break;
        }
        Strcat_char(s, @intCast(ch));
    }
    return s;
}

export fn Strcmp(x: c.Str, y: c.Str) c_int {
    return switch (std.mem.order(u8, std.mem.span(x.*.ptr), std.mem.span(y.*.ptr))) {
        .gt => 1,
        .eq => 0,
        .lt => -1,
    };
}
test Strcmp {
    const c_import = @cImport({
        @cInclude("string.h");
    });
    const Tmp = struct {
        fn zigStrcmp(lhs: []const u8, rhs: []const u8) c_int {
            return switch (std.mem.order(u8, lhs, rhs)) {
                .gt => 1,
                .eq => 0,
                .lt => -1,
            };
        }
    };
    {
        const a = "abc";
        const b = "bcd";
        try std.testing.expectEqual(c_import.strcmp(a, b), Tmp.zigStrcmp(a, b));
    }
    {
        const b = "abc";
        const a = "bcd";
        try std.testing.expectEqual(c_import.strcmp(a, b), Tmp.zigStrcmp(a, b));
    }
    {
        const a = "abc";
        const b = "abc";
        try std.testing.expectEqual(c_import.strcmp(a, b), Tmp.zigStrcmp(a, b));
    }
}
export fn Strcmp_charp(x: c.Str, y: [*c]const u8) c_int {
    return switch (std.mem.order(u8, std.mem.span(x.*.ptr), std.mem.span(y))) {
        .gt => 1,
        .eq => 0,
        .lt => -1,
    };
}
// int Strncmp(Str x, Str y, size_t n) { return strncmp(x.ptr, y.ptr, n); }
// int Strncmp_charp(Str x, const char* y, size_t n) { return strncmp(x.ptr, y, n); }
export fn Strcasecmp(x: c.Str, y: c.Str) c_int {
    return switch (std.ascii.orderIgnoreCase(std.mem.span(x.*.ptr), std.mem.span(y.*.ptr))) {
        .gt => 1,
        .eq => 0,
        .lt => -1,
    };
}
export fn Strcasecmp_charp(x: c.Str, y: [*c]const u8) c_int {
    return switch (std.ascii.orderIgnoreCase(std.mem.span(x.*.ptr), std.mem.span(y))) {
        .gt => 1,
        .eq => 0,
        .lt => -1,
    };
}
// int Strncasecmp(Str x, Str y, size_t n) { return strncasecmp(x.ptr, y.ptr, n); }
// int Strncasecmp_charp(Str x, const char* y, size_t n) { return strncasecmp(x.ptr, y, n); }

export fn Strlastchar(s: c.Str) u8 {
    return if (s.*.length > 0) s.*.ptr[s.*.length - 1] else 0;
}
export fn Strfputs(s: c.Str, f: *c.FILE) c_int {
    return @intCast(c.fwrite(s.*.ptr, 1, s.*.length, f));
}
