const std = @import("std");
const c = @cImport({
    @cInclude("w3m.h");
    @cInclude("buffer_list.h");
    @cInclude("keymap.h");
    @cInclude("minicoro.h");
});
const message_queue = @import("message_queue.zig");
const defun = @import("defun.zig");

const InputEvent = union(enum) {
    timeout,
    err: c_int,
    tty_char: u8,
};
const InputEventQueue = message_queue.MessageQueue(InputEvent);
const ModalKeyInputQueue = message_queue.MessageQueue(u8);

const InputContext = struct {
    request_exit: bool = false,
    queue: InputEventQueue,
    // secondary key input queue
    use_modal: bool = false,
    queue_modal: ModalKeyInputQueue,
};
var event_buf: [128]InputEvent = undefined;
var modal_buf: [128]u8 = undefined;
var ctx: InputContext = .{
    .queue = InputEventQueue.init(&event_buf),
    .queue_modal = ModalKeyInputQueue.init(&modal_buf),
};
const GetChFunc = fn () callconv(.c) c_int;

export fn GetCh() c_int {
    return ctx.queue_modal.dequeue();
}

export fn event_begin_input(timeout_ms: c_int) ?*const GetChFunc {
    ctx.use_modal = true;
    _ = timeout_ms;
    return &GetCh;
}

export fn event_end_input(_: *const GetChFunc) void {
    ctx.use_modal = false;
}

fn addFunc(f: *const defun.CommandFunc, name: []const u8, desc: []const u8) void {
    _ = f;
    _ = name;
    _ = desc;
}

export fn coro_entry(co: [*c]c.mco_coro) void {
    std.debug.print("coroutine 1\n", .{});
    _ = c.mco_yield(co);
    std.debug.print("coroutine 2\n", .{});
}

pub fn main() !void {
    // c.initialize();
    // defun.init(addFunc);
    // c.parseArgs(
    //     @intCast(std.os.argv.len),
    //     @ptrCast(std.os.argv),
    // );
    // try main_loop();
    c.main_loop(
        @intCast(std.os.argv.len),
        @ptrCast(std.os.argv),
    );
}

fn createSignalfd() i32 {
    var mask = std.os.linux.sigemptyset();
    // std.os.linux.sigaddset(&mask, std.os.linux.SIG.INT);
    // std.os.linux.sigaddset(&mask, std.os.linux.SIG.TERM);
    std.os.linux.sigaddset(&mask, std.os.linux.SIG.USR1);
    // std.os.linux.sigaddset(&mask, std.os.linux.SIG.USR2);
    _ = std.os.linux.sigprocmask(std.os.linux.SIG.BLOCK, &mask, null);
    return @intCast(std.os.linux.signalfd(-1, &mask, std.os.linux.SFD.CLOEXEC));
}

fn producer() !void {
    const epoll_fd: i32 = @intCast(std.os.linux.epoll_create1(std.os.linux.EPOLL.CLOEXEC));
    defer _ = std.os.linux.close(epoll_fd);

    // stdin
    {
        var event = std.os.linux.epoll_event{
            .events = std.os.linux.EPOLL.IN,
            .data = std.os.linux.epoll_data{ .fd = std.posix.STDIN_FILENO },
        };
        _ = std.os.linux.epoll_ctl(
            epoll_fd,
            std.os.linux.EPOLL.CTL_ADD,
            event.data.fd,
            &event,
        );
    }

    // signalfd
    const signal_fd = createSignalfd();
    defer _ = std.os.linux.close(signal_fd);
    {
        var event = std.os.linux.epoll_event{
            .events = std.os.linux.EPOLL.IN,
            .data = std.os.linux.epoll_data{ .fd = signal_fd },
        };
        _ = std.os.linux.epoll_ctl(
            epoll_fd,
            std.os.linux.EPOLL.CTL_ADD,
            event.data.fd,
            &event,
        );
    }

    while (!ctx.request_exit) {
        var events: [5]std.os.linux.epoll_event = undefined;
        const event_count = std.os.linux.epoll_wait(
            epoll_fd,
            @ptrCast(&events),
            events.len,
            -1,
        );
        if (event_count > events.len) {
            std.log.info("{}:epoll_wait", .{event_count});
            continue;
        }

        if (event_count == 0) {
            std.log.info("{d}:timeout", .{std.time.milliTimestamp()});
            continue;
        }

        for (events[0..event_count]) |ev| {
            if (ev.data.fd == std.posix.STDIN_FILENO) {
                var ready_read_size: c_int = undefined;
                _ = std.os.linux.ioctl(ev.data.fd, std.os.linux.T.FIONREAD, @intFromPtr(&ready_read_size));
                for (0..@as(usize, @intCast(ready_read_size))) |_| {
                    var ch: u8 = undefined;
                    const ret = std.os.linux.read(ev.data.fd, @ptrCast(&ch), 1);
                    // std.log.debug("ret {}", .{ret});
                    if (ret < 0) {
                        ctx.queue.enqueue(.{
                            .err = 1,
                        });
                    } else if (ret == 0) {
                        ctx.queue.enqueue(.{
                            .err = 2,
                        });
                    } else if (ret == 1) {
                        if (ctx.use_modal) {
                            ctx.queue_modal.enqueue(ch);
                        } else {
                            ctx.queue.enqueue(.{
                                .tty_char = ch,
                            });
                        }
                    } else {
                        ctx.queue.enqueue(.{
                            .err = 3,
                        });
                    }
                }
            } else if (ev.data.fd == signal_fd) {
                ctx.request_exit = true;
                ctx.queue.enqueue(.{
                    .err = 3,
                });
            } else {}
        }
    }
}

fn main_loop() !void {
    const thread = try std.Thread.spawn(.{}, producer, .{});

    while (!ctx.request_exit) {
        if (!c.onFrame()) {
            continue;
        }

        const event = ctx.queue.dequeue();
        switch (event) {
            .timeout => {},
            .err => {
                break;
            },
            .tty_char => |ch| {
                c.onKeyInput(ch);
            },
        }
    }

    _ = std.posix.system.raise(std.posix.SIG.USR1);
    thread.join();
    c.fmTerm();
}
