const std = @import("std");

pub fn atoi(T: type, _value: [*c]const u8) T {
    const value: []const u8 = std.mem.span(_value);
    return std.fmt.parseInt(T, value, 10) catch 0;
}

pub fn atof(T: type, _value: [*c]const u8) T {
    const value: []const u8 = std.mem.span(_value);
    return std.fmt.parseFloat(T, value) catch 0;
}
