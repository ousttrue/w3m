const std = @import("std");
const co = @import("co");
const runtime = @import("runtime.zig");
const defun = @import("defun.zig");
const c = @import("c.zig").c;

var S: *co.schedule = undefined;

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

var task_stack: std.Deque(W3mTask) = .initBuffer(&.{});

pub fn tasks_init() void {
    S = co.coroutine_open() orelse {
        @panic("coroutine_open");
    };
}

pub fn tasks_deinit() void {
    co.coroutine_close(S);
}

pub fn tasks_push(func: defun.CmdFunc, args: c.CmdArgs) void {
    task_stack.pushBack(runtime.allocator, .{
        .func = func,
        .args = args,
        .co_id = -1,
    }) catch @panic("OOM");

    if (task_stack.backPtr()) |task| {
        task.begin();
        if (task.state() == .DEAD) {
            _ = task_stack.popBack();
        }
    }
}

pub fn tasks_current() ?*W3mTask {
    return task_stack.backPtr();
}

pub fn tasks_pop() void {
    _ = task_stack.popBack();
}

pub fn block_in_task() void {
    co.coroutine_yield(S);
}
