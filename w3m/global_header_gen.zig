const std = @import("std");
const global = @import("global.zig");

pub fn main(init: std.process.Init) !void {
    var output_file = try std.Io.Dir.cwd().createFile(init.io, "global.h", .{});
    defer output_file.close(init.io);
    var path: [128]u8 = undefined;
    const path_size = try output_file.realPath(init.io, &path);
    std.log.debug("write to {s}", .{path[0..path_size]});

    var write_buf: [128]u8 = undefined;
    var w = output_file.writer(init.io, &write_buf);
    defer w.flush() catch @panic("OOM");

    std.log.debug("begin", .{});
    try w.interface.writeAll("#pragma once\n");
    try w.interface.writeAll("#include <stdint.h>\n");
    try w.interface.writeAll("\n");
    inline for (@typeInfo(global).@"struct".decls) |d| {
        try write_field(&w.interface, d.name);
    }
    std.log.debug("end", .{});
}

fn write_field(writer: *std.Io.Writer, comptime name: []const u8) !void {
    const T = @TypeOf(@field(global, name));
    if (T == c_int) {
        try writer.print("extern int {s};\n", .{name});
    } else if (T == u32) {
        try writer.print("extern uint32_t {s};\n", .{name});
    } else if (T == u8) {
        try writer.print("extern char {s};\n", .{name});
    } else if (T == f64) {
        try writer.print("extern double {s};\n", .{name});
    // } else if (T == ?[*:0]const u8) {
    } else if (T == [*c]const u8) {
        try writer.print("extern char* {s};\n", .{name});
    } else {
        @panic(@typeInfo(T).name);
    }
}
