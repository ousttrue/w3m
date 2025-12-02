const std = @import("std");
const zlua = @import("zlua");
const Lua = zlua.Lua;
const gcstr = @import("gcstr");
const c = @cImport({
    @cInclude("w3m_runtime.h");
    @cInclude("w3m_config.h");
    @cInclude("local_cgi.h");
    @cInclude("image.h");
});

extern fn w3m_parse_arg(argc: c_int, argv: [*c][*c]c_char) c_int;
extern fn w3m_loop() c_int;

pub fn main() !u8 {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    const allocator = gpa.allocator();
    defer _ = gpa.deinit();

    var lua = try Lua.init(allocator);
    defer lua.deinit();

    lua.pushInteger(42);
    std.debug.print("{}\n", .{try lua.toInteger(1)});

    if (w3m_parse_arg(@intCast(std.os.argv.len), @ptrCast(&std.os.argv[0])) == 0) {
        return 1;
    }
    const code = w3m_loop();
    return @as(u8, @intCast(code));
}

const W3MHELPERPANEL_CMDNAME = "w3mhelperpanel";

const optionpanel_src1 = ("<html><head><title>Option Setting Panel</title></head><body>" //
    ++ "<h1 align=center>Option Setting Panel<br>(w3m version {s})</b></h1>" //
    ++ "<form method=post action=\"file:///$LIB/" ++ W3MHELPERPANEL_CMDNAME ++ "\">" //
    ++ "<input type=hidden name=mode value=panel>" //
    ++ "<input type=hidden name=cookie value=\"{s}\">" //
    ++ "<input type=submit value=\"{s}\">" //
    ++ "</form><br>" //
    ++ "<form method=internal action=option>" //
);

const SectionIterator = struct {
    sections: [*]c.param_section,
    pos: usize = 0,

    fn next(this: *@This()) ?*c.param_section {
        if (this.sections[this.pos].name == null) {
            return null;
        }
        defer this.pos += 1;
        return &this.sections[this.pos];
    }
};

const ParamIterator = struct {
    params: [*]c.param_ptr,
    pos: usize = 0,

    fn next(this: *@This()) ?*c.param_ptr {
        if (this.params[this.pos].name == null) {
            return null;
        }
        defer this.pos += 1;
        return &this.params[this.pos];
    }
};

const SelectIterator = struct {
    select: [*]c.sel_c,
    pos: usize = 0,

    fn next(this: *@This()) ?*c.sel_c {
        if (this.select[this.pos].text == null) {
            return null;
        }
        defer this.pos += 1;
        return &this.select[this.pos];
    }
};

const CesListIterator = struct {
    list: [*]c.wc_ces_list,
    pos: usize = 0,

    fn next(this: *@This()) ?*c.wc_ces_list {
        if (this.list[this.pos].desc == null) {
            return null;
        }
        defer this.pos += 1;
        return &this.list[this.pos];
    }
};

var RC_search_table: ?std.StringHashMap(*c.param_ptr) = null;

fn make_rc_table() !void {
    var map = std.StringHashMap(*c.param_ptr).init(gcstr.GcAllocator.allocator());
    defer RC_search_table = map;
    var sit = SectionIterator{
        .sections = c.w3m_config.sections,
    };
    while (sit.next()) |section| {
        var pit = ParamIterator{
            .params = section.params,
        };
        while (pit.next()) |p| {
            const name = std.mem.span(p.name);
            try map.put(name, p);
        }
    }
}

export fn config_make_rc_table() void {
    make_rc_table() catch @panic("config_make_rc_table");
}

export fn config_search_param(_name: [*c]const u8) [*c]c.param_ptr {
    if (_name) |name| {
        if (RC_search_table) |*table| {
            const span = std.mem.span(name);
            return table.get(span);
        } else {
            @panic("RC_search_table not initialized");
        }
    } else {
        return null;
    }
}

fn write_config_panel_html(writer: *std.Io.Writer) !void {
    try writer.print(optionpanel_src1, .{
        c.w3m_version,
        gcstr.c.html_quote(c.localCookie().*.ptr),
        "External Viewer Setup",
    });

    try writer.writeAll("<table><tr><td>");

    var sit = SectionIterator{
        .sections = c.w3m_config.sections,
    };
    while (sit.next()) |section| {
        try writer.print("<h1>{s}</h1>", .{section.name});
        try writer.writeAll("<table width=100% cellpadding=0>");

        var pit = ParamIterator{
            .params = section.params,
        };
        while (pit.next()) |p| {
            try writer.print("<tr><td>{s}</td><td width={}>", .{
                p.comment,
                @floor(28 * c.pixel_per_char),
            });
            const str = std.mem.span(c.to_str(p).*.ptr);
            switch (p.inputtype) {
                c.PI_TEXT => {
                    try writer.print("<input type=text name={s} value=\"{s}\">", .{
                        p.name,
                        c.html_quote(str),
                    });
                },
                c.PI_ONOFF => {
                    const x = try std.fmt.parseInt(c_int, str, 10);
                    try writer.print("<input type=radio name={s} value=1{s}>YES&nbsp;&nbsp;<input type=radio name={s} value=0{s}>NO", .{
                        p.name,
                        if (x != 0) " checked" else "",
                        p.name,
                        if (x != 0) "" else " checked",
                    });
                },
                c.PI_SEL_C => {
                    const value = try std.fmt.parseInt(c_int, str, 10);
                    try writer.print("<select name={s}>", .{p.name});
                    var selIt = SelectIterator{
                        .select = @ptrCast(@alignCast(p.select)),
                    };
                    // for (struct sel_c* s = (struct sel_c*)p.select; s.text != NULL; s++) {
                    while (selIt.next()) |s| {
                        try writer.print("<option value={s}\n", .{s.cvalue});
                        if ((p.type != c.P_CHAR and s.value == value)
                        // or (p.type == c.P_CHAR && (char)s.value == *(tmp.ptr))
                        ) {
                            try writer.writeAll(" selected");
                        }
                        try writer.writeByte('>');
                        try writer.writeAll(std.mem.span(s.text));
                    }
                    try writer.writeAll("</select>");
                },
                c.PI_CODE => {
                    try writer.print("<select name={s}>", .{
                        p.name,
                    });
                    const ptr: *[*]c.sel_c = @ptrCast(@alignCast(p.select));
                    var cesIt = CesListIterator{
                        .list = @ptrCast(@alignCast(ptr.*)),
                    };
                    //             for (wc_ces_list* c = *(wc_ces_list**)p.select; c.desc != NULL; c++) {
                    while (cesIt.next()) |ces| {
                        try writer.print("<option value={s}\n", .{ces.name});
                        if (ces.id == try std.fmt.parseInt(c_int, str, 10)) {
                            try writer.writeAll(" selected");
                        }
                        try writer.writeByte('>');
                        try writer.writeAll(std.mem.span(ces.desc));
                    }
                    try writer.writeAll("</select>");
                },
                else => {},
            }
            try writer.writeAll("</td></tr>\n");
        }
        try writer.writeAll("<tr><td></td><td><p><input type=submit value=\"OK\"></td></tr>");
        try writer.writeAll("</table><hr width=50%>");
    }
    try writer.writeAll("</table></form></body></html>");
}

export fn config_panel_html() gcstr.c.Str {
    // if (optionpanel_str == NULL)
    const allocator = gcstr.GcAllocator.allocator();
    var out = std.Io.Writer.Allocating.init(allocator);
    defer out.deinit();
    const writer: *std.io.Writer = &out.writer;

    write_config_panel_html(writer) catch @panic("_config_panel_html");

    const src = out.toOwnedSlice() catch @panic("OOM");
    return gcstr.Strnew_charp_n(&src[0], @intCast(src.len));
}

extern fn _config_panel_html() gcstr.c.Str;
extern fn w3m_initialize() void;

// test "config_panel" {
//     // var argv = [1][*]const u8{"test"};
//     // _ = w3m_parse_arg(@intCast(argv.len), @ptrCast(&argv[0]));
//     w3m_initialize();
//
//     const c_ver = _config_panel_html();
//     const z_ver = config_panel_html();
//     const c_span: []const u8 = c_ver.*.ptr[0..c_ver.*.length];
//     const z_span: []const u8 = z_ver.*.ptr[0..z_ver.*.length];
//     try std.testing.expectEqualSlices(u8, c_span, z_span);
// }

export fn str_to_bool(_value: [*c]const u8, old: bool) bool {
    if (_value == null)
        return true;

    const value = std.mem.span(_value);
    return switch (std.ascii.toLower(value[0])) {
        '0',
        'f', // false
        'n', // no
        'u', // undef
        => false,
        'o' => if (std.ascii.toLower(value[1]) == 'f') // off
            false
        else // on
            true,
        't' => if (std.ascii.toLower(value[1]) == 'o') // toggle */
            !old
        else
            true, // true
        '!',
        'r', // reverse
        'x', // exchange
        => return !old,
        else => true,
    };
}

test "str_to_bool" {
    {
        try std.testing.expectEqual(false, str_to_bool("0", true));
        try std.testing.expectEqual(false, str_to_bool("false", true));
        try std.testing.expectEqual(false, str_to_bool("no", true));
        try std.testing.expectEqual(false, str_to_bool("undef", true));
        try std.testing.expectEqual(false, str_to_bool("off", true));
        try std.testing.expectEqual(true, str_to_bool("on", true));
        try std.testing.expectEqual(false, str_to_bool("toggle", true));
        try std.testing.expectEqual(true, str_to_bool("true", true));
        try std.testing.expectEqual(false, str_to_bool("!", true));
        try std.testing.expectEqual(false, str_to_bool("reverse", true));
        try std.testing.expectEqual(false, str_to_bool("x", true));
    }
    {
        try std.testing.expectEqual(false, str_to_bool("0", false));
        try std.testing.expectEqual(false, str_to_bool("false", false));
        try std.testing.expectEqual(false, str_to_bool("no", false));
        try std.testing.expectEqual(false, str_to_bool("undef", false));
        try std.testing.expectEqual(false, str_to_bool("off", false));
        try std.testing.expectEqual(true, str_to_bool("on", false));
        try std.testing.expectEqual(true, str_to_bool("true", false));
        try std.testing.expectEqual(true, str_to_bool("toggle", false));
        try std.testing.expectEqual(true, str_to_bool("!", false));
        try std.testing.expectEqual(true, str_to_bool("reverse", false));
        try std.testing.expectEqual(true, str_to_bool("x", false));
    }
    {
        try std.testing.expectEqual(false, str_to_bool("f", false));
        try std.testing.expectEqual(false, str_to_bool("n", false));
        try std.testing.expectEqual(false, str_to_bool("u", false));
        try std.testing.expectEqual(true, str_to_bool("o", false));
        try std.testing.expectEqual(true, str_to_bool("o", false));
        try std.testing.expectEqual(true, str_to_bool("t", false));
        try std.testing.expectEqual(true, str_to_bool("r", false));
    }
}

// show parameter with bad options invokation
export fn show_params(handle: std.fs.File.Handle) void {
    const file = std.fs.File{
        .handle = handle,
    };
    var buf: [1024]u8 = undefined;
    var writer = file.writer(&buf);
    write_show_params(&writer.interface) catch @panic("show_params");
}

fn write_show_params(writer: *std.io.Writer) !void {
    defer writer.flush() catch @panic("OOM");
    try writer.writeAll("\nconfiguration parameters\n");
    var sit = SectionIterator{
        .sections = c.w3m_config.sections,
    };
    var j: usize = 0;
    const padding = " " ** 64;
    while (sit.next()) |section| : (j += 1) {
        try writer.print("  section[{}]: {s}\n", .{ j, section.name });
        var pit = ParamIterator{
            .params = section.params,
        };
        while (pit.next()) |p| {
            const t = switch (p.type) {
                c.P_INT, c.P_SHORT, c.P_CHARINT, c.P_NZINT => if (p.inputtype == c.PI_ONOFF) "bool" else "number",
                c.P_CHAR => "char",
                c.P_STRING => "string",
                c.P_SSLPATH => "path",
                c.P_COLOR => "color",
                c.P_CODE => "charset",
                c.P_PIXELS => "number",
                c.P_SCALE => "percent",
                else => unreachable,
            };
            const name = std.mem.span(p.name);
            var l: c_int = 30 - @as(c_int, @intCast(name.len + t.len));
            if (l < 0)
                l = 1;
            try writer.print("    -o {s}=<{s}>{s}{s}\n", .{
                name,
                t,
                padding[0..@intCast(l)],
                p.comment,
            });
        }
    }
}

export fn config_load(handle: std.fs.File.Handle) void {
    const file = std.fs.File{
        .handle = handle,
    };
    var linebuf: [512]u8 = undefined;
    var reader = file.reader(&linebuf);
    read_config(&reader.interface) catch @panic("config_load");
}

fn get_lower_key(line: []const u8, buf: []u8) ?[]const u8 {
    for (line, 0..) |ch, i| {
        if (std.ascii.isWhitespace(ch)) {
            buf[i] = 0;
            return buf[0..i];
        } else {
            buf[i] = std.ascii.toLower(ch);
        }
    }
    return null;
}

fn read_config(reader: *std.io.Reader) !void {
    var i: usize = 0;
    while (try reader.takeDelimiter('\n')) |_line| : (i += 1) {
        const line = std.mem.trimLeft(u8, _line, &std.ascii.whitespace);
        if (line.len == 0) {
            continue;
        }
        if (line[0] == '#') {
            // comment
            continue;
        }

        var _key: [64]u8 = undefined;
        const key = get_lower_key(line, &_key) orelse {
            std.log.err("{}: [parse error]{s}", .{ i, line });
            @panic("config: line has no space");
        };

        var sp_end = key.len + 1;
        while (sp_end < line.len) : (sp_end += 1) {
            if (!std.ascii.isWhitespace(line[sp_end])) {
                break;
            }
        }

        if (sp_end < line.len) {
            std.log.debug("{s} => {s}", .{ key, line[sp_end..] });
            _ = c.config_set_param(&key[0], &line[sp_end]);
        } else {
            std.log.debug("{s} -- empty ", .{key});
            _ = c.config_set_param(&key[0], "");
        }
    }
    // @panic("X");
}
