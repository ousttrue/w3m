const std = @import("std");
const c = @import("c_include.zig").c;

const SelectItem = struct {
    value: c_int,
    cvalue: []const u8,
    text: []const u8,
};

fn GetSet(T: type) type {
    return struct {
        ptr: *anyopaque,

        fn get(this: *@This()) T {
            return @as(*T, @ptrCast(this.ptr)).*;
        }
    };
}

const GetSetText = struct {
    ptr: *anyopaque,
};

const TypedGetSet = union(enum) {
    P_INT: GetSet(i32),
    P_SHORT: GetSet(i16),
    P_CHARINT: GetSet(i8),
    P_CHAR: GetSet(u8),
    P_STRING: GetSetText,
    P_SSLPATH: GetSetText,
    P_COLOR: GetSet(u32),
    P_CODE: GetSet(c.wc_ces),
    P_PIXELS: GetSet(u32),
    P_NZINT: GetSet(i34),
    P_SCALE: GetSet(f64),
};

const Param = struct {
    name: []const u8,
    comment: []const u8,
    getset: TypedGetSet,
};

const Section = struct {
    params: std.ArrayList(Param) = .{},
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
        var param = Param{
            .name = std.mem.span(p.name),
            .comment = std.mem.span(p.comment),
            .getset = undefined,
        };
        switch (p.type) {
            c.P_INT => {
                param.getset = .{ .P_INT = .{ .ptr = p.varptr.? } };
            },
            c.P_SHORT => {
                param.getset = .{ .P_SHORT = .{ .ptr = p.varptr.? } };
            },
            c.P_CHARINT => {
                param.getset = .{ .P_CHARINT = .{ .ptr = p.varptr.? } };
            },
            c.P_CHAR => {
                param.getset = .{ .P_CHAR = .{ .ptr = p.varptr.? } };
            },
            c.P_STRING => {
                param.getset = .{ .P_STRING = .{ .ptr = p.varptr.? } };
            },
            c.P_SSLPATH => {
                param.getset = .{ .P_SSLPATH = .{ .ptr = p.varptr.? } };
            },
            c.P_COLOR => {
                param.getset = .{ .P_COLOR = .{ .ptr = p.varptr.? } };
            },
            c.P_CODE => {
                param.getset = .{ .P_CODE = .{ .ptr = p.varptr.? } };
            },
            c.P_PIXELS => {
                param.getset = .{ .P_PIXELS = .{ .ptr = p.varptr.? } };
            },
            c.P_NZINT => {
                param.getset = .{ .P_NZINT = .{ .ptr = p.varptr.? } };
            },
            c.P_SCALE => {
                param.getset = .{ .P_SCALE = .{ .ptr = p.varptr.? } };
            },
            else => @panic("unknown"),
        }
        this.sections[section].params.append(this.allocator, param) catch {};
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
