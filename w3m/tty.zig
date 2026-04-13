const std = @import("std");
const c = @import("c.zig").c;
const g = @import("global.zig");
const runtime = @import("runtime.zig");
const Tty = @import("TtyLinux.zig");

var tty: ?*Tty = null;

pub fn init() void {
    tty = .create(
        runtime.allocator,
        runtime.evented.io(),
        std.Io.File.stdin(),
        std.Io.File.stdout(),
    );
}


