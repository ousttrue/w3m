const std = @import("std");
const c = @import("c.zig").c;
const g = @import("global.zig");
pub const runtime = @import("runtime.zig");
pub const global = @import("global.zig");
const tty = @import("tty.zig");
const content_type = @import("content_type.zig");
const guessContentType = content_type.guessContentType;
const terminfo_entry = @import("terminfo_entry.zig");
const input_dispatcher = @import("input_dispatcher.zig");
const image = @import("image.zig");

pub export fn _dummy_() void {
    // export symbols ?
    std.log.debug("{}", .{global});
    std.log.debug("{}", .{terminfo_entry});
    std.log.debug("{}", .{content_type});
    std.log.debug("{}", .{image});
}

comptime {
    std.testing.refAllDecls(@This());
}

pub fn init(process_init: std.process.Init) void {
    runtime.init(process_init);
    tty.init(process_init.io);
}

pub fn deinit() void {
    input_dispatcher.deinit();
    tty.deinit();
    runtime.deinit();
}

extern fn checkDownloadList() bool;
extern fn submitCurrentBuffer(args: *c.CmdArgs) bool;
extern fn processCurrentEvent() bool;
extern fn processCurrentBufferEvent() bool;
extern fn processResizeAndImage(args: *c.CmdArgs) void;

export fn w3m_loop() c_int {
    input_dispatcher.init();

    while (g.is_running) {
        var args: c.CmdArgs = .{};

        if (checkDownloadList()) {
            c.ldDL(null);
        }
        if (processCurrentEvent()) {
            continue;
        }
        if (processCurrentBufferEvent()) {
            continue;
        }
        if (submitCurrentBuffer(&args)) {
            continue;
        }

        // TODO:
        // processResizeAndImage(&args);

        const has_input = tty.epoll.next(80) catch {
            // error ?
            break;
        };

        if (has_input) |_| {
            var buf: [1]u8 = undefined;
            const readsize = std.posix.read(
                tty.tty_in.handle,
                &buf,
            ) catch @panic("getch");
            std.debug.assert(readsize == 1);

            const ch: u8 = buf[0];
            input_dispatcher.dispatch_key(ch);
        } else {
            input_dispatcher.dispatch_timeout();
        }
    }

    return 0;
}
