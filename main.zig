const std = @import("std");
const w3m = @import("w3m");
const c = @cImport({
    @cInclude("w3m/w3m.h");
});

pub fn call_dummy() void {
    std.log.debug("{}", .{w3m});
}

pub fn main(init: std.process.Init) !u8 {
    if (false) {}

    w3m.init(init);
    defer w3m.deinit();

    const argv = init.minimal.args.vector;
    defer init.gpa.free(argv);

    if (!c.w3m_args(null, @intCast(argv.len), @ptrCast(@constCast(argv.ptr)))) {
        return 0;
    }

    const exit_code = c.w3m_loop();

    call_dummy();

    return @intCast(exit_code);
}
