const std = @import("std");
const zlua = @import("zlua");
const Lua = zlua.Lua;
const gcstr = @import("gcstr");
const c = @cImport({
    @cInclude("w3m_runtime.h");
    @cInclude("w3m_config.h");
    @cInclude("local_cgi.h");
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

export fn config_panel_html() gcstr.c.Str {
    // if (optionpanel_str == NULL)
    const allocator = gcstr.GcAllocator.allocator();
    var out = std.Io.Writer.Allocating.init(allocator);
    defer out.deinit();
    var writer: *std.io.Writer = &out.writer;

    writer.print(optionpanel_src1, .{
        c.w3m_version,
        gcstr.c.html_quote(c.localCookie().*.ptr),
        "External Viewer Setup",
    }) catch @panic("OOM");

    // if (!OptionEncode) {
    //     optionpanel_str = wc_Str_conv(optionpanel_str, OptionCharset, InnerCharset);
    //     for (int i = 0; sections[i].name != NULL; i++) {
    //         sections[i].name = wc_conv(_(sections[i].name), OptionCharset, InnerCharset)->ptr;
    //         for (struct param_ptr* p = sections[i].params; p->name; p++) {
    //             p->comment = wc_conv(_(p->comment), OptionCharset, InnerCharset)->ptr;
    //             if (p->inputtype == PI_SEL_C && p->select != colorstr) {
    //                 for (struct sel_c* s = (struct sel_c*)p->select; s->text != NULL; s++) {
    //                     s->text = wc_conv(_(s->text), OptionCharset, InnerCharset)->ptr;
    //                 }
    //             }
    //         }
    //     }
    //     for (struct sel_c* s = colorstr; s->text; s++)
    //         s->text = wc_conv(_(s->text), OptionCharset, InnerCharset)->ptr;
    //     OptionEncode = true;
    // }
    // Str src = Strdup(optionpanel_str);

    writer.writeAll("<table><tr><td>") catch @panic("OOM");
    var i: usize = 0;
    // for (int i = 0; sections[i].name != NULL; i++) {
    while (c.w3m_config.sections[i].name != null) : (i += 1) {
        const section = &c.w3m_config.sections[i];
        if (section.name == null) {
            break;
        }
        // std.debug.print("{}: {s}", .{ i, section.name });
        // struct param_section* section = &sections[i];
        writer.print("<h1>{s}</h1>", .{section.name}) catch @panic("OOM");
        //     Strcat_charp(src, "<table width=100% cellpadding=0>");
        //     for (struct param_ptr* p = section->params; p->name; ++p) {
        //         Strcat_m_charp(src, "<tr><td>", p->comment, NULL);
        //         Strcat(src, Sprintf("</td><td width=%d>", (int)(28 * pixel_per_char)));
        //         switch (p->inputtype) {
        //         case PI_TEXT:
        //             Strcat_m_charp(src, "<input type=text name=",
        //                 p->name,
        //                 " value=\"",
        //                 html_quote(to_str(p)->ptr), "\">", NULL);
        //             break;
        //         case PI_ONOFF: {
        //             int x = atoi(to_str(p)->ptr);
        //             Strcat_m_charp(src, "<input type=radio name=",
        //                 p->name,
        //                 " value=1",
        //                 (x ? " checked" : ""),
        //                 ">YES&nbsp;&nbsp;<input type=radio name=",
        //                 p->name,
        //                 " value=0", (x ? "" : " checked"), ">NO", NULL);
        //             break;
        //         }
        //         case PI_SEL_C: {
        //             Str tmp = to_str(p);
        //             Strcat_m_charp(src, "<select name=", p->name, ">", NULL);
        //             for (struct sel_c* s = (struct sel_c*)p->select; s->text != NULL; s++) {
        //                 Strcat_charp(src, "<option value=");
        //                 Strcat(src, Sprintf("%s\n", s->cvalue));
        //                 if ((p->type != P_CHAR && s->value == atoi(tmp->ptr))
        //                     || (p->type == P_CHAR && (char)s->value == *(tmp->ptr)))
        //                     Strcat_charp(src, " selected");
        //                 Strcat_char(src, '>');
        //                 Strcat_charp(src, s->text);
        //             }
        //             Strcat_charp(src, "</select>");
        //             break;
        //         }
        //         case PI_CODE: {
        //             Str tmp = to_str(p);
        //             Strcat_m_charp(src, "<select name=", p->name, ">", NULL);
        //             for (wc_ces_list* c = *(wc_ces_list**)p->select; c->desc != NULL; c++) {
        //                 Strcat_charp(src, "<option value=");
        //                 Strcat(src, Sprintf("%s\n", c->name));
        //                 if (c->id == atoi(tmp->ptr))
        //                     Strcat_charp(src, " selected");
        //                 Strcat_char(src, '>');
        //                 Strcat_charp(src, c->desc);
        //             }
        //             Strcat_charp(src, "</select>");
        //             break;
        //         }
        //         }
        //         Strcat_charp(src, "</td></tr>\n");
        //     }
        //     Strcat_charp(src,
        //         "<tr><td></td><td><p><input type=submit value=\"OK\"></td></tr>");
        //     Strcat_charp(src, "</table><hr width=50%>");
    }
    // Strcat_charp(src, "</table></form></body></html>");
    // return src;
    const src = out.toOwnedSlice() catch @panic("OOM");
    return gcstr.Strnew_charp_n(&src[0], @intCast(src.len));
}

extern fn _config_panel_html() gcstr.c.Str;
extern fn w3m_initialize() void;

test "config_panel" {
    // var argv = [1][*]const u8{"test"};
    // _ = w3m_parse_arg(@intCast(argv.len), @ptrCast(&argv[0]));
    w3m_initialize();

    const c_ver = _config_panel_html();
    const z_ver = config_panel_html();
    const c_span: []const u8 = c_ver.*.ptr[0..c_ver.*.length];
    const z_span: []const u8 = z_ver.*.ptr[0..z_ver.*.length];
    try std.testing.expectEqualSlices(u8, c_span, z_span);
}
