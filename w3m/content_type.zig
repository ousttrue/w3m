const std = @import("std");
const g = @import("global.zig");
const runtime = @import("runtime.zig");

var mimetypes_list: std.ArrayList([]const u8) = .initBuffer(&.{});

var UserMimeTypes: std.ArrayList([]ExtContentType) = .initBuffer(&.{});

const ExtContentType = struct {
    ext: [:0]const u8,
    content_type: [:0]const u8,
};

const DefaultGuess = [_]ExtContentType{
    .{ .ext = "html", .content_type = "text/html" },
    .{ .ext = "htm", .content_type = "text/html" },
    .{ .ext = "shtml", .content_type = "text/html" },
    .{ .ext = "xhtml", .content_type = "application/xhtml+xml" },
    .{ .ext = "gif", .content_type = "image/gif" },
    .{ .ext = "jpeg", .content_type = "image/jpeg" },
    .{ .ext = "jpg", .content_type = "image/jpeg" },
    .{ .ext = "png", .content_type = "image/png" },
    .{ .ext = "xbm", .content_type = "image/xbm" },
    .{ .ext = "au", .content_type = "audio/basic" },
    .{ .ext = "gz", .content_type = "application/x-gzip" },
    .{ .ext = "Z", .content_type = "application/x-compress" },
    .{ .ext = "bz2", .content_type = "application/x-bzip" },
    .{ .ext = "tar", .content_type = "application/x-tar" },
    .{ .ext = "zip", .content_type = "application/x-zip" },
    .{ .ext = "lha", .content_type = "application/x-lha" },
    .{ .ext = "lzh", .content_type = "application/x-lha" },
    .{ .ext = "ps", .content_type = "application/postscript" },
    .{ .ext = "pdf", .content_type = "application/pdf" },
};

fn loadMimeTypes(allocator: std.mem.Allocator, filename: []const u8) !?[]ExtContentType {
    const f = std.Io.Dir.cwd().openFile(runtime.io, filename, .{}) catch {
        return null;
    };
    defer f.close(runtime.io);

    var read_buf: [128]u8 = undefined;
    var reader = f.reader(runtime.io, &read_buf);

    var list: std.ArrayList(ExtContentType) = .initBuffer(&.{});
    defer list.deinit(allocator);
    while (true) {
        const d = reader.interface.takeDelimiterInclusive('\n') catch {
            break;
        };
        if (d[0] == '#')
            continue;
        var it = std.mem.splitAny(u8, d, &std.ascii.whitespace);
        const ext = it.next() orelse {
            continue;
        };
        while (it.next()) |content_type| {
            if (content_type.len == 0)
                continue;

            try list.append(allocator, .{
                .ext = allocator.dupeZ(u8, ext) catch @panic("OOM"),
                .content_type = allocator.dupeZ(u8, content_type) catch @panic("OOM"),
            });
        }
    }
    if (list.items.len > 0) {
        return try list.toOwnedSlice(allocator);
    } else {
        return null;
    }
}

export fn initMimeTypes() void {
    // update mimetypes_list
    mimetypes_list.clearRetainingCapacity();
    if (g.mimetypes_files) |files| {
        var it = std.mem.splitScalar(u8, std.mem.span(files), ',');
        while (it.next()) |_item| {
            const item = std.mem.trim(u8, _item, &std.ascii.whitespace);
            if (item.len > 0) {
                mimetypes_list.append(runtime.allocator, item) catch @panic("OOM");
            }
        }
    }

    UserMimeTypes.clearRetainingCapacity();
    for (mimetypes_list.items) |item| {
        if (loadMimeTypes(runtime.allocator, item) catch @panic("loadMimeTypes")) |list| {
            UserMimeTypes.append(runtime.allocator, list) catch @panic("OOM");
        }
    }
}

fn guessContentTypeFromTable(table: []const ExtContentType, path: []const u8) ?[:0]const u8 {
    const ext = std.fs.path.extension(path);
    if (ext.len == 0) {
        return null;
    }

    for (table) |item| {
        if (std.ascii.eqlIgnoreCase(ext[1..], item.ext)) {
            return item.content_type;
        }
    }

    return null;
}

pub export fn guessContentType(_src: [*c]const u8) [*c]const u8 {
    const src = std.mem.span(_src);
    if (src.len == 0)
        return null;

    for (UserMimeTypes.items) |table| {
        if (guessContentTypeFromTable(table, src)) |content_type| {
            return content_type.ptr;
        }
    }

    if (guessContentTypeFromTable(&DefaultGuess, src)) |content_type| {
        return content_type.ptr;
    }

    return null;
}
