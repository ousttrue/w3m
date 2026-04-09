const std = @import("std");
pub const runtime = @import("runtime.zig");
pub const global = @import("global.zig");
// pub const keybind = @import("keybind.zig");
pub const tty = @import("tty.zig");

pub export fn _dummy_() void {
    // export symbols ?
    std.log.debug("{}", .{global});
    std.log.debug("{}", .{tty});
}

comptime {
    std.testing.refAllDecls(@This());
}

pub fn init(process_init: std.process.Init) void {
    runtime.init(process_init);
    // keybind.init();
    tty.init();
}

pub fn deinit() void {
    // keybind.deinit();
    tty.deinit();
    runtime.deinit();
}
