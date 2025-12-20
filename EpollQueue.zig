const std = @import("std");
const Queue = @import("queue.zig").Queue;

const Event = union(enum) {
    idle,
    key: u8,
};

const MAX_EVENT = 5;
const READ_BUFSIZE = 64 * 1024;
const WRITE_BUFSIZE = 64 * 1024;

allocator: std.mem.Allocator,
thread: ?std.Thread = null,
should_quit: bool = false,
epoll_fd: i32,
queue: Queue(Event, 512) = .{},

pub fn create(allocator: std.mem.Allocator) !*@This() {
    const this = try allocator.create(@This());
    this.* = .{
        .allocator = allocator,
        .epoll_fd = @intCast(std.os.linux.epoll_create1(
            std.os.linux.EPOLL.CLOEXEC,
        )),
    };
    return this;
}

pub fn destroy(this: *@This()) void {
    this.stop();
    std.posix.close(this.epoll_fd);
    this.allocator.destroy(this);
}

pub fn add_fd(this: *@This(), fd: c_int) void {
    var read_event = std.os.linux.epoll_event{
        .events = std.os.linux.EPOLL.IN,
        .data = .{ .fd = fd },
    };
    std.debug.assert(0 == std.os.linux.epoll_ctl(
        this.epoll_fd,
        std.os.linux.EPOLL.CTL_ADD,
        read_event.data.fd,
        &read_event,
    ));
}

pub fn start(this: *@This()) !void {
    if (this.thread == null) {
        this.thread = try std.Thread.spawn(.{}, readThread, .{
            this,
        });
    }
}

/// stops reading from the tty.
pub fn stop(this: *@This()) void {
    // If we don't have a thread, we have nothing to stop
    if (this.thread == null) return;
    this.should_quit = true;
    if (this.thread) |thread| {
        thread.join();
        this.thread = null;
        this.should_quit = false;
    }
}

/// returns the next available event, blocking until one is available
pub fn nextEvent(this: *@This()) Event {
    return this.queue.pop();
}

/// blocks until an event is available. Useful when your application is
/// operating on a poll + drain architecture (see tryEvent)
pub fn pollEvent(this: *@This()) void {
    this.queue.poll();
}

/// returns an event if one is available, otherwise null. Non-blocking.
pub fn tryEvent(this: *@This()) ?Event {
    return this.queue.tryPop();
}

/// posts an event into the event queue. Will block if there is not
/// capacity for the event
pub fn postEvent(this: *@This(), event: Event) void {
    this.queue.push(event);
}

pub fn tryPostEvent(this: *@This(), event: Event) bool {
    return this.queue.tryPush(event);
}

fn readThread(this: *@This()) !void {
    const timeout_ms = 100;
    while (!this.should_quit) {
        var events: [MAX_EVENT]std.os.linux.epoll_event = undefined;
        const event_count = std.os.linux.epoll_wait(
            this.epoll_fd,
            @ptrCast(&events[0]),
            events.len,
            timeout_ms,
        );
        if (event_count == 0) {
            this.postEvent(.{ .idle = void{} });
            continue;
        }
        if (event_count >= events.len) {
            // debugger ? error ?
            // time out ?
            this.postEvent(.{ .idle = void{} });
            continue;
        }

        // var write_buffer: [WRITE_BUFSIZE]u8 = .{};

        for (events[0..event_count]) |ev| {
            // if (ev.data.fd == read_event.data.fd)
            {
                var read_buffer: [READ_BUFSIZE]u8 = undefined;
                const bytes_read = std.os.linux.read(
                    ev.data.fd,
                    @ptrCast(&read_buffer[0]),
                    read_buffer.len,
                );
                if (bytes_read > 0) {
                    for (read_buffer[0..bytes_read]) |b| {
                        this.postEvent(.{
                            .key = b,
                        });
                    }
                } else {
                    //
                }
            }
            // else if (ev.data.fd == signal_event.data.fd) {
            //     var buf: [@sizeOf(os.linux.signalfd_siginfo)]u8 align(8) = undefined;
            //     if (buf.len != try os.read(signal_event.data.fd, &buf)) {
            //         return os.ReadError.ReadError;
            //     }
            //     const info = @ptrCast(*os.linux.signalfd_siginfo, &buf);
            //     switch (info.signo) {
            //         os.linux.SIG.INT => {
            //             log.info("{d}:Got SIGINT", .{time.milliTimestamp()});
            //             running = false;
            //         },
            //         os.linux.SIG.TERM => {
            //             log.info("{d}:Got SIGTERM", .{time.milliTimestamp()});
            //             running = false;
            //         },
            //         os.linux.SIG.USR1 => {
            //             log.info("{d}:Set verbose=false", .{time.milliTimestamp()});
            //             verbose = false;
            //         },
            //         os.linux.SIG.USR2 => {
            //             log.info("{d}:Set verbose=true", .{time.milliTimestamp()});
            //             verbose = true;
            //         },
            //         else => unreachable,
            //     }
            // } else {
            //     unreachable;
            // }
        }
    }
}
