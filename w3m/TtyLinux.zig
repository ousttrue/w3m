const std = @import("std");
const c = @cImport({
    @cInclude("termios.h");
});

allocator: std.mem.Allocator,

stdin: std.Io.File,
in_buf: [1024]u8 = undefined,
stdin_reader: std.Io.File.Reader,

stdout: std.Io.File,
out_buf: [128]u8 = undefined,

termios: std.posix.termios,
stdout_writer: std.Io.File.Writer,

pub fn create(
    allocator: std.mem.Allocator,
    io: std.Io,
    stdin: std.Io.File,
    stdout: std.Io.File,
) *@This() {
    var this = allocator.create(@This()) catch @panic("OOM");
    this.* = .{
        .allocator = allocator,

        .stdin = stdin,
        .stdin_reader = stdin.reader(io, &this.in_buf),

        .termios = std.posix.tcgetattr(stdin.handle) catch {
            @panic("tcgetattr");
        },

        .stdout = stdout,
        .stdout_writer = stdout.writer(io, &this.out_buf),
    };
    return this;
}

pub fn destroy(this: *@This()) void {
    this.restore();
    this.allocator.destroy(this);
}

pub fn restore(this: *@This()) void {
    std.posix.tcsetattr(this.stdin.handle, .FLUSH, this.termios) catch |err| {
        std.log.err("couldn't restore terminal: {}", .{err});
    };
}

// pub fn ttymode_add_local_input(this:*@This(), mode: c_int, imode: c_int)
// {
//     struct termios ioval;
//     tcgetattr(tty, &ioval);
//
//     ioval.c_lflag |= mode;
//     ioval.c_iflag |= imode;
//
//     while (tcsetattr(tty, TCSANOW, &ioval) == -1) {
//         if (errno == EINTR || errno == EAGAIN)
//             continue;
//         printf("Error occurred while set %x: errno=%d\n", mode, errno);
//         reset_error_exit(SIGNAL_ARGLIST);
//     }
// }

// pub fn set_cc(this: *@This(), spec: c_int, val: c_int) !void {
//     var ioval = try std.posix.tcgetattr(this.file.handle);
//     ioval.c_cc[spec] = val;
//     try std.posix.tcsetattr(this.file.fhandle, .TCSANOW, &ioval);
//     // == -1) {
//     //     if (errno == EINTR || errno == EAGAIN)
//     //         continue;
//     //     printf("Error occurred: errno=%d\n", errno);
//     //     reset_error_exit(SIGNAL_ARGLIST);
//     // }
// }

pub fn raw(this: *@This()) !void {
    var termios = this.termios;
    termios.lflag.ISIG = false;
    termios.lflag.ICANON = false;
    termios.lflag.ECHO = false;
    termios.lflag.IEXTEN = false;
    termios.iflag.IXON = false;
    termios.iflag.IXOFF = false;
    termios.iflag.INLCR = false;
    termios.iflag.IGNCR = false;
    termios.iflag.ICRNL = false;
    termios.cc[c.VMIN] = 1;
    try std.posix.tcsetattr(this.stdin.handle, .NOW, termios);
}

pub fn cooked(this: *@This(), opts: struct { echo: bool = true }) !void {
    var termios = this.termios;
    termios.lflag.ISIG = true;
    termios.lflag.ICANON = true;
    termios.lflag.ECHO = opts.echo;
    termios.lflag.IEXTEN = true;
    termios.cc[c.VMIN] = 4;
    try std.posix.tcsetattr(this.stdin.handle, .NOW, termios);
}

/// disables line buffering and erase/kill character-processing
pub fn crmode(this: *@This(), enable: bool) !void {
    var termios = try std.posix.tcgetattr(this.stdin.handle);
    if (enable) {
        termios.lflag.ICANON = false;
        termios.lflag.ISIG = true;
        termios.iflag.IXON = false;
        termios.cc[c.VMIN] = 1;
    } else {
        termios.lflag.ICANON = true;
        termios.cc[c.VMIN] = 4;
        @panic("not impl");
    }
    try std.posix.tcsetattr(this.stdin.handle, .NOW, termios);
}

pub fn echo(this: *@This(), enable: bool) !void {
    var termios = try std.posix.tcgetattr(this.stdin.handle);
    // var termios = this.termios;
    termios.lflag.ECHO = enable;
    try std.posix.tcsetattr(this.stdin.handle, .NOW, termios);
}

pub fn getch(this: *@This()) u8 {
    while (true) {
        var buf: [1]u8 = undefined;
        this.stdin_reader.interface.readSliceAll(&buf) catch |e| {
            std.log.err("{}", .{e});
            //     if (errno == EINTR || errno == EAGAIN)
            //         continue;
            break;
        };
        return buf[0];
    }
    unreachable;
}

pub fn puts(this: *@This(), str: []const u8) !void {
    return try this.stdout_writer.interface.writeAll(str);
}

pub fn putc(this: *@This(), ch: u8) !void {
    try this.stdout_writer.interface.writeByte(ch);
}
