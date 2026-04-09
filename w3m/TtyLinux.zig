const std = @import("std");
const c = @cImport({
    @cInclude("termios.h");
});

stdin: std.Io.File,
termios: std.posix.termios,

pub fn init(
    stdin: std.Io.File,
) @This() {
    return .{
        .stdin = stdin,
        .termios = std.posix.tcgetattr(stdin.handle) catch {
            @panic("tcgetattr");
        },
    };
}

pub fn deinit(this: @This()) void {
    this.restore();
}

pub fn restore(this: *@This()) void {
    std.posix.tcsetattr(this.stdin.handle, .FLUSH, this.termios) catch |err| {
        std.log.err("couldn't restore terminal: {}", .{err});
    };
}

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
