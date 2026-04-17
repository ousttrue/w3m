const std = @import("std");
const c = @import("c.zig").c;
const co = @import("co");
const runtime = @import("runtime.zig");
const defun = @import("defun.zig");

pub const W3mTask = struct {
    func: defun.CmdFunc,
    args: c.CmdArgs,
    co_id: c_int,

    const State = enum(u8) {
        DEAD = 0,
        READY = 1,
        RUNNING = 2,
        SUSPEND = 3,
    };

    export fn coroutine(_S: ?*co.schedule, p: ?*anyopaque) void {
        _ = _S;
        var this: *@This() = @ptrCast(@alignCast(p));
        this.func.func(&this.args);
    }

    pub fn init(func: defun.CmdFunc, args: c.CmdArgs) @This() {
        return .{
            .func = func,
            .args = args,
            .co_id = -1,
        };
    }

    pub fn begin(this: *@This()) void {
        this.co_id = co.coroutine_new(S, &W3mTask.coroutine, this);
        co.coroutine_resume(S, this.co_id);
    }

    pub fn state(this: *@This()) State {
        return @enumFromInt(co.coroutine_status(S, this.co_id));
    }

    pub fn enqueue(this: *@This(), ch: u8) void {
        this.args.ch = ch;
        co.coroutine_resume(S, this.co_id);
    }

    pub fn block(this: *@This(), d: std.Io.Duration, args: *c.CmdArgs) c_int {
        std.debug.assert(&this.args == args);
        const start = std.Io.Clock.real.now(runtime.io);
        while (true) {
            co.coroutine_yield(S);
            if (args.ch > 0) {
                return args.ch;
            } else {
                const end = std.Io.Clock.real.now(runtime.io);
                const duration = std.Io.Timestamp.durationTo(start, end);
                if (duration.toMilliseconds() >= d.toMicroseconds()) {
                    // timeout
                    return 0;
                }
            }
        }
        unreachable;
    }
};

var S: *co.schedule = undefined;

pub fn init() void {
    S = co.coroutine_open() orelse {
        @panic("coroutine_open");
    };
}

pub fn deinit() void {
    co.coroutine_close(S);
}
