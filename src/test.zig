const std = @import("std");
const c = @cImport({
    @cInclude("src/content/http_message.h");
});

fn toSlice(slice: c.CharSlice) ?[]u8 {
    if (slice.p) |p| {
        return p[0..slice.len];
    } else {
        return null;
    }
}

test "http-message" {
    const slice = c.extractSemiColon("attr=value;", c.makeSlice("attr"));
    try std.testing.expectEqualSlices(u8, "value", slice.p[0..slice.len]);
}
