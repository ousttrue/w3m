const std = @import("std");
extern fn _main(argc: c_int, argv: *[*c]const u8) c_int;

pub fn main() anyerror!void {
    const allocator = std.heap.page_allocator;
    var it = try std.process.ArgIterator.initWithAllocator(allocator);
    defer it.deinit();

    var args = std.ArrayList([*c]const u8).init(allocator);
    defer args.deinit();
    while (it.next()) |arg| {
        try args.append(&arg[0]);
    }
    try args.append(null);

    std.debug.assert(_main(
        @intCast(c_int, args.items.len)-1,
        &args.items[0],
    ) == 0);
}
