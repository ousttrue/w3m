const std = @import("std");

export fn utf8sequence_from_codepoint(codepoint: u32, pOut: [*c]u8) c_int {
    const p: [*]u8 = @ptrCast(pOut);
    const code_point_bytes: []u8 = p[0..4];
    if (std.unicode.utf8Encode(@intCast(codepoint), code_point_bytes)) |bytes_encoded| {
        return @intCast(bytes_encoded);
    } else |_| {
        return 0;
    }
}
