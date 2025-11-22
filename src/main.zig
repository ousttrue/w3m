const std = @import("std");
const zlua = @import("zlua");
const Lua = zlua.Lua;

extern fn w3m_main(argc: c_int, argv: [*c][*c]c_char) c_int;

pub fn main() !u8 {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    const allocator = gpa.allocator();
    defer _ = gpa.deinit();

    var lua = try Lua.init(allocator);
    defer lua.deinit();

    lua.pushInteger(42);
    std.debug.print("{}\n", .{try lua.toInteger(1)});

    const code = w3m_main(@intCast(std.os.argv.len), @ptrCast(&std.os.argv[0]));
    return @intCast(code);
}
