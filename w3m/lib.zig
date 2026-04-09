const std = @import("std");
const c = @cImport({
    @cInclude("defun_impl.h");
    @cInclude("constants.h");
    @cInclude("w3m.h");
});

pub const runtime = @import("runtime.zig");
pub const global = @import("global.zig");
// pub const keybind = @import("keybind.zig");
pub const tty = @import("tty.zig");
const g = @import("global.zig");

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

extern fn checkDownloadList() bool;
extern fn submitCurrentBuffer() bool;
extern fn processCurrentEvent() bool;
extern fn processCurrentBufferEvent() bool;
extern fn processResizeAndImage() void;
extern var GlobalKeymap: [128][*c]const u8;
extern fn setupCurrentBuffer() void;

export fn w3m_loop() c_int {
    while (true) {
        if (checkDownloadList()) {
            c.ldDL(.{});
        }
        if (submitCurrentBuffer()) {
            continue;
        }
        if (processCurrentEvent()) {
            continue;
        }
        if (processCurrentBufferEvent()) {
            continue;
        }

        processResizeAndImage();

        const ch = tty.getch();
        //         last_key = c;
        //         if (CurrentAlarm->sec > 0) {
        //             alarm(0);
        //         }
        if (std.ascii.isAscii(ch)) {
            if (('0' <= ch) and (ch <= '9') and (g.prec_num != 0 or std.mem.eql(u8, std.mem.span(GlobalKeymap[ch]), "NOTHING"))) {
                g.prec_num = g.prec_num * 10 + (ch - '0');
                if (g.prec_num > c.PREC_LIMIT)
                    g.prec_num = c.PREC_LIMIT;
            } else {
                setupCurrentBuffer();
                g.CurrentKey = ch;
                c.w3mFunc(GlobalKeymap[ch]);

                g.prec_num = 0;
            }
        }
        // g.prev_key = g.CurrentKey;
        g.CurrentKey = -1;
        g.CurrentKeyData = null;
    }

    return 0;
}
