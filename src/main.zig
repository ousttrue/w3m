const std = @import("std");
const GcAllocator = @import("GcAllocator.zig");

extern fn w3m_main(argc: c_int, argv: [*c][*c]c_char) c_int;

pub fn main() !u8 {
    // var alloc = GcAllocator.allocator();
    //
    // // We'll write to the terminal
    // const _stdout = std.fs.File.stdout();
    // var buf: [1024]u8 = undefined;
    // var stdout = _stdout.writer(&buf);
    //
    // // Compare the output by enabling/disabling
    // // gc.disable();
    //
    // // Allocate a bunch of stuff and never free it, outputting
    // // the heap size along the way. When the GC is enabled,
    // // it'll stabilize at a certain size.
    // var i: u64 = 0;
    // while (i < 10_000_000) : (i += 1) {
    //     // This is all really ugly but its not idiomatic Zig code so
    //     // just take this at face value. We're doing weird stuff here
    //     // to show that we're collecting garbage.
    //     const p: **u8 = @ptrCast(try alloc.alloc(*u8, @sizeOf(*u8)));
    //     const q = try alloc.alloc(u8, @sizeOf(u8));
    //     _ = alloc.resize(q, 2 * @sizeOf(u8));
    //     p.* = @ptrCast(q);
    //
    //     if (i % 100_000 == 0) {
    //         const heap = GcAllocator.getHeapSize();
    //         try stdout.interface.print("heap size: {d}\n", .{heap});
    //     }
    // }

    const code = w3m_main(@intCast(std.os.argv.len), @ptrCast(&std.os.argv[0]));
    return @intCast(code);
}
