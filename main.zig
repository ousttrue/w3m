const std = @import("std");
const w3m = @import("w3m");

extern fn w3m_main(argc: c_int, argv: [*c]const [:0]const u8) c_int;

pub fn call_dummy() void {
    std.log.debug("{}", .{w3m});
}

pub fn main(init: std.process.Init) !u8 {
    if (false) {}

    w3m.init(init);
    defer w3m.deinit();

    var argv = try init.minimal.args.toSlice(init.gpa);
    defer init.gpa.free(argv);

    const exit_code = w3m_main(@intCast(argv.len), &argv[0]);

    call_dummy();

    return @intCast(exit_code);
}
