const std = @import("std");

export fn utf8sequence_to_codepoint(utf8: [*]const u8) u32 {
    if (std.unicode.utf8ByteSequenceLength(utf8[0])) |len| {
        const p: [*]const u8 = @ptrCast(utf8);
        const code_point_bytes: []const u8 = p[0..len];
        if (std.unicode.utf8Decode(code_point_bytes)) |codepoint| {
            return codepoint;
        } else |_| {
            return 0;
        }
    } else |_| {
        return 0;
    }
}

export fn utf8sequence_from_str(utf8: [*c]const u8, pOut: [*c]u8) c_int {
    if (std.unicode.utf8ByteSequenceLength(utf8[0])) |len| {
        const p: [*]u8 = @ptrCast(pOut);
        const code_point_bytes: []u8 = p[0..4];
        switch (len) {
            1 => {
                code_point_bytes[0] = utf8[0];
                code_point_bytes[1] = 0;
                code_point_bytes[2] = 0;
                code_point_bytes[3] = 0;
            },
            2 => {
                code_point_bytes[0] = utf8[0];
                code_point_bytes[1] = utf8[1];
                code_point_bytes[2] = 0;
                code_point_bytes[3] = 0;
            },
            3 => {
                code_point_bytes[0] = utf8[0];
                code_point_bytes[1] = utf8[1];
                code_point_bytes[2] = utf8[2];
                code_point_bytes[3] = 0;
            },
            4 => {
                code_point_bytes[0] = utf8[0];
                code_point_bytes[1] = utf8[1];
                code_point_bytes[2] = utf8[2];
                code_point_bytes[3] = utf8[3];
            },
            else => {},
        }
        return len;
    } else |_| {
        return 0;
    }
}

export fn utf8sequence_from_codepoint(codepoint: u32, pOut: [*c]u8) c_int {
    const p: [*]u8 = @ptrCast(pOut);
    const code_point_bytes: []u8 = p[0..4];
    if (std.unicode.utf8Encode(@intCast(codepoint), code_point_bytes)) |bytes_encoded| {
        switch (bytes_encoded) {
            1 => {
                pOut[1] = 0;
                pOut[2] = 0;
                pOut[3] = 0;
            },
            2 => {
                pOut[2] = 0;
                pOut[3] = 0;
            },
            3 => {
                pOut[3] = 0;
            },
            4 => {},
            else => {
                //?
            },
        }
        return @intCast(bytes_encoded);
    } else |_| {
        return 0;
    }
}

export fn utf8sequence_len(utf8: [*c]const u8) c_int {
    if (std.unicode.utf8ByteSequenceLength(utf8[0])) |len| {
        return len;
    } else |_| {
        return 0;
    }
}
