const std = @import("std");
const c = @cImport({
    @cInclude("src/core/http_message.h");
});

test "http-message" {
    var s = c.Strnew();
    try std.testing.expect(s != null);
    try std.testing.expect(c.matchattr("attr=value;", "attr", 4, &s));
    try std.testing.expectEqualSlices(u8, "value", @as([:0]u8, std.mem.span(s.*.ptr)));
}
