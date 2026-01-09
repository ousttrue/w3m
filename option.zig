const std = @import("std");
const c = @import("c_include.zig").c;

const CMT_HELPER = "External Viewer Setup";

const NULL = @as([*c]const u8, @ptrFromInt(0));

fn GetSet(T: type) type {
    return struct {
        ptr: *anyopaque,

        fn get(this: *@This()) T {
            return @as(*T, @ptrCast(this.ptr)).*;
        }

        fn to_str(this: *@This()) [:0]const u8 {
            _ = this;
            return "no impl";
        }
    };
}

const GetSetText = struct {
    ptr: *anyopaque,
};

const TypedGetSet = union(enum) {
    P_INT: GetSet(u32),
    P_CODE: GetSet(u32),
    P_COLOR: GetSet(u32),
    P_NZINT: GetSet(i32),
    P_SHORT: GetSet(i16),
    P_CHARINT: GetSet(i8),
    P_CHAR: GetSet(i8),
    P_STRING: GetSetText,
    P_SSLPATH: GetSetText,
    P_PIXELS: GetSet(f64),
    P_SCALE: GetSet(f64),
};

const SelectItem = struct {
    value: u32,
    cvalue: [:0]const u8,
    text: [:0]const u8,
};

const Param = struct {
    name: [:0]const u8,
    comment: [:0]const u8,
    input_type: c.ParamInputTypes,
    getset: TypedGetSet,
    select: []SelectItem = &.{},

    fn to_str(this: *@This()) [*c]const u8 {
        switch (this.getset) {
            .P_PIXELS, .P_SCALE => |getset| {
                const ptr: *f64 = @ptrCast(@alignCast(getset.ptr));
                return c.Sprintf("%g", ptr.*).*.ptr;
            },
            .P_INT, .P_COLOR, .P_CODE => |getset| {
                const ptr: *u32 = @ptrCast(@alignCast(getset.ptr));
                return c.Sprintf("%d", ptr.*).*.ptr;
            },
            .P_NZINT => |getset| {
                const ptr: *i32 = @ptrCast(@alignCast(getset.ptr));
                return c.Sprintf("%d", ptr.*).*.ptr;
            },
            .P_SHORT => |getset| {
                const ptr: *i16 = @ptrCast(@alignCast(getset.ptr));
                return c.Sprintf("%d", ptr.*).*.ptr;
            },
            .P_CHARINT => |getset| {
                const ptr: *i8 = @ptrCast(getset.ptr);
                return c.Sprintf("%d", ptr.*).*.ptr;
            },
            .P_CHAR => |getset| {
                const ptr: *i8 = @ptrCast(getset.ptr);
                return c.Sprintf("%c", ptr.*).*.ptr;
            },
            .P_STRING, .P_SSLPATH => |getset| {
                //  SystemCharset -> InnerCharset
                const ptr: [*c][*]const u8 = @ptrCast(@alignCast(getset.ptr));
                return c.Strnew_charp(c.conv_from_system(ptr.*)).*.ptr;
            },
        }
    }
};

const Section = struct {
    name: [:0]const u8 = &.{},
    params: std.ArrayList(Param) = .{},
};

const Option = struct {
    allocator: std.mem.Allocator,
    sections: [c.SETTINGS_MAX]Section = .{
        .{ .name = "Display Settings" },
        .{ .name = "Color Settings" },
        .{ .name = "Miscellaneous Settings" },
        .{ .name = "Directory Settings" },
        .{ .name = "External Program Settings" },
        .{ .name = "Network Settings" },
        .{ .name = "Proxy Settings" },
        .{ .name = "SSL Settings" },
        .{ .name = "Cookie Settings" },
        .{ .name = "Charset Settings" },
    },

    param_map: std.StringHashMap(*c.param_ptr) = undefined,
    display_charset_str: [*c]c.wc_ces_list,

    fn init(allocator: std.mem.Allocator) @This() {
        return .{
            .allocator = allocator,
            .param_map = .init(allocator),
            .display_charset_str = c.wc_get_ces_list(),
        };
    }

    fn deinit(this: *@This()) void {
        this.param_map.deinit();
    }

    fn register(this: *@This(), section: c.SettingsSections, p: *c.param_ptr) void {
        var param = Param{
            .name = std.mem.span(p.name),
            .comment = std.mem.span(p.comment),
            .input_type = p.inputtype,
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
        if (p.select != null) {
            {
                var s: [*c]c.sel_c = p.select;
                var i: usize = 0;
                while (s.*.text != null) : (i += 1) {
                    s += 1;
                }
                param.select = this.allocator.alloc(SelectItem, i) catch @panic("OOM");
            }
            if (p.inputtype != c.PI_CODE) {
                var s: [*c]c.sel_c = p.select;
                var i: usize = 0;
                while (s.*.text != null) : (i += 1) {
                    param.select[i] = .{
                        .value = @intCast(s.*.value),
                        .cvalue = std.mem.span(s.*.cvalue),
                        .text = std.mem.span(s.*.text),
                    };
                    s += 1;
                }
            } else {
                var s: [*c]c.wc_ces_list = @ptrCast(p.select);
                var i: usize = 0;
                while (s.*.name != null) : (i += 1) {
                    param.select[i] = .{
                        .value = s.*.id,
                        .cvalue = std.mem.span(s.*.name),
                        .text = std.mem.span(s.*.desc),
                    };
                    s += 1;
                }
            }
        }
        this.sections[section].params.append(this.allocator, param) catch {};
        this.param_map.put(std.mem.span(p.name), p) catch {};
    }

    fn panel(this: @This()) c.Str {
        const optionpanel_src1 =
            \\<html><head><title>Option Setting Panel</title></head><body>
            \\<h1 align=center>Option Setting Panel<br>(w3m version %s)</b></h1>
            \\<form method=post action="file:///$LIB/w3mhelperpanel">
            \\<input type=hidden name=mode value=panel>
            \\<input type=hidden name=cookie value="%s">
            \\<input type=submit value="%s">
            \\</form><br>
            \\<form method=internal action=option>
        ;

        const optionpanel_str = c.Sprintf(
            optionpanel_src1,
            c.w3m_version,
            c.html_quote(c.localCookie().*.ptr),
            CMT_HELPER,
        );

        // g_runtime.OptionCharset = g_runtime.SystemCharset; /* FIXME */
        const src = c.Strdup(optionpanel_str);

        c.Strcat_charp(src, "<table><tr><td>");
        for (&g_opts.sections) |*section| {
            c.Strcat_m_charp(src, "<h1>", section.name.ptr, "</h1>", NULL);
            c.Strcat_charp(src, "<table width=100% cellpadding=0>");
            for (section.params.items) |*p| {
                c.Strcat_m_charp(src, "<tr><td>", p.comment.ptr, NULL);
                c.Strcat(src, c.Sprintf("</td><td width=%d>", 28 * c.getRuntime().*.pixel_per_char));
                switch (p.input_type) {
                    c.PI_TEXT => {
                        c.Strcat_m_charp(
                            src,
                            "<input type=text name=",
                            p.name.ptr,
                            " value=\"",
                            c.html_quote(p.to_str()),
                            "\">",
                            NULL,
                        );
                    },
                    c.PI_ONOFF => {
                        const x = std.fmt.parseInt(i32, std.mem.span(p.to_str()), 10) catch
                            @panic("parseInt");
                        c.Strcat_m_charp(
                            src,
                            "<input type=radio name=",
                            p.name.ptr,
                            " value=1",
                            @as([*c]const u8, if (x != 0) " checked" else ""),
                            ">YES&nbsp;&nbsp;<input type=radio name=",
                            p.name.ptr,
                            " value=0",
                            @as([*c]const u8, if (x != 0) "" else " checked"),
                            ">NO",
                            NULL,
                        );
                    },
                    c.PI_SEL_C => {
                        const tmp = p.to_str();
                        c.Strcat_m_charp(src, "<select name=", p.name.ptr, ">", NULL);
                        //                 for (struct sel_c* s = (struct sel_c*)p->select; s->text != NULL; s++) {
                        for (p.select) |s| {
                            c.Strcat_charp(src, "<option value=");
                            c.Strcat(src, c.Sprintf("%s\n", s.cvalue.ptr));
                            switch (p.getset) {
                                .P_CHAR => {
                                    if (s.value == tmp[0]) {
                                        c.Strcat_charp(src, " selected");
                                    }
                                },
                                else => {
                                    if (s.value == std.fmt.parseInt(u8, std.mem.span(tmp), 10) catch
                                        @panic("parseInt"))
                                    {
                                        c.Strcat_charp(src, " selected");
                                    }
                                },
                            }
                            _ = c.Strcat_char(src, '>');
                            _ = c.Strcat_charp(src, s.text.ptr);
                        }
                        c.Strcat_charp(src, "</select>");
                    },
                    c.PI_CODE => {
                        const tmp = p.to_str();
                        c.Strcat_m_charp(src, "<select name=", p.name.ptr, ">", NULL);
                        var s = this.display_charset_str;
                        while (s != null and s.*.name != null) : (s += 1) {
                            c.Strcat_charp(src, "<option value=");
                            c.Strcat(src, c.Sprintf("%s\n", s.*.name));
                            if (s.*.id == std.fmt.parseInt(u32, std.mem.span(tmp), 10) catch
                                @panic("parseInt"))
                            {
                                c.Strcat_charp(src, " selected");
                            }
                            _ = c.Strcat_char(src, '>');
                            _ = c.Strcat_charp(src, s.*.desc);
                        }
                        c.Strcat_charp(src, "</select>");
                    },
                    else => {},
                }
                c.Strcat_charp(src, "</td></tr>\n");
            }
            c.Strcat_charp(src, "<tr><td></td><td><p><input type=submit value=\"OK\"></td></tr>");
            c.Strcat_charp(src, "</table><hr width=50%>");
        }
        c.Strcat_charp(src, "</table></form></body></html>");
        return src;
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
    //     if (!g_runtime.OptionEncode) {
    //         optionpanel_str = wc_Str_conv(optionpanel_str, g_runtime.OptionCharset, g_runtime.InnerCharset);
    //         for (int i = 0; sections[i].name != NULL; i++) {
    //             sections[i].name = wc_conv(_(sections[i].name), g_runtime.OptionCharset, g_runtime.InnerCharset)->ptr;
    //             for (struct param_ptr* p = sections[i].params; p->name; p++) {
    //                 p->comment = wc_conv(_(p->comment), g_runtime.OptionCharset,
    //                     g_runtime.InnerCharset)
    //                                  ->ptr;
    //                 if (p->inputtype == PI_SEL_C
    //                     && p->select != colorstr) {
    //                     for (struct sel_c* s = (struct sel_c*)p->select; s->text != NULL; s++) {
    //                         s->text = wc_conv(_(s->text), g_runtime.OptionCharset,
    //                             g_runtime.InnerCharset)
    //                                       ->ptr;
    //                     }
    //                 }
    //             }
    //         }
    //
    //         for (struct sel_c* s = colorstr; s->text; s++)
    //             s->text = wc_conv(_(s->text), g_runtime.OptionCharset,
    //                 g_runtime.InnerCharset)
    //                           ->ptr;
    //
    //         g_runtime.OptionEncode = TRUE;
    //     }

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

export fn opt_load_panel() c.Str {
    return g_opts.panel();
}
