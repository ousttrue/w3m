const std = @import("std");

extern fn w3m_main(argc: c_int, argv: [*c]const [:0]const u8) c_int;

pub fn main(init: std.process.Init) !u8 {
    var argv = try init.minimal.args.toSlice(init.gpa);
    defer init.gpa.free(argv);

    const exit_code = w3m_main(@intCast(argv.len), &argv[0]);
    return @intCast(exit_code);
}
