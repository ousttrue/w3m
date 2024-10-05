const std = @import("std");

extern fn main2(argc: c_int, argv: [*c][*c]c_char) c_int;

pub fn main() void {
    _ = main2(@intCast(std.os.argv.len), @ptrCast(&std.os.argv[0]));
}
