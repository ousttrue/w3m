const std = @import("std");
const TermCapabilities = @import("TermCapabilities.zig");

pub fn main() void {
    var caps = TermCapabilities{};
    if (!caps.load("xterm")) {
        @panic("load");
    }

    inline for (std.meta.fields(TermCapabilities)) |f| {
        if (@typeInfo(f.type) == .Struct) {
            std.debug.print("{any}\n", .{@field(caps, f.name)});
        } else {
            std.debug.print("{s}\n", .{@typeName(f.type)});
        }
    }
}
