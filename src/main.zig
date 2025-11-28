const std = @import("std");
const zlua = @import("zlua");
const Lua = zlua.Lua;

extern fn w3m_parse_arg(argc: c_int, argv: [*c][*c]c_char) c_int;
extern fn w3m_loop() c_int;

pub fn main() !u8 {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    const allocator = gpa.allocator();
    defer _ = gpa.deinit();

    var lua = try Lua.init(allocator);
    defer lua.deinit();

    lua.pushInteger(42);
    std.debug.print("{}\n", .{try lua.toInteger(1)});

    if(w3m_parse_arg(@intCast(std.os.argv.len), @ptrCast(&std.os.argv[0]))==0){
        return 1;
    }
    const code = w3m_loop();
    return @as(u8, @intCast(code));
}
