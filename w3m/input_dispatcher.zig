const std = @import("std");
const c = @import("c.zig").c;
const runtime = @import("runtime.zig");
const g = @import("global.zig");
const defun = @import("defun.zig");
const config = @import("config");
const w3m_task = switch (config.task_backend) {
    .coroutine => @import("task_coroutine.zig"),
    .thread => @import("task_thread.zig"),
};
pub const W3mTask = w3m_task.W3mTask;

var task_stack: std.Deque(w3m_task.W3mTask) = .initBuffer(&.{});

var root: W3mTask = undefined;

pub fn init(func: defun.CmdFunc, args: c.CmdArgs) void {
    w3m_task.init();
    root = .init(func, args);
    root.begin();
}

pub fn deinit() void {
    w3m_task.deinit();
}

pub fn is_running() bool {
    return root.state() != .DEAD;
}

pub fn tasks_push(func: defun.CmdFunc, args: c.CmdArgs) void {
    task_stack.pushBack(runtime.allocator, .init(func, args)) catch @panic("OOM");

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

extern var GlobalKeymap: [128][*c]const u8;
extern fn setupCurrentBuffer() void;

export fn w3mFunc(cmd: [*c]const u8) void {
    const func = defun.getFunc(std.mem.span(cmd)) orelse {
        std.log.warn("{s} not found", .{cmd});
        return;
    };

    tasks_push(func, .{});
}

pub fn dispatch_key(ch: u8) void {
    if (tasks_current()) |task| {
        task.enqueue(ch);
        if (task.state() == .DEAD) {
            tasks_pop();
        }
    } else {
        // root
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
                w3mFunc(GlobalKeymap[ch]);

                g.prec_num = 0;
            }
        }
        // g.prev_key = g.CurrentKey;
        g.CurrentKey = -1;
        g.CurrentKeyData = null;
    }
}

pub fn dispatch_timeout() void {
    // timeout
    if (tasks_current()) |task| {
        task.enqueue(0);
        if (task.state() == .DEAD) {
            tasks_pop();
        }
    } else {
        root.enqueue(0);
    }
}

/// return -1 if timeout
/// call from coroutine
pub export fn getch_timeout(ms: u32, args: ?*c.CmdArgs) c_int {
    if (tasks_current()) |task| {
        return task.block(.fromMilliseconds(ms), args.?);
    } else {
        return root.block(.fromMilliseconds(ms), args.?);
    }
    // _ = args;
    //
    //
    // const io = evented.io();
    //
    // const InputOrTimeout = std.Io.Select(union(enum) {
    //     input: u8,
    //     timeout: void,
    // });
    // var buf: [1]InputOrTimeout.Union = undefined;
    // var select: InputOrTimeout = .init(io, &buf);
    //
    // select.async(.input, getch_async, .{io});
    // select.async(.timeout, sleep_async, .{ io, std.Io.Duration.fromMilliseconds(ms) });
    //
    // const winner = select.await() catch @panic("select.await");
    // defer select.cancelDiscard(); // cancel remaining, discard results
    // return switch (winner) {
    //     .input => |ch| ch,
    //     .timeout => -1,
    // };
}
