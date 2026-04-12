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

    var argv: []const [:0]const u8 = try init.minimal.args.toSlice(init.gpa);
    defer init.gpa.free(argv);

    if (!c.w3m_args(.{}, @intCast(argv.len), @ptrCast(@constCast(&argv[0])))) {
        return 0;
    }

    const exit_code = c.w3m_loop();

    call_dummy();

    return @intCast(exit_code);
}
