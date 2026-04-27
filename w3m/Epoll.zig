const std = @import("std");
const MAX_EVENT = 5;

epoll_fd: i32,
signal_fd: i32,
events: [MAX_EVENT]std.os.linux.epoll_event = undefined,
event_count: usize = 0,
event_index: u32 = 0,

pub fn init() @This() {
    const epoll_fd = std.os.linux.epoll_create1(std.os.linux.EPOLL.CLOEXEC);

    var mask = std.os.linux.sigemptyset();
    std.os.linux.sigaddset(&mask, .WINCH);
    _ = std.os.linux.sigprocmask(std.os.linux.SIG.BLOCK, &mask, null);
    const signal_fd = std.os.linux.signalfd(-1, &mask, std.os.linux.SFD.CLOEXEC);

    var this: @This() = .{
        .epoll_fd = @intCast(epoll_fd),
        .signal_fd = @intCast(signal_fd),
    };
    this.add_fd(this.signal_fd);

    return this;
}

pub fn deinit(this: *@This()) void {
    std.posix.close(this.epoll_fd);
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

pub fn next(this: *@This(), timeout_ms: i32) !?std.os.linux.epoll_event {
    if (this.event_index >= this.event_count) {
        this.event_index = 0;
        this.event_count = std.os.linux.epoll_wait(
            this.epoll_fd,
            (&this.events).ptr,
            this.events.len,
            timeout_ms,
        );
        if (this.event_count == 0) {
            // timeout
            return null;
        }
        if (this.event_count > this.events.len) {
            // error ?
            this.event_count = 0;
            return error.index_over;
        }
    }

    if (this.event_index < this.event_count) {
        // valid
        defer this.event_index += 1;
        return this.events[this.event_index];
    }

    unreachable;
}
