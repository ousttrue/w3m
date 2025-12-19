const std = @import("std");
const EpollQueue = @import("EpollQueue.zig");

allocator: std.mem.Allocator,
input: std.fs.File,
is_tty: bool,
is_rawmode: bool = false,
termios: ?std.posix.termios = null,
buffer: [256]u8 = undefined,
queue: ?*EpollQueue = null,

pub fn init(allocator: std.mem.Allocator, input: std.fs.File) !@This() {
    var this = @This(){
        .allocator = allocator,
        .input = input,
        .is_tty = std.c.isatty(input.handle) != 0,
    };
    if (this.is_tty) {
        this.termios = try std.posix.tcgetattr(this.input.handle);
    }

    return this;
}

pub fn deinit(this: *@This()) void {
    this.exitRawMode();
}

pub fn enterRawMode(this: *@This()) !void {
    if (this.termios) |termios| {
        var raw = termios;
        // see termios(3)
        raw.iflag.IGNBRK = false;
        raw.iflag.BRKINT = false;
        raw.iflag.PARMRK = false;
        raw.iflag.ISTRIP = false;
        raw.iflag.INLCR = false;
        raw.iflag.IGNCR = false;
        raw.iflag.ICRNL = false;
        raw.iflag.IXON = false;

        raw.oflag.OPOST = false;

        raw.lflag.ECHO = false;
        raw.lflag.ECHONL = false;
        raw.lflag.ICANON = false;
        raw.lflag.ISIG = false;
        raw.lflag.IEXTEN = false;

        raw.cflag.CSIZE = .CS8;
        raw.cflag.PARENB = false;

        raw.cc[@intFromEnum(std.posix.V.MIN)] = 1;
        raw.cc[@intFromEnum(std.posix.V.TIME)] = 0;
        try std.posix.tcsetattr(this.input.handle, .FLUSH, raw);
        this.is_rawmode = true;

        if(this.queue)|queue|{
            // drop input queue
            queue.destroy();
            this.queue = null;
        }

        const queue = try EpollQueue.create(this.allocator);
        queue.add_fd(this.input.handle);
        try queue.start();
        this.queue = queue;
    }
}

pub fn exitRawMode(this: *@This()) void {
    if (this.termios) |termios| {
        if (this.queue) |queue| {
            queue.destroy();
            this.queue = null;
        }
        std.posix.tcsetattr(this.input.handle, .FLUSH, termios) catch @panic("exitRawMode");
        this.is_rawmode = false;
    }
}

pub fn cbreakMode(this: *@This()) !void {
    if (!this.is_rawmode) {
        return;
    }
    if (this.termios) |_| {
        var cbreak = try std.posix.tcgetattr(this.input.handle);
        cbreak.lflag.ISIG = true;
        // cbreak.cc[@intFromEnum(std.posix.V.MIN)] = 4;
        std.posix.tcsetattr(this.input.handle, .FLUSH, cbreak) catch @panic("exitRawMode");
    }
}

pub fn getWinsize(this: @This()) !std.posix.winsize {
    var winsize = std.posix.winsize{
        .row = 0,
        .col = 0,
        .xpixel = 0,
        .ypixel = 0,
    };

    const err = std.posix.system.ioctl(this.input.handle, std.posix.T.IOCGWINSZ, @intFromPtr(&winsize));
    if (std.posix.errno(err) == .SUCCESS)
        return winsize;
    return error.IoctlError;
}

pub fn getch(this: *@This(), onIdle: *const fn () callconv(.c) void) u8 {
    if (this.queue) |queue| {
        while (true) {
            const event = queue.nextEvent();
            switch (event) {
                .key => |key| {
                    return key;
                },
                .idle => {
                    onIdle();
                },
            }
        }
    } else {
        var buf: [1]u8 = undefined;
        if (this.input.read(&buf)) |size| {
            if (size == 1) {
                return buf[0];
            } else {
                return 0;
            }
        } else |err| {
            @panic(@errorName(err));
        }
    }
}
