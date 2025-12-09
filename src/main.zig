const std = @import("std");
const zlua = @import("zlua");
const Lua = zlua.Lua;
const config = @import("config.zig");
const c = @import("w3m.zig").c;
const util = @import("util.zig");
const url = @import("url.zig");
const local_cgi = @import("local_cgi.zig");
const input_stream = @import("input_stream.zig");

extern fn w3m_parse_arg(argc: c_int, argv: [*c][*c]c_char) c_int;
extern fn w3m_loop() c_int;

pub fn main() !u8 {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    const allocator = gpa.allocator();
    defer _ = gpa.deinit();

    var lua = try Lua.init(allocator);
    defer lua.deinit();

    lua.pushInteger(42);
    std.debug.print("{}\n", .{try lua.toInteger(1)});

    if (w3m_parse_arg(@intCast(std.os.argv.len), @ptrCast(&std.os.argv[0])) == 0) {
        return 1;
    }
    const code = w3m_loop();
    return @as(u8, @intCast(code));
}

export fn config_load(handle: std.fs.File.Handle) void {
    const file = std.fs.File{
        .handle = handle,
    };
    var linebuf: [512]u8 = undefined;
    var reader = file.reader(&linebuf);
    config.read_config(&reader.interface) catch @panic("config_load");
}

export fn parseURL(src: [*c]const u8, p_url: *c.Url, current: [*c]c.Url) void {
    url.parseURL(src, p_url, current);
}

export fn parseURL2(src: [*c]const u8, pu: *c.Url, current: [*c]c.Url) void {
    url.parseURL2(src, pu, current);
}

test "Url" {
    {
        const src = "/home/USER/.w3m/bookmark.html";
        var pu: c.Url = undefined;
        parseURL(src, &pu, null);
        try std.testing.expectEqual(pu.scheme, c.SCM_LOCAL);
    }
    {
        const src = "https://search.yahoo.co.jp/search";
        var pu: c.Url = undefined;
        parseURL(src, &pu, null);
        try std.testing.expectEqual(pu.scheme, c.SCM_HTTPS);
        try std.testing.expectEqualSlices(u8, "search.yahoo.co.jp", std.mem.span(pu.host));
    }
}

export fn localcgi_post(
    uri: [*c]const u8,
    qstr: [*c]const u8,
    request: [*c]c.FormList,
    referer: [*c]const u8,
) ?*c.FILE {
    return local_cgi.post(uri, qstr, request, referer);
}

export fn localCookie() c.Str {
    return local_cgi.localCookie();
}

export fn set_environ(name: [*c]const u8, value: [*c]const u8) void {
    if (name != null and value != null) {
        _ = c.setenv(name, value, 1);
    }
}

export fn init_buffer(base: *c.base_stream, buf: ?[*]u8, bufsize: usize) void {
    input_stream.init_buffer(base, buf, bufsize);
}
