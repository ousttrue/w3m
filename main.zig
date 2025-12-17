const std = @import("std");
const c = @cImport({
    @cInclude("w3m_runtime.h");
});
const vaxis = @import("vaxis");

const RawMode = struct {
    fn create(allocator: std.mem.Allocator) !*@This()
    {
        const this = try allocator.create(@This());
        this.* = .{
        };
        return this;
    }

    fn destroy(this: *@This(), allocator: std.mem.Allocator)void
    {
        _ = this;
        _ = allocator;
    }
};

const Term = struct {
    allocator: std.mem.Allocator,

    fn init(allocator: std.mem.Allocator) @This() {
        return .{
            .allocator = allocator,
        };
    }

    fn deinit(this: *const @This()) void {
        _ = this;
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
