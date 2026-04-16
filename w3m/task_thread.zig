const std = @import("std");
const defun = @import("defun.zig");
const c = @import("c.zig").c;

pub const W3mTask = struct {
    func: defun.CmdFunc,
    args: c.CmdArgs,

    const State = enum(u8) {
        DEAD = 0,
        READY = 1,
        RUNNING = 2,
        SUSPEND = 3,
    };

    pub fn begin(this: *@This()) void {
        _ = this;
    }

    pub fn state(this: *@This()) State {
        _ = this;
        return .DEAD;
    }

    pub fn enqueue(this: *@This(), ch: u8) void {
        this.args.ch = ch;
    }

    pub fn block(this: *@This(), d: std.Io.Duration, args: *c.CmdArgs) c_int {
        _ = this;
        _ = d;
        _ = args;
        //     std.debug.assert(&this.args == args);
        //     const start = std.Io.Clock.real.now(runtime.io);
        //     while (true) {
        //         co.coroutine_yield(S);
        //         if (args.ch > 0) {
        //             return args.ch;
        //         } else {
        //             const end = std.Io.Clock.real.now(runtime.io);
        //             const duration = std.Io.Timestamp.durationTo(start, end);
        //             if (duration.toMilliseconds() >= d.toMicroseconds()) {
        //                 // timeout
        //                 return 0;
        //             }
        //         }
        //     }
        unreachable;
    }
};

pub fn tasks_init() void {}
pub fn tasks_deinit() void {}

pub fn tasks_current() ?*W3mTask {
    return null;
}

pub fn tasks_pop() void {}

pub fn tasks_push(func: defun.CmdFunc, args: c.CmdArgs) void {
    _ = func;
    _ = args;
}

pub fn block_in_task() void {}
