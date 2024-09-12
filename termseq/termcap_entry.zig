const std = @import("std");
const TermCapabilities = @import("TermCapabilities.zig");

pub fn main() void {
    var caps = TermCapabilities{};
    if (!caps.load("xterm")) {
        @panic("load");
    }

    inline for (std.meta.fields(TermCapabilities)) |f| {
        if (f.type == TermCapabilities.StringCapability or f.type == TermCapabilities.IntCapability) {
            std.debug.print("{any}\n", .{@field(caps, f.name)});
        }
    }
}
