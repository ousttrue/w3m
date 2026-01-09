const std = @import("std");
const c = @import("c_include.zig").c;

const Section = struct {
    params: std.ArrayList(*c.param_ptr) = .{},
};

const Option = struct {
    allocator: std.mem.Allocator,
    sections: [c.SETTINGS_MAX]Section = [1]Section{.{}} ** c.SETTINGS_MAX,
    param_map: std.StringHashMap(*c.param_ptr) = undefined,

    fn init(allocator: std.mem.Allocator) @This() {
        return .{
            .allocator = allocator,
            .param_map = .init(allocator),
        };
    }

    fn deinit(this: *@This()) void {
        this.param_map.deinit();
    }

    fn register(this: *@This(), section: c.SettingsSections, p: *c.param_ptr) void {
        this.sections[section].params.append(this.allocator, p) catch {};
        this.param_map.put(std.mem.span(p.name), p) catch {};
    }
};

var g_opts: Option = undefined;

pub fn opt_init_alloc(allocator: std.mem.Allocator) void {
    g_opts = .init(allocator);
}
pub fn opt_deinit() void {
    g_opts.deinit();
}

export fn opt_register(section: c.SettingsSections, _p: [*c]c.param_ptr) void {
    const p: *c.param_ptr = _p orelse {
        return;
    };
    g_opts.register(section, p);
}

export fn opt_get_param(name: [*c]const u8) [*c]c.param_ptr {
    if (g_opts.param_map.get(std.mem.span(name))) |p| {
        return p;
    } else {
        return null;
    }
}
