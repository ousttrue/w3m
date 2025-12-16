const std = @import("std");

extern fn w3m_main(argc: c_int, argv: [*c]const [*:0]u8) c_int;

pub fn main() u8 {
    const exit_code = w3m_main(@intCast(std.os.argv.len), &std.os.argv[0]);
    return @intCast(exit_code);
}
