const std = @import("std");
const c = @cImport({
    @cInclude("w3m_runtime.h");
});
const vaxis = @import("vaxis");

const RawMode = struct {
    const Event = union(enum) {
        key_press: vaxis.Key,
        winsize: vaxis.Winsize,
        // focus_in,
        // foo: u8,
    };

    buffer: [1024]u8 = undefined,
    tty: vaxis.Tty = undefined,
    vx: vaxis.Vaxis = undefined,
    loop: vaxis.Loop(Event) = undefined,
    fn create(allocator: std.mem.Allocator) !*@This() {
        const this = try allocator.create(@This());
        this.* = .{};
        this.tty = try vaxis.Tty.init(&this.buffer);
        this.vx = try vaxis.init(allocator, .{});
        this.loop = .{ .tty = &this.tty, .vaxis = &this.vx };
        try this.loop.init();
        try this.loop.start();
        return this;
    }

    fn destroy(this: *@This(), allocator: std.mem.Allocator) void {
        this.loop.stop();
        this.vx.deinit(allocator, this.tty.writer());
        this.tty.deinit();
    }
};

const Term = struct {
    allocator: std.mem.Allocator,
    rawmode: ?*RawMode = null,

    fn init(allocator: std.mem.Allocator) @This() {
        return .{
            .allocator = allocator,
        };
    }

    fn deinit(this: *const @This()) void {
        _ = this;
    }

    fn enterRawMode(this: *@This()) !void {
        if (this.rawmode == null) {
            this.rawmode = try RawMode.create(this.allocator);
        }
    }

    fn exitRawMode(this: *@This()) void {
        if (this.rawmode) |rawmode| {
            rawmode.destroy(this.allocator);
            this.rawmode = null;
        }
    }
};
var g_term: Term = undefined;

/// return ture if enter main loop
extern fn w3m_args(argc: c_int, argv: [*c]const [*:0]u8) bool;

extern fn w3m_loop() void;

pub fn main() void {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.detectLeaks();
    const allocator = gpa.allocator();

    g_term = Term.init(allocator);
    defer g_term.deinit();

    if (w3m_args(@intCast(std.os.argv.len), &std.os.argv[0])) {
        w3m_loop();
    }
}
