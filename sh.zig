const std = @import("std");

export fn sh_exists(_path: [*c]const u8) bool {
    if (_path) |path| {
        if (std.fs.cwd().statFile(std.mem.span(path))) |_| {
            return true;
        } else |_| {}
    }
    return false;
}

