const std = @import("std");

/// return ture if enter main loop
extern fn w3m_args(argc: c_int, argv: [*c]const [*:0]u8) bool;

extern fn w3m_loop() void;

pub fn main() void {
    if (w3m_args(@intCast(std.os.argv.len), &std.os.argv[0])) {
        w3m_loop();
    }
}
