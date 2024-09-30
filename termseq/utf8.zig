const std = @import("std");

export fn utf8sequence_to_codepoint(_utf8: ?[*]const u8) u32 {
    const utf8 = _utf8 orelse {
        return 0;
    };
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

export fn utf8sequence_from_str(_utf8: ?[*]const u8, _pOut: ?[*]u8) c_int {
    const utf8 = _utf8 orelse {
        return 0;
    };
    const pOut = _pOut orelse {
        return 0;
    };
    if (std.unicode.utf8ByteSequenceLength(utf8[0])) |len| {
        const p: [*]u8 = @ptrCast(pOut);
        const code_point_bytes: []u8 = p[0..5];
        switch (len) {
            1 => {
                code_point_bytes[0] = utf8[0];
                code_point_bytes[1] = 0;
                code_point_bytes[2] = 0;
                code_point_bytes[3] = 0;
                code_point_bytes[4] = 0;
            },
            2 => {
                code_point_bytes[0] = utf8[0];
                code_point_bytes[1] = utf8[1];
                code_point_bytes[2] = 0;
                code_point_bytes[3] = 0;
                code_point_bytes[4] = 0;
            },
            3 => {
                code_point_bytes[0] = utf8[0];
                code_point_bytes[1] = utf8[1];
                code_point_bytes[2] = utf8[2];
                code_point_bytes[3] = 0;
                code_point_bytes[4] = 0;
            },
            4 => {
                code_point_bytes[0] = utf8[0];
                code_point_bytes[1] = utf8[1];
                code_point_bytes[2] = utf8[2];
                code_point_bytes[3] = utf8[3];
                code_point_bytes[4] = 0;
            },
            else => {},
        }
        return len;
    } else |_| {
        return 0;
    }
}

export fn utf8sequence_from_codepoint(codepoint: u32, _pOut: ?[*]u8) c_int {
    const pOut = _pOut orelse {
        return 0;
    };
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

export fn utf8sequence_len(_utf8: ?[*]const u8) c_int {
    const utf8 = _utf8 orelse {
        return 0;
    };
    if (std.unicode.utf8ByteSequenceLength(utf8[0])) |len| {
        return len;
    } else |_| {
        return 0;
    }
}

export fn utf8str_codepoint_count(_utf8: ?[*:0]const u8) c_int {
    const utf8 = _utf8 orelse {
        return 0;
    };
    const p = std.mem.span(utf8);
    if (std.unicode.calcUtf16LeLen(p)) |len| {
        return @intCast(len);
    } else |_| {
        return 0;
    }
}

var bufffer: [128]u8 = undefined;

export fn utf8str_utf16str(_utf8: ?[*:0]const u8, pOut: [*c]u16, len: usize) c_int {
    const utf8 = _utf8 orelse {
        return 0;
    };
    const inBuf = std.mem.span(utf8);
    const p: [*]u8 = @ptrCast(pOut);
    const outBuf: []u8 = p[0 .. len * 2];
    var fba = std.heap.FixedBufferAllocator.init(outBuf);
    if (std.unicode.utf8ToUtf16LeAllocZ(fba.allocator(), inBuf)) |utf16| {
        if (std.ascii.isASCII(utf8[0])) {} else {
            const a: c_int = 0;
            _ = a;
        }
        return @intCast(utf16.len);
    } else |_| {
        return 0;
    }
}
