const std = @import("std");
const g = @import("global.zig");
pub var io: std.Io = undefined;
pub var allocator: std.mem.Allocator = undefined;
pub var environ_map: *std.process.Environ.Map = undefined;

const CONF_DIR = "/usr/etc/w3m";

pub fn init(process_init: std.process.Init) void {
    io = process_init.io;
    allocator = process_init.gpa;
    environ_map = process_init.environ_map;
}

pub fn deinit() void {}

pub fn expandPath(str: []const u8, buf: []u8) []const u8 {
    if (str.len == 0) {
        return &.{};
    }
    if (str[0] != '~') {
        return std.fmt.bufPrintZ(buf, "{s}", .{str}) catch {
            @panic("bufPrintZ");
        };
    }

    if (str.len == 1) {
        const home = environ_map.get("HOME") orelse {
            @panic("NO_HOME");
        };
        return home;
    } else if (str[1] == '/') { // ~/dir... or ~
        const home = environ_map.get("HOME") orelse {
            @panic("NO_HOME");
        };
        return std.fmt.bufPrintZ(buf, "{s}/{s}", .{ home, str[2..] }) catch {
            @panic("bufPrintZ");
        };
    } else {
        return std.fmt.bufPrintZ(buf, "{s}", .{str}) catch {
            @panic("bufPrintZ");
        };
    }
}

test "expandPath" {
    const gpa = std.testing.allocator;
    var map: std.process.Environ.Map = .init(gpa);
    environ_map = &map;
    defer map.deinit();

    const user = "TEST_USER";
    try map.put("USER", user);
    try map.put("HOME", "/home/" ++ user);

    {
        var buf: [128]u8 = undefined;
        const path = expandPath("~", &buf);
        try std.testing.expectEqualSlices(u8, "/home/" ++ user, path);
    }
    {
        var buf: [128]u8 = undefined;
        const path = expandPath("~/hoge", &buf);
        try std.testing.expectEqualSlices(u8, "/home/" ++ user ++ "/hoge", path);
    }
}

pub fn w3m_dir(name: []const u8, dft: []const u8) []const u8 {
    if (environ_map.get(name)) |env_value| {
        return allocator.dupe(u8, env_value) catch @panic("OOM");
    } else {
        return allocator.dupe(u8, dft) catch @panic("OOM");
    }
}

pub fn w3m_conf_dir() []const u8 {
    return w3m_dir("W3M_CONF_DIR", CONF_DIR);
}

pub fn confFile(base: []const u8, buf: []u8) []const u8 {
    var tmp: [512]u8 = undefined;
    const tmp2 = std.fmt.bufPrintZ(&tmp, "{s}/{s}", .{ w3m_conf_dir(), base }) catch {
        @panic("confFile");
    };
    return expandPath(tmp2, buf);
}

pub fn rcFile(base: []const u8, buf: []u8) []const u8 {
    if (base.len > 0 and
        (base[0] == '/' or
            (base[0] == '.' and (base[1] == '/' or (base[1] == '.' and base[2] == '/'))) or
            (base[0] == '~' and base[1] == '/')))
    {
        // /file, ./file, ../file, ~/file
        return expandPath(base, buf);
    }

    var tmp: [512]u8 = undefined;
    const tmp2 = std.fmt.bufPrintZ(&tmp, "{s}/{s}", .{ g.rc_dir, base }) catch {
        @panic("confFile");
    };
    return expandPath(tmp2, buf);
}
