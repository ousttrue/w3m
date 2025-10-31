const std = @import("std");
extern fn w3m_main(argc: c_int, argv: [*c][*c]c_char) c_int;

pub fn main() u8 {
    const code = w3m_main(@intCast(std.os.argv.len), @ptrCast(&std.os.argv[0]));
    return @intCast(code);
}
