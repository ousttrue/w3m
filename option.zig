const std = @import("std");
const c = @import("c_include.zig").c;
const cmts = @cImport({
    @cInclude("option_cmt.h");
});

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

const SettingsSections = enum(u8) {
    SETTINGS_DISPLAY,
    SETTINGS_COLOR,
    SETTINGS_MISCELLANEOUS,
    SETTINGS_DIRECTORY,
    SETTINGS_EXTERNALPROGRAM,
    SETTINGS_NETWORK,
    SETTINGS_PROXY,
    SETTINGS_SSL,
    SETTINGS_COOKIE,
    SETTINGS_CHARSET,
};

const GetSetText = struct {
    ptr: *anyopaque,
};

const ParamInputTypes = enum {
    PI_TEXT,
    PI_ONOFF,
    PI_SEL_C,
    PI_CODE,
};

const ParamTypes = enum {
    P_INT,
    P_SHORT,
    P_CHARINT,
    P_CHAR,
    P_STRING,
    P_SSLPATH,
    P_COLOR,
    P_CODE,
    P_PIXELS,
    P_NZINT,
    P_SCALE,
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
    input_type: ParamInputTypes,
    getset: TypedGetSet,
    select: []const SelectItem,

    fn set(this: *@This(), value: []const u8) void {
        switch (this.getset) {
            .P_INT => |getset| {
                const ptr: *i32 = @ptrCast(@alignCast(getset.ptr));
                if (this.input_type == .PI_ONOFF) {
                    ptr.* = if (str_to_bool(value, ptr.* != 0)) 1 else 0;
                } else {
                    if (std.fmt.parseInt(i32, value, 10)) |n| {
                        ptr.* = n;
                    } else |_| {}
                }

                for (this.select) |s| {
                    if (std.mem.eql(u8, s.cvalue, value)) {
                        ptr.* = @intCast(s.value);
                        break;
                    }
                }
            },
            .P_NZINT => |getset| {
                const ptr: *i32 = @ptrCast(@alignCast(getset.ptr));
                if (std.fmt.parseInt(i32, value, 10)) |n| {
                    if (n > 0) {
                        ptr.* = n;
                    }
                } else |_| {}
            },
            .P_SHORT => |getset| {
                const ptr: *i16 = @ptrCast(@alignCast(getset.ptr));
                if (this.input_type == .PI_ONOFF) {
                    ptr.* = if (str_to_bool(value, ptr.* != 0)) 1 else 0;
                } else {
                    if (std.fmt.parseInt(i16, value, 10)) |n| {
                        ptr.* = n;
                    } else |_| {}
                }
            },
            .P_CHARINT => |getset| {
                const ptr: *i8 = @ptrCast(@alignCast(getset.ptr));
                if (this.input_type == .PI_ONOFF) {
                    ptr.* = if (str_to_bool(value, ptr.* != 0)) 1 else 0;
                } else {
                    if (std.fmt.parseInt(i8, value, 10)) |n| {
                        ptr.* = n;
                    } else |_| {}
                }
            },
            .P_CHAR => |getset| {
                const ptr: *u8 = @ptrCast(@alignCast(getset.ptr));
                ptr.* = value[0];
            },
            .P_STRING => |getset| {
                const ptr: *[*c]const u8 = @ptrCast(@alignCast(getset.ptr));
                ptr.* = value.ptr;
            },
            .P_SSLPATH => |getset| {
                const ptr: *[*c]const u8 = @ptrCast(@alignCast(getset.ptr));
                if (value.len > 0) {
                    ptr.* = c.rcFile(value.ptr);
                } else {
                    ptr.* = null;
                }
                c.getRuntime().*.ssl_path_modified = 1;
            },
            .P_COLOR => |getset| {
                const ptr: *u8 = @ptrCast(@alignCast(getset.ptr));
                ptr.* = str_to_color(value);
            },
            .P_CODE => |getset| {
                const ptr: *u32 = @ptrCast(@alignCast(getset.ptr));
                ptr.* = c.wc_guess_charset_short(value.ptr, ptr.*);
            },
            .P_PIXELS => |getset| {
                const ptr: *f64 = @ptrCast(@alignCast(getset.ptr));
                if (std.fmt.parseFloat(f64, value)) |ppc| {
                    if (ppc >= c.MINIMUM_PIXEL_PER_CHAR and ppc <= c.MAXIMUM_PIXEL_PER_CHAR * 2) {
                        ptr.* = ppc;
                    }
                } else |_| {}
            },
            .P_SCALE => |getset| {
                const ptr: *f64 = @ptrCast(@alignCast(getset.ptr));
                if (std.fmt.parseFloat(f64, value)) |ppc| {
                    if (ppc >= 10 and ppc <= 1000) {
                        ptr.* = ppc;
                    }
                } else |_| {}
            },
        }
    }

    fn str_to_bool(value: []const u8, old: bool) bool {
        if (value.len == 0)
            return true;

        return switch (std.ascii.toLower(value[0])) {
            '0',
            'f', // false
            'n', // no
            'u',
            => // undef
            false,

            'o' => if (std.ascii.toLower(value[1]) == 'f')
                // off
                false
            else
                // on
                true,

            't' => if (std.ascii.toLower(value[1]) == 'o')
                // toggle
                !old
            else
                // true
                true,

            '!',
            'r', // reverse
            'x',
            => // exchange
            !old,
            else => true,
        };
    }

    // str to ansi color code
    fn str_to_color(value: []const u8) u8 {
        return switch (std.ascii.toLower(value[0])) {
            '0' => 0, // black
            '1', 'r' => 1, // red
            '2', 'g' => 2, // green
            '3', 'y' => 3, // yellow
            '4' => 4, // blue
            '5', 'm' => 5, // magenta
            '6', 'c' => 6, // cyan
            '7', 'w' => 7, // white
            '8', 't' => 8, // terminal
            'b' => if (std.ascii.startsWithIgnoreCase(value, "blu"))
                4 // blue
            else
                0, // black
            else => 8, // terminal
        };
    }

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

const SectionRef = struct {
    section: SettingsSections,
    index: usize,
};

const Option = struct {
    allocator: std.mem.Allocator,
    sections: [@typeInfo(SettingsSections).@"enum".fields.len]Section = .{
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

    param_map_zig: std.StringHashMap(SectionRef) = undefined,
    display_charset_str: [*c]c.wc_ces_list,

    fn init(allocator: std.mem.Allocator) @This() {
        return .{
            .allocator = allocator,
            .param_map_zig = .init(allocator),
            .display_charset_str = c.wc_get_ces_list(),
        };
    }

    fn deinit(this: *@This()) void {
        this.param_map_zig.deinit();
    }

    fn register(
        this: *@This(),
        section: SettingsSections,
        name: [:0]const u8,
        comment: [:0]const u8,
        ptr: *anyopaque,
        param_type: ParamTypes,
        input_type: ParamInputTypes,
        select: []const SelectItem,
    ) void {
        var param = Param{
            .name = name,
            .comment = comment,
            .input_type = input_type,
            .getset = undefined,
            .select = select,
        };
        switch (param_type) {
            .P_INT => {
                param.getset = .{ .P_INT = .{ .ptr = ptr } };
            },
            .P_SHORT => {
                param.getset = .{ .P_SHORT = .{ .ptr = ptr } };
            },
            .P_CHARINT => {
                param.getset = .{ .P_CHARINT = .{ .ptr = ptr } };
            },
            .P_CHAR => {
                param.getset = .{ .P_CHAR = .{ .ptr = ptr } };
            },
            .P_STRING => {
                param.getset = .{ .P_STRING = .{ .ptr = ptr } };
            },
            .P_SSLPATH => {
                param.getset = .{ .P_SSLPATH = .{ .ptr = ptr } };
            },
            .P_COLOR => {
                param.getset = .{ .P_COLOR = .{ .ptr = ptr } };
            },
            .P_CODE => {
                param.getset = .{ .P_CODE = .{ .ptr = ptr } };
            },
            .P_PIXELS => {
                param.getset = .{ .P_PIXELS = .{ .ptr = ptr } };
            },
            .P_NZINT => {
                param.getset = .{ .P_NZINT = .{ .ptr = ptr } };
            },
            .P_SCALE => {
                param.getset = .{ .P_SCALE = .{ .ptr = ptr } };
            },
        }
        this.param_map_zig.put(param.name, .{
            .section = section,
            .index = this.sections[@intFromEnum(section)].params.items.len,
        }) catch {};
        this.sections[@intFromEnum(section)].params.append(this.allocator, param) catch {};
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
                    .PI_TEXT => {
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
                    .PI_ONOFF => {
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
                    .PI_SEL_C => {
                        const tmp = p.to_str();
                        c.Strcat_m_charp(src, "<select name=", p.name.ptr, ">", NULL);
                        //                 for (struct SelectItem* s = (struct SelectItem*)p->select; s->text != NULL; s++) {
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
                    .PI_CODE => {
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
                }
                c.Strcat_charp(src, "</td></tr>\n");
            }
            c.Strcat_charp(src, "<tr><td></td><td><p><input type=submit value=\"OK\"></td></tr>");
            c.Strcat_charp(src, "</table><hr width=50%>");
        }
        c.Strcat_charp(src, "</table></form></body></html>");
        return src;
    }

    fn get_param(this: *@This(), name: []const u8) ?*Param {
        if (this.param_map_zig.get(name)) |section_index| {
            return &this.sections[@intFromEnum(section_index.section)].params.items[section_index.index];
        } else {
            return null;
        }
    }
};

var g_opts: Option = undefined;

pub fn opt_init_alloc(allocator: std.mem.Allocator) void {
    g_opts = .init(allocator);
}
pub fn opt_deinit() void {
    g_opts.deinit();
}

export fn opt_load_panel() c.Str {
    return g_opts.panel();
}

export fn opt_get_param_option(name: [*c]const u8) [*c]const u8 {
    if (g_opts.param_map_zig.get(std.mem.span(name))) |section_index| {
        const p = &g_opts.sections[@intFromEnum(section_index.section)].params.items[section_index.index];
        return p.to_str();
    } else {
        return null;
    }
}

fn opt_set_param(name: []const u8, value: []const u8) bool {
    if (std.mem.eql(u8, name, "inline_img_protocol")) {
        // DEBUG
        const a = 0;
        _ = a;
    }

    if (g_opts.get_param(name)) |p| {
        p.set(value);
        return true;
    }
    return false;
}

export fn opt_set_param_option(option: [*c]const u8) bool {
    const tmp = c.Strnew();
    var p: [*:0]const u8 = option;
    // , *q;

    while (p[0] != 0 and !c.IS_SPACE(p[0]) and p[0] != '=') : (p += 1) {
        _ = c.Strcat_char(tmp, p[0]);
    }
    while (p[0] != 0 and c.IS_SPACE(p[0])) : (p += 1) {}
    if (p[0] == '=') {
        p += 1;
        while (p[0] != 0 and c.IS_SPACE(p[0])) {
            p += 1;
        }
    }
    c.Strlower(tmp);
    if (opt_set_param(std.mem.span(tmp.*.ptr), std.mem.span(p))) {
        // goto option_assigned;
    } else {
        var q = tmp.*.ptr;
        if (std.mem.startsWith(u8, std.mem.span(q), "no")) { // -o noxxx, -o no-xxx, -o no_xxx
            q += 2;
            if (q.* == '-' or q.* == '_')
                q += 1;
        } else if (tmp.*.ptr[0] == '-') { // -o -xxx
            q += 1;
        } else {
            return false;
        }

        if (opt_set_param(std.mem.span(q), "0")) {
            // goto option_assigned;
        } else {
            return false;
        }
    }
    return true;
}

/// show parameter with bad options invokation
export fn show_params(_fp: ?*c.FILE) void {
    const fp = _fp orelse {
        return;
    };

    const g_runtime: *c.Runtime = c.getRuntime().?;
    g_runtime.OptionCharset = g_runtime.SystemCharset; // FIXME

    _ = c.fputs("\nconfiguration parameters\n", fp);
    for (g_opts.sections, 0..) |section, j| {
        {
            const cmt = if (0 == g_runtime.OptionEncode)
                c.wc_conv(section.name.ptr, g_runtime.OptionCharset, g_runtime.InnerCharset).*.ptr
            else
                section.name.ptr;
            _ = c.fprintf(fp, "  section[%d]: %s\n", j, c.conv_to_system(cmt));
        }

        for (section.params.items) |*p| {
            const t: []const u8 = switch (p.getset) {
                .P_INT, .P_SHORT, .P_CHARINT, .P_NZINT => if (p.input_type == .PI_ONOFF)
                    "bool"
                else
                    "number",
                .P_CHAR => "char",
                .P_STRING => "string",
                .P_SSLPATH => "path",
                .P_COLOR => "color",
                .P_CODE => "charset",
                .P_PIXELS => "number",
                .P_SCALE => "percent",
            };

            const cmt = if (0 == g_runtime.OptionEncode)
                c.wc_conv(p.comment.ptr, g_runtime.OptionCharset, g_runtime.InnerCharset).*.ptr
            else
                p.comment.ptr;

            var l: i32 = 30 - @as(i32, @intCast(p.name.len)) + @as(i32, @intCast(t.len));
            if (l < 0)
                l = 1;
            _ = c.fprintf(
                fp,
                "    -o %s=<%s>%*s%s\n",
                p.name.ptr,
                t.ptr,
                l,
                " ",
                c.conv_to_system(cmt),
            );
        }
    }
}

fn opt_register(
    section: SettingsSections,
    // _p: [*c]c.param_ptr,
    name: [*c]const u8,
    comment: [*c]const u8,
    ptr: *anyopaque,
    param_type: ParamTypes,
    input_type: ParamInputTypes,
    select: []SelectItem,
) void {
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
    //                     for (struct SelectItem* s = (struct SelectItem*)p->select; s->text != NULL; s++) {
    //                         s->text = wc_conv(_(s->text), g_runtime.OptionCharset,
    //                             g_runtime.InnerCharset)
    //                                       ->ptr;
    //                     }
    //                 }
    //             }
    //         }
    //
    //         for (struct SelectItem* s = colorstr; s->text; s++)
    //             s->text = wc_conv(_(s->text), g_runtime.OptionCharset,
    //                 g_runtime.InnerCharset)
    //                           ->ptr;
    //
    //         g_runtime.OptionEncode = TRUE;
    //     }

    // const p: *c.param_ptr = _p orelse {
    //     return;
    // };
    g_opts.register(
        section,
        std.mem.span(name),
        std.mem.span(comment),
        ptr,
        param_type,
        input_type,
        select,
    );
}

const colorstr = [_]SelectItem{
    .{ .value = 0, .cvalue = "black", .text = "black" },
    .{ .value = 1, .cvalue = "red", .text = "red" },
    .{ .value = 2, .cvalue = "green", .text = "green" },
    .{ .value = 3, .cvalue = "yellow", .text = "yellow" },
    .{ .value = 4, .cvalue = "blue", .text = "blue" },
    .{ .value = 5, .cvalue = "magenta", .text = "magenta" },
    .{ .value = 6, .cvalue = "cyan", .text = "cyan" },
    .{ .value = 7, .cvalue = "white", .text = "white" },
    .{ .value = 8, .cvalue = "terminal", .text = "terminal" },
};

const defaulturls = [_]SelectItem{
    .{ .value = c.DEFAULT_URL_EMPTY, .cvalue = "DEFAULT_URL_EMPTY", .text = "none" },
    .{ .value = c.DEFAULT_URL_CURRENT, .cvalue = "DEFAULT_URL_CURRENT", .text = "current URL" },
    .{ .value = c.DEFAULT_URL_LINK, .cvalue = "DEFAULT_URL_LINK", .text = "link URL" },
};

const displayinsdel = [_]SelectItem{
    .{ .value = c.DISPLAY_INS_DEL_SIMPLE, .cvalue = "DISPLAY_INS_DEL_SIMPLE", .text = "simple" },
    .{ .value = c.DISPLAY_INS_DEL_NORMAL, .cvalue = "DISPLAY_INS_DEL_NORMAL", .text = "use tag" },
    .{ .value = c.DISPLAY_INS_DEL_FONTIFY, .cvalue = "DISPLAY_INS_DEL_FONTIFY", .text = "fontify" },
};

const dnsorders = [_]SelectItem{
    .{ .value = c.DNS_ORDER_UNSPEC, .cvalue = "DNS_ORDER_UNSPEC", .text = "unspecified" },
    .{ .value = c.DNS_ORDER_INET_INET6, .cvalue = "DNS_ORDER_INET_INET6", .text = "inet inet6" },
    .{ .value = c.DNS_ORDER_INET6_INET, .cvalue = "DNS_ORDER_INET6_INET", .text = "inet6 inet" },
    .{ .value = c.DNS_ORDER_INET_ONLY, .cvalue = "DNS_ORDER_INET_ONLY", .text = "inet only" },
    .{ .value = c.DNS_ORDER_INET6_ONLY, .cvalue = "DNS_ORDER_INET6_ONLY", .text = "inet6 only" },
};

const badcookiestr = [_]SelectItem{
    .{ .value = c.ACCEPT_BAD_COOKIE_DISCARD, .cvalue = "ACCEPT_BAD_COOKIE_DISCARD", .text = "discard" },
    .{ .value = c.ACCEPT_BAD_COOKIE_ASK, .cvalue = "ACCEPT_BAD_COOKIE_ASK", .text = "ask" },
};

const mailtooptionsstr = [_]SelectItem{
    .{ .value = c.MAILTO_OPTIONS_IGNORE, .cvalue = "MAILTO_OPTIONS_IGNORE", .text = "ignore options and use only the address" },
    .{ .value = c.MAILTO_OPTIONS_USE_MAILTO_URL, .cvalue = "MAILTO_OPTIONS_USE_MAILTO_URL", .text = "use full mailto URL" },
};

const auto_detect_str = [_]SelectItem{
    .{ .value = c.WC_OPT_DETECT_OFF, .cvalue = "WC_OPT_DETECT_OFF", .text = "OFF" },
    .{ .value = c.WC_OPT_DETECT_ISO_2022, .cvalue = "WC_OPT_DETECT_ISO_2022", .text = "Only ISO 2022" },
    .{ .value = c.WC_OPT_DETECT_ON, .cvalue = "WC_OPT_DETECT_ON", .text = "ON" },
};

const graphic_char_str = [_]SelectItem{
    .{ .value = c.GRAPHIC_CHAR_ASCII, .cvalue = "GRAPHIC_CHAR_ASCII", .text = "ASCII" },
    .{ .value = c.GRAPHIC_CHAR_CHARSET, .cvalue = "GRAPHIC_CHAR_CHARSET", .text = "charset specific" },
    .{ .value = c.GRAPHIC_CHAR_DEC, .cvalue = "GRAPHIC_CHAR_DEC", .text = "DEC special graphics" },
};

const inlineimgstr = [_]SelectItem{
    .{ .value = c.INLINE_IMG_NONE, .cvalue = "INLINE_IMG_NONE", .text = "external command" },
    .{ .value = c.INLINE_IMG_OSC5379, .cvalue = "INLINE_IMG_OSC5379", .text = "OSC 5379 (mlterm)" },
    .{ .value = c.INLINE_IMG_SIXEL, .cvalue = "INLINE_IMG_SIXEL", .text = "sixel (img2sixel)" },
    .{ .value = c.INLINE_IMG_ITERM2, .cvalue = "INLINE_IMG_ITERM2", .text = "OSC 1337 (iTerm2)" },
    .{ .value = c.INLINE_IMG_KITTY, .cvalue = "INLINE_IMG_KITTY", .text = "kitty (ImageMagick)" },
};

/// parse key value line
///
/// example:
/// ```
/// # tab stop space
/// tabstop 8
/// ```
export fn opt_load(handle: std.fs.File.Handle) void {
    const file: std.fs.File = .{
        .handle = handle,
    };
    var buf: [128]u8 = undefined;
    var r = file.reader(&buf);
    while (r.interface.takeDelimiter('\n') catch null) |_line| {
        const line = std.mem.trim(u8, _line, &std.ascii.whitespace);
        if (line.len == 0)
            continue;
        if (line[0] == '#')
            // comment
            continue;

        if (std.mem.indexOfAny(u8, line, &.{ ' ', '\t' })) |b| {
            const key = line[0..b];
            var e = b + 1;
            while (e < line.len and c.IS_SPACE(line[e])) {
                e += 1;
            }
            _ = opt_set_param(key, line[e..]);
        }
    }
}

export fn panel_set_option(buf: ?*c.Buffer, _arg: [*c]c.parsed_tagarg) void {
    _ = buf;
    var s: c.Str = c.Strnew();
    var arg: ?*c.parsed_tagarg = _arg;
    while (arg) |a| : (arg = a.next orelse null) {
        if (a.value) |value| {
            const p = c.conv_to_system(value);
            if (opt_set_param(std.mem.span(a.arg), std.mem.span(p))) {
                const tmp = c.Sprintf("%s %s\n", a.arg, p);
                _ = c.Strcat(tmp, s);
                s = tmp;
            }
        }
    }

    if (c.getRuntime().*.config_file) |config_file| {
        if (std.fs.cwd().createFile(std.mem.span(config_file), .{})) |f| {
            _ = f.write(s.*.ptr[0..s.*.length]) catch {};
            f.close();
        } else |_| {
            c.disp_message("Can't write option!", false);
        }
    } else {
        c.disp_message("There's no config file... config not saved", false);
    }

    c.sync_with_option();
    c.tab_back(c.CurrentTab());
}

pub fn opt_init() void {
    const g_runtime: *c.Runtime = c.getRuntime().?;
    g_opts.register(.SETTINGS_DISPLAY, "tabstop", cmts.CMT_TABSTOP, &g_runtime.Tabstop, .P_NZINT, .PI_TEXT, &.{});
    g_opts.register(.SETTINGS_DISPLAY, "indent_incr", cmts.CMT_INDENT_INCR, &g_runtime.IndentIncr, .P_NZINT, .PI_TEXT, &.{});
    g_opts.register(.SETTINGS_DISPLAY, "pixel_per_char", cmts.CMT_PIXEL_PER_CHAR, &g_runtime.pixel_per_char, .P_PIXELS, .PI_TEXT, &.{});
    g_opts.register(.SETTINGS_DISPLAY, "pixel_per_line", cmts.CMT_PIXEL_PER_LINE, &g_runtime.pixel_per_line, .P_PIXELS, .PI_TEXT, &.{});
    g_opts.register(.SETTINGS_DISPLAY, "frame", cmts.CMT_FRAME, &g_runtime.RenderFrame, .P_CHARINT, .PI_ONOFF, &.{});
    g_opts.register(.SETTINGS_DISPLAY, "target_self", cmts.CMT_TSELF, &g_runtime.TargetSelf, .P_CHARINT, .PI_ONOFF, &.{});
    g_opts.register(.SETTINGS_DISPLAY, "open_tab_blank", cmts.CMT_OPEN_TAB_BLANK, &g_runtime.open_tab_blank, .P_INT, .PI_ONOFF, &.{});
    g_opts.register(.SETTINGS_DISPLAY, "open_tab_dl_list", cmts.CMT_OPEN_TAB_DL_LIST, &g_runtime.open_tab_dl_list, .P_INT, .PI_ONOFF, &.{});
    g_opts.register(.SETTINGS_DISPLAY, "display_link", cmts.CMT_DISPLINK, &g_runtime.displayLink, .P_INT, .PI_ONOFF, &.{});
    g_opts.register(.SETTINGS_DISPLAY, "display_link_number", cmts.CMT_DISPLINKNUMBER, &g_runtime.displayLinkNumber, .P_INT, .PI_ONOFF, &.{});
    g_opts.register(.SETTINGS_DISPLAY, "decode_url", cmts.CMT_DECODE_URL, &g_runtime.DecodeURL, .P_INT, .PI_ONOFF, &.{});
    g_opts.register(.SETTINGS_DISPLAY, "display_lineinfo", cmts.CMT_DISPLINEINFO, &g_runtime.displayLineInfo, .P_INT, .PI_ONOFF, &.{});
    g_opts.register(.SETTINGS_DISPLAY, "ext_dirlist", cmts.CMT_EXT_DIRLIST, &g_runtime.UseExternalDirBuffer, .P_INT, .PI_ONOFF, &.{});
    g_opts.register(.SETTINGS_DISPLAY, "dirlist_cmd", cmts.CMT_DIRLIST_CMD, @ptrCast(&g_runtime.DirBufferCommand), .P_STRING, .PI_TEXT, &.{});
    g_opts.register(.SETTINGS_DISPLAY, "use_dictcommand", cmts.CMT_USE_DICTCOMMAND, &g_runtime.UseDictCommand, .P_INT, .PI_ONOFF, &.{});
    g_opts.register(.SETTINGS_DISPLAY, "dictcommand", cmts.CMT_DICTCOMMAND, @ptrCast(&g_runtime.DictCommand), .P_STRING, .PI_TEXT, &.{});
    g_opts.register(.SETTINGS_DISPLAY, "multicol", cmts.CMT_MULTICOL, &g_runtime.multicolList, .P_INT, .PI_ONOFF, &.{});
    g_opts.register(.SETTINGS_DISPLAY, "alt_entity", cmts.CMT_ALT_ENTITY, &g_runtime.UseAltEntity, .P_CHARINT, .PI_ONOFF, &.{});
    g_opts.register(.SETTINGS_DISPLAY, "graphic_char", cmts.CMT_GRAPHIC_CHAR, &g_runtime.UseGraphicChar, .P_CHARINT, .PI_SEL_C, &graphic_char_str);
    g_opts.register(.SETTINGS_DISPLAY, "display_borders", cmts.CMT_DISP_BORDERS, &g_runtime.DisplayBorders, .P_CHARINT, .PI_ONOFF, &.{});
    g_opts.register(.SETTINGS_DISPLAY, "disable_center", cmts.CMT_DISABLE_CENTER, &g_runtime.DisableCenter, .P_CHARINT, .PI_ONOFF, &.{});
    g_opts.register(.SETTINGS_DISPLAY, "fold_textarea", cmts.CMT_FOLD_TEXTAREA, &g_runtime.FoldTextarea, .P_CHARINT, .PI_ONOFF, &.{});
    g_opts.register(.SETTINGS_DISPLAY, "display_ins_del", cmts.CMT_DISP_INS_DEL, &g_runtime.displayInsDel, .P_INT, .PI_SEL_C, &displayinsdel);
    g_opts.register(.SETTINGS_DISPLAY, "ignore_null_img_alt", cmts.CMT_IGNORE_NULL_IMG_ALT, &g_runtime.ignore_null_img_alt, .P_INT, .PI_ONOFF, &.{});
    g_opts.register(.SETTINGS_DISPLAY, "view_unseenobject", cmts.CMT_VIEW_UNSEENOBJECTS, &g_runtime.view_unseenobject, .P_INT, .PI_ONOFF, &.{});
    g_opts.register(.SETTINGS_DISPLAY, "display_image", cmts.CMT_DISP_IMAGE, &g_runtime.displayImage, .P_INT, .PI_ONOFF, &.{});
    g_opts.register(.SETTINGS_DISPLAY, "pseudo_inlines", cmts.CMT_PSEUDO_INLINES, &g_runtime.pseudoInlines, .P_INT, .PI_ONOFF, &.{});
    g_opts.register(.SETTINGS_DISPLAY, "auto_image", cmts.CMT_AUTO_IMAGE, &g_runtime.autoImage, .P_INT, .PI_ONOFF, &.{});
    g_opts.register(.SETTINGS_DISPLAY, "max_load_image", cmts.CMT_MAX_LOAD_IMAGE, &g_runtime.maxLoadImage, .P_INT, .PI_TEXT, &.{});
    g_opts.register(.SETTINGS_DISPLAY, "ext_image_viewer", cmts.CMT_EXT_IMAGE_VIEWER, &g_runtime.useExtImageViewer, .P_INT, .PI_ONOFF, &.{});
    g_opts.register(.SETTINGS_DISPLAY, "image_scale", cmts.CMT_IMAGE_SCALE, &g_runtime.image_scale, .P_SCALE, .PI_TEXT, &.{});
    g_opts.register(.SETTINGS_DISPLAY, "inline_img_protocol", cmts.CMT_INLINE_IMG_PROTOCOL, &g_runtime.enable_inline_image, .P_INT, .PI_SEL_C, &inlineimgstr);
    g_opts.register(.SETTINGS_DISPLAY, "imgdisplay", cmts.CMT_IMGDISPLAY, @ptrCast(&g_runtime.Imgdisplay), .P_STRING, .PI_TEXT, &.{});
    g_opts.register(.SETTINGS_DISPLAY, "image_map_list", cmts.CMT_IMAGE_MAP_LIST, &g_runtime.image_map_list, .P_INT, .PI_ONOFF, &.{});
    g_opts.register(.SETTINGS_DISPLAY, "fold_line", cmts.CMT_FOLD_LINE, &g_runtime.FoldLine, .P_INT, .PI_ONOFF, &.{});
    g_opts.register(.SETTINGS_DISPLAY, "show_lnum", cmts.CMT_SHOW_NUM, &g_runtime.showLineNum, .P_INT, .PI_ONOFF, &.{});
    g_opts.register(.SETTINGS_DISPLAY, "show_srch_str", cmts.CMT_SHOW_SRCH_STR, &g_runtime.show_srch_str, .P_INT, .PI_ONOFF, &.{});
    g_opts.register(.SETTINGS_DISPLAY, "label_topline", cmts.CMT_LABEL_TOPLINE, &g_runtime.label_topline, .P_INT, .PI_ONOFF, &.{});
    g_opts.register(.SETTINGS_DISPLAY, "nextpage_topline", cmts.CMT_NEXTPAGE_TOPLINE, &g_runtime.nextpage_topline, .P_INT, .PI_ONOFF, &.{});

    g_opts.register(.SETTINGS_COLOR, "color", cmts.CMT_COLOR, &g_runtime.useColor, .P_INT, .PI_ONOFF, &.{});
    g_opts.register(.SETTINGS_COLOR, "high-intensity", cmts.CMT_HINTENSITY_COLOR, &g_runtime.highIntensityColors, .P_INT, .PI_ONOFF, &.{});
    g_opts.register(.SETTINGS_COLOR, "basic_color", cmts.CMT_B_COLOR, &g_runtime.basic_color, .P_COLOR, .PI_SEL_C, &colorstr);
    g_opts.register(.SETTINGS_COLOR, "anchor_color", cmts.CMT_A_COLOR, &g_runtime.anchor_color, .P_COLOR, .PI_SEL_C, &colorstr);
    g_opts.register(.SETTINGS_COLOR, "image_color", cmts.CMT_I_COLOR, &g_runtime.image_color, .P_COLOR, .PI_SEL_C, &colorstr);
    g_opts.register(.SETTINGS_COLOR, "form_color", cmts.CMT_F_COLOR, &g_runtime.form_color, .P_COLOR, .PI_SEL_C, &colorstr);
    g_opts.register(.SETTINGS_COLOR, "mark_color", cmts.CMT_MARK_COLOR, &g_runtime.mark_color, .P_COLOR, .PI_SEL_C, &colorstr);
    g_opts.register(.SETTINGS_COLOR, "bg_color", cmts.CMT_BG_COLOR, &g_runtime.bg_color, .P_COLOR, .PI_SEL_C, &colorstr);
    g_opts.register(.SETTINGS_COLOR, "active_style", cmts.CMT_ACTIVE_STYLE, &g_runtime.useActiveColor, .P_INT, .PI_ONOFF, &.{});
    g_opts.register(.SETTINGS_COLOR, "active_color", cmts.CMT_C_COLOR, &g_runtime.active_color, .P_COLOR, .PI_SEL_C, &colorstr);
    g_opts.register(.SETTINGS_COLOR, "visited_anchor", cmts.CMT_VISITED_ANCHOR, &g_runtime.useVisitedColor, .P_INT, .PI_ONOFF, &.{});
    g_opts.register(.SETTINGS_COLOR, "visited_color", cmts.CMT_V_COLOR, &g_runtime.visited_color, .P_COLOR, .PI_SEL_C, &colorstr);

    g_opts.register(.SETTINGS_DIRECTORY, "document_root", cmts.CMT_DROOT, @ptrCast(&g_runtime.document_root), .P_STRING, .PI_TEXT, &.{});
    g_opts.register(.SETTINGS_DIRECTORY, "personal_document_root", cmts.CMT_PDROOT, @ptrCast(&g_runtime.personal_document_root), .P_STRING, .PI_TEXT, &.{});
    g_opts.register(.SETTINGS_DIRECTORY, "cgi_bin", cmts.CMT_CGIBIN, @ptrCast(&g_runtime.cgi_bin), .P_STRING, .PI_TEXT, &.{});
    g_opts.register(.SETTINGS_DIRECTORY, "index_file", cmts.CMT_IFILE, @ptrCast(&g_runtime.index_file), .P_STRING, .PI_TEXT, &.{});
    g_opts.register(.SETTINGS_DIRECTORY, "tmp_dir", cmts.CMT_TMP, @ptrCast(&g_runtime.param_tmp_dir), .P_STRING, .PI_TEXT, &.{});

    g_opts.register(.SETTINGS_MISCELLANEOUS, "pagerline", cmts.CMT_PAGERLINE, &g_runtime.PagerMax, .P_NZINT, .PI_TEXT, &.{});
    g_opts.register(.SETTINGS_MISCELLANEOUS, "use_history", cmts.CMT_HISTORY, &g_runtime.UseHistory, .P_INT, .PI_ONOFF, &.{});
    g_opts.register(.SETTINGS_MISCELLANEOUS, "history", cmts.CMT_HISTSIZE, &g_runtime.URLHistSize, .P_INT, .PI_TEXT, &.{});
    g_opts.register(.SETTINGS_MISCELLANEOUS, "save_hist", cmts.CMT_SAVEHIST, &g_runtime.SaveURLHist, .P_INT, .PI_ONOFF, &.{});
    g_opts.register(.SETTINGS_MISCELLANEOUS, "confirm_qq", cmts.CMT_CONFIRM_QQ, &g_runtime.confirm_on_quit, .P_INT, .PI_ONOFF, &.{});
    g_opts.register(.SETTINGS_MISCELLANEOUS, "close_tab_back", cmts.CMT_CLOSE_TAB_BACK, &g_runtime.close_tab_back, .P_INT, .PI_ONOFF, &.{});
    g_opts.register(.SETTINGS_MISCELLANEOUS, "mark", cmts.CMT_USE_MARK, &g_runtime.use_mark, .P_INT, .PI_ONOFF, &.{});
    g_opts.register(.SETTINGS_MISCELLANEOUS, "emacs_like_lineedit", cmts.CMT_EMACS_LIKE_LINEEDIT, &g_runtime.emacs_like_lineedit, .P_INT, .PI_ONOFF, &.{});
    g_opts.register(.SETTINGS_MISCELLANEOUS, "space_autocomplete", cmts.CMT_SPACE_AUTOCOMPLETE, &g_runtime.space_autocomplete, .P_INT, .PI_ONOFF, &.{});
    g_opts.register(.SETTINGS_MISCELLANEOUS, "vi_prec_num", cmts.CMT_VI_PREC_NUM, &g_runtime.vi_prec_num, .P_INT, .PI_ONOFF, &.{});
    g_opts.register(.SETTINGS_MISCELLANEOUS, "mark_all_pages", cmts.CMT_MARK_ALL_PAGES, &g_runtime.MarkAllPages, .P_INT, .PI_ONOFF, &.{});
    g_opts.register(.SETTINGS_MISCELLANEOUS, "wrap_search", cmts.CMT_WRAP, &g_runtime.WrapDefault, .P_INT, .PI_ONOFF, &.{});
    g_opts.register(.SETTINGS_MISCELLANEOUS, "ignorecase_search", cmts.CMT_IGNORE_CASE, &g_runtime.IgnoreCase, .P_INT, .PI_ONOFF, &.{});
    g_opts.register(.SETTINGS_MISCELLANEOUS, "clear_buffer", cmts.CMT_CLEAR_BUF, &g_runtime.clear_buffer, .P_INT, .PI_ONOFF, &.{});
    g_opts.register(.SETTINGS_MISCELLANEOUS, "auto_uncompress", cmts.CMT_AUTO_UNCOMPRESS, &g_runtime.AutoUncompress, .P_CHARINT, .PI_ONOFF, &.{});
    g_opts.register(.SETTINGS_MISCELLANEOUS, "preserve_timestamp", cmts.CMT_PRESERVE_TIMESTAMP, &g_runtime.PreserveTimestamp, .P_CHARINT, .PI_ONOFF, &.{});
    g_opts.register(.SETTINGS_MISCELLANEOUS, "keymap_file", cmts.CMT_KEYMAP_FILE, @ptrCast(&g_runtime.keymap_file), .P_STRING, .PI_TEXT, &.{});

    g_opts.register(.SETTINGS_EXTERNALPROGRAM, "mime_types", cmts.CMT_MIMETYPES, @ptrCast(&g_runtime.mimetypes_files), .P_STRING, .PI_TEXT, &.{});
    g_opts.register(.SETTINGS_EXTERNALPROGRAM, "mailcap", cmts.CMT_MAILCAP, @ptrCast(&g_runtime.mailcap_files), .P_STRING, .PI_TEXT, &.{});
    g_opts.register(.SETTINGS_EXTERNALPROGRAM, "urimethodmap", cmts.CMT_URIMETHODMAP, @ptrCast(&g_runtime.urimethodmap_files), .P_STRING, .PI_TEXT, &.{});
    g_opts.register(.SETTINGS_EXTERNALPROGRAM, "editor", cmts.CMT_EDITOR, @ptrCast(&g_runtime.Editor), .P_STRING, .PI_TEXT, &.{});
    g_opts.register(.SETTINGS_EXTERNALPROGRAM, "mailto_options", cmts.CMT_MAILTO_OPTIONS, &g_runtime.MailtoOptions, .P_INT, .PI_SEL_C, &mailtooptionsstr);
    g_opts.register(.SETTINGS_EXTERNALPROGRAM, "mailer", cmts.CMT_MAILER, @ptrCast(&g_runtime.Mailer), .P_STRING, .PI_TEXT, &.{});
    g_opts.register(.SETTINGS_EXTERNALPROGRAM, "extbrowser", cmts.CMT_EXTBRZ, @ptrCast(&g_runtime.ExtBrowser), .P_STRING, .PI_TEXT, &.{});
    g_opts.register(.SETTINGS_EXTERNALPROGRAM, "extbrowser2", cmts.CMT_EXTBRZ2, @ptrCast(&g_runtime.ExtBrowser2), .P_STRING, .PI_TEXT, &.{});
    g_opts.register(.SETTINGS_EXTERNALPROGRAM, "extbrowser3", cmts.CMT_EXTBRZ3, @ptrCast(&g_runtime.ExtBrowser3), .P_STRING, .PI_TEXT, &.{});
    g_opts.register(.SETTINGS_EXTERNALPROGRAM, "extbrowser4", cmts.CMT_EXTBRZ4, @ptrCast(&g_runtime.ExtBrowser4), .P_STRING, .PI_TEXT, &.{});
    g_opts.register(.SETTINGS_EXTERNALPROGRAM, "extbrowser5", cmts.CMT_EXTBRZ5, @ptrCast(&g_runtime.ExtBrowser5), .P_STRING, .PI_TEXT, &.{});
    g_opts.register(.SETTINGS_EXTERNALPROGRAM, "extbrowser6", cmts.CMT_EXTBRZ6, @ptrCast(&g_runtime.ExtBrowser6), .P_STRING, .PI_TEXT, &.{});
    g_opts.register(.SETTINGS_EXTERNALPROGRAM, "extbrowser7", cmts.CMT_EXTBRZ7, @ptrCast(&g_runtime.ExtBrowser7), .P_STRING, .PI_TEXT, &.{});
    g_opts.register(.SETTINGS_EXTERNALPROGRAM, "extbrowser8", cmts.CMT_EXTBRZ8, @ptrCast(&g_runtime.ExtBrowser8), .P_STRING, .PI_TEXT, &.{});
    g_opts.register(.SETTINGS_EXTERNALPROGRAM, "extbrowser9", cmts.CMT_EXTBRZ9, @ptrCast(&g_runtime.ExtBrowser9), .P_STRING, .PI_TEXT, &.{});
    g_opts.register(.SETTINGS_EXTERNALPROGRAM, "bgextviewer", cmts.CMT_BGEXTVIEW, &g_runtime.BackgroundExtViewer, .P_INT, .PI_ONOFF, &.{});

    g_opts.register(.SETTINGS_PROXY, "use_proxy", cmts.CMT_USE_PROXY, &g_runtime.use_proxy, .P_CHARINT, .PI_ONOFF, &.{});
    g_opts.register(.SETTINGS_PROXY, "http_proxy", cmts.CMT_HTTP_PROXY, @ptrCast(&g_runtime.HTTP_proxy), .P_STRING, .PI_TEXT, &.{});
    g_opts.register(.SETTINGS_PROXY, "https_proxy", cmts.CMT_HTTPS_PROXY, @ptrCast(&g_runtime.HTTPS_proxy), .P_STRING, .PI_TEXT, &.{});
    g_opts.register(.SETTINGS_PROXY, "ftp_proxy", cmts.CMT_FTP_PROXY, @ptrCast(&g_runtime.FTP_proxy), .P_STRING, .PI_TEXT, &.{});
    g_opts.register(.SETTINGS_PROXY, "no_proxy", cmts.CMT_NO_PROXY, @ptrCast(&g_runtime.NO_proxy), .P_STRING, .PI_TEXT, &.{});
    g_opts.register(.SETTINGS_PROXY, "noproxy_netaddr", cmts.CMT_NOPROXY_NETADDR, &g_runtime.NOproxy_netaddr, .P_INT, .PI_ONOFF, &.{});
    g_opts.register(.SETTINGS_PROXY, "no_cache", cmts.CMT_NO_CACHE, &g_runtime.NoCache, .P_CHARINT, .PI_ONOFF, &.{});

    g_opts.register(.SETTINGS_NETWORK, "passwd_file", cmts.CMT_PASSWDFILE, @ptrCast(&g_runtime.passwd_file), .P_STRING, .PI_TEXT, &.{});
    g_opts.register(.SETTINGS_NETWORK, "disable_secret_security_check", cmts.CMT_DISABLE_SECRET_SECURITY_CHECK, &g_runtime.disable_secret_security_check, .P_INT, .PI_ONOFF, &.{});
    g_opts.register(.SETTINGS_NETWORK, "ftppasswd", cmts.CMT_FTPPASS, @ptrCast(&g_runtime.ftppasswd), .P_STRING, .PI_TEXT, &.{});
    g_opts.register(.SETTINGS_NETWORK, "ftppass_hostnamegen", cmts.CMT_FTPPASS_HOSTNAMEGEN, &g_runtime.ftppass_hostnamegen, .P_INT, .PI_ONOFF, &.{});
    g_opts.register(.SETTINGS_NETWORK, "pre_form_file", cmts.CMT_PRE_FORM_FILE, @ptrCast(&g_runtime.pre_form_file), .P_STRING, .PI_TEXT, &.{});
    g_opts.register(.SETTINGS_NETWORK, "siteconf_file", cmts.CMT_SITECONF_FILE, @ptrCast(&c.siteconf_file), .P_STRING, .PI_TEXT, &.{});
    g_opts.register(.SETTINGS_NETWORK, "user_agent", cmts.CMT_USERAGENT, @ptrCast(&g_runtime.UserAgent), .P_STRING, .PI_TEXT, &.{});
    g_opts.register(.SETTINGS_NETWORK, "no_referer", cmts.CMT_NOSENDREFERER, &g_runtime.NoSendReferer, .P_INT, .PI_ONOFF, &.{});
    g_opts.register(.SETTINGS_NETWORK, "cross_origin_referer", cmts.CMT_CROSSORIGINREFERER, &g_runtime.CrossOriginReferer, .P_INT, .PI_ONOFF, &.{});
    g_opts.register(.SETTINGS_NETWORK, "accept_language", cmts.CMT_ACCEPTLANG, @ptrCast(&g_runtime.AcceptLang), .P_STRING, .PI_TEXT, &.{});
    g_opts.register(.SETTINGS_NETWORK, "accept_encoding", cmts.CMT_ACCEPTENCODING, @ptrCast(&g_runtime.AcceptEncoding), .P_STRING, .PI_TEXT, &.{});
    g_opts.register(.SETTINGS_NETWORK, "accept_media", cmts.CMT_ACCEPTMEDIA, @ptrCast(&g_runtime.AcceptMedia), .P_STRING, .PI_TEXT, &.{});
    g_opts.register(.SETTINGS_NETWORK, "argv_is_url", cmts.CMT_ARGV_IS_URL, &g_runtime.ArgvIsURL, .P_CHARINT, .PI_ONOFF, &.{});
    g_opts.register(.SETTINGS_NETWORK, "retry_http", cmts.CMT_RETRY_HTTP, &g_runtime.retryAsHttp, .P_INT, .PI_ONOFF, &.{});
    g_opts.register(.SETTINGS_NETWORK, "default_url", cmts.CMT_DEFAULT_URL, &g_runtime.DefaultURLString, .P_INT, .PI_SEL_C, &defaulturls);
    g_opts.register(.SETTINGS_NETWORK, "follow_redirection", cmts.CMT_FOLLOW_REDIRECTION, &g_runtime.FollowRedirection, .P_INT, .PI_TEXT, &.{});
    g_opts.register(.SETTINGS_NETWORK, "meta_refresh", cmts.CMT_META_REFRESH, &g_runtime.MetaRefresh, .P_CHARINT, .PI_ONOFF, &.{});
    g_opts.register(.SETTINGS_NETWORK, "localhost_only", cmts.CMT_LOCALHOST_ONLY, &g_runtime.LocalhostOnly, .P_CHARINT, .PI_ONOFF, &.{});
    g_opts.register(.SETTINGS_NETWORK, "dns_order", cmts.CMT_DNS_ORDER, &g_runtime.DNS_order, .P_INT, .PI_SEL_C, &dnsorders);

    g_opts.register(.SETTINGS_COOKIE, "use_cookie", cmts.CMT_USECOOKIE, &g_runtime.use_cookie, .P_INT, .PI_ONOFF, &.{});
    g_opts.register(.SETTINGS_COOKIE, "show_cookie", cmts.CMT_SHOWCOOKIE, &g_runtime.show_cookie, .P_INT, .PI_ONOFF, &.{});
    g_opts.register(.SETTINGS_COOKIE, "accept_cookie", cmts.CMT_ACCEPTCOOKIE, &g_runtime.accept_cookie, .P_INT, .PI_ONOFF, &.{});
    g_opts.register(.SETTINGS_COOKIE, "accept_bad_cookie", cmts.CMT_ACCEPTBADCOOKIE, &g_runtime.accept_bad_cookie, .P_INT, .PI_SEL_C, &badcookiestr);
    g_opts.register(.SETTINGS_COOKIE, "cookie_reject_domains", cmts.CMT_COOKIE_REJECT_DOMAINS, @ptrCast(&g_runtime.cookie_reject_domains), .P_STRING, .PI_TEXT, &.{});
    g_opts.register(.SETTINGS_COOKIE, "cookie_accept_domains", cmts.CMT_COOKIE_ACCEPT_DOMAINS, @ptrCast(&g_runtime.cookie_accept_domains), .P_STRING, .PI_TEXT, &.{});
    g_opts.register(.SETTINGS_COOKIE, "cookie_avoid_wrong_number_of_dots", cmts.CMT_COOKIE_AVOID_WONG_NUMBER_OF_DOTS, @ptrCast(&g_runtime.cookie_avoid_wrong_number_of_dots), .P_STRING, .PI_TEXT, &.{});

    g_opts.register(.SETTINGS_SSL, "ssl_forbid_method", cmts.CMT_SSL_FORBID_METHOD, @ptrCast(&g_runtime.ssl_forbid_method), .P_STRING, .PI_TEXT, &.{});
    g_opts.register(.SETTINGS_SSL, "ssl_min_version", cmts.CMT_SSL_MIN_VERSION, @ptrCast(&g_runtime.ssl_min_version), .P_STRING, .PI_TEXT, &.{});
    g_opts.register(.SETTINGS_SSL, "ssl_cipher", cmts.CMT_SSL_CIPHER, @ptrCast(&g_runtime.ssl_cipher), .P_STRING, .PI_TEXT, &.{});
    g_opts.register(.SETTINGS_SSL, "ssl_verify_server", cmts.CMT_SSL_VERIFY_SERVER, &g_runtime.ssl_verify_server, .P_INT, .PI_ONOFF, &.{});
    g_opts.register(.SETTINGS_SSL, "ssl_cert_file", cmts.CMT_SSL_CERT_FILE, @ptrCast(&g_runtime.ssl_cert_file), .P_SSLPATH, .PI_TEXT, &.{});
    g_opts.register(.SETTINGS_SSL, "ssl_key_file", cmts.CMT_SSL_KEY_FILE, @ptrCast(&g_runtime.ssl_key_file), .P_SSLPATH, .PI_TEXT, &.{});
    g_opts.register(.SETTINGS_SSL, "ssl_ca_path", cmts.CMT_SSL_CA_PATH, @ptrCast(&g_runtime.ssl_ca_path), .P_SSLPATH, .PI_TEXT, &.{});
    g_opts.register(.SETTINGS_SSL, "ssl_ca_file", cmts.CMT_SSL_CA_FILE, @ptrCast(&g_runtime.ssl_ca_file), .P_SSLPATH, .PI_TEXT, &.{});
    g_opts.register(.SETTINGS_SSL, "ssl_ca_default", cmts.CMT_SSL_CA_DEFAULT, &g_runtime.ssl_ca_default, .P_INT, .PI_ONOFF, &.{});

    g_opts.register(.SETTINGS_CHARSET, "display_charset", cmts.CMT_DISPLAY_CHARSET, &g_runtime.DisplayCharset, .P_CODE, .PI_CODE, &.{});
    g_opts.register(.SETTINGS_CHARSET, "document_charset", cmts.CMT_DOCUMENT_CHARSET, &g_runtime.DocumentCharset, .P_CODE, .PI_CODE, &.{});
    g_opts.register(.SETTINGS_CHARSET, "auto_detect", cmts.CMT_AUTO_DETECT, &c.WcOption.auto_detect, .P_CHARINT, .PI_SEL_C, &auto_detect_str);
    g_opts.register(.SETTINGS_CHARSET, "system_charset", cmts.CMT_SYSTEM_CHARSET, &g_runtime.SystemCharset, .P_CODE, .PI_CODE, &.{});
    g_opts.register(.SETTINGS_CHARSET, "follow_locale", cmts.CMT_FOLLOW_LOCALE, &g_runtime.FollowLocale, .P_CHARINT, .PI_ONOFF, &.{});
    g_opts.register(.SETTINGS_CHARSET, "use_wide", cmts.CMT_USE_WIDE, &c.WcOption.use_wide, .P_CHARINT, .PI_ONOFF, &.{});
    g_opts.register(.SETTINGS_CHARSET, "use_combining", cmts.CMT_USE_COMBINING, &c.WcOption.use_combining, .P_CHARINT, .PI_ONOFF, &.{});
    g_opts.register(.SETTINGS_CHARSET, "east_asian_width", cmts.CMT_EAST_ASIAN_WIDTH, &c.WcOption.east_asian_width, .P_CHARINT, .PI_ONOFF, &.{});
    g_opts.register(.SETTINGS_CHARSET, "use_language_tag", cmts.CMT_USE_LANGUAGE_TAG, &c.WcOption.use_language_tag, .P_CHARINT, .PI_ONOFF, &.{});
    g_opts.register(.SETTINGS_CHARSET, "ucs_conv", cmts.CMT_UCS_CONV, &c.WcOption.ucs_conv, .P_CHARINT, .PI_ONOFF, &.{});
    g_opts.register(.SETTINGS_CHARSET, "pre_conv", cmts.CMT_PRE_CONV, &c.WcOption.pre_conv, .P_CHARINT, .PI_ONOFF, &.{});
    g_opts.register(.SETTINGS_CHARSET, "search_conv", cmts.CMT_SEARCH_CONV, &g_runtime.SearchConv, .P_CHARINT, .PI_ONOFF, &.{});
    g_opts.register(.SETTINGS_CHARSET, "fix_width_conv", cmts.CMT_FIX_WIDTH_CONV, &c.WcOption.fix_width_conv, .P_CHARINT, .PI_ONOFF, &.{});
    g_opts.register(.SETTINGS_CHARSET, "use_gb12345_map", cmts.CMT_USE_GB12345_MAP, &c.WcOption.use_gb12345_map, .P_CHARINT, .PI_ONOFF, &.{});
    g_opts.register(.SETTINGS_CHARSET, "use_jisx0201", cmts.CMT_USE_JISX0201, &c.WcOption.use_jisx0201, .P_CHARINT, .PI_ONOFF, &.{});
    g_opts.register(.SETTINGS_CHARSET, "use_jisc6226", cmts.CMT_USE_JISC6226, &c.WcOption.use_jisc6226, .P_CHARINT, .PI_ONOFF, &.{});
    g_opts.register(.SETTINGS_CHARSET, "use_jisx0201k", cmts.CMT_USE_JISX0201K, &c.WcOption.use_jisx0201k, .P_CHARINT, .PI_ONOFF, &.{});
    g_opts.register(.SETTINGS_CHARSET, "use_jisx0212", cmts.CMT_USE_JISX0212, &c.WcOption.use_jisx0212, .P_CHARINT, .PI_ONOFF, &.{});
    g_opts.register(.SETTINGS_CHARSET, "use_jisx0213", cmts.CMT_USE_JISX0213, &c.WcOption.use_jisx0213, .P_CHARINT, .PI_ONOFF, &.{});
    g_opts.register(.SETTINGS_CHARSET, "strict_iso2022", cmts.CMT_STRICT_ISO2022, &c.WcOption.strict_iso2022, .P_CHARINT, .PI_ONOFF, &.{});
    g_opts.register(.SETTINGS_CHARSET, "gb18030_as_ucs", cmts.CMT_GB18030_AS_UCS, &c.WcOption.gb18030_as_ucs, .P_CHARINT, .PI_ONOFF, &.{});
    g_opts.register(.SETTINGS_CHARSET, "simple_preserve_space", cmts.CMT_SIMPLE_PRESERVE_SPACE, &g_runtime.SimplePreserveSpace, .P_CHARINT, .PI_ONOFF, &.{});
}
