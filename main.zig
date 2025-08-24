const std = @import("std");
const c = @cImport({
    @cInclude("w3m.h");
    @cInclude("parseArgs.h");
});

pub fn main() void {
    const line_str = c.parseArgs(
        @intCast(std.os.argv.len),
        @ptrCast(std.os.argv),
    );
    _ = c.main_loop(line_str);
}
