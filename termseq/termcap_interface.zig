extern fn tgetent(bp: [*]u8, name: [*:0]const u8) c_int;
extern fn tgetnum(cap: [*:0]const u8) c_int;
extern fn tgetflag(cap: [*:0]const u8) c_int;
extern fn tgetstr(cap: [*:0]const u8, buf: [*][*]u8) *u8;

const StringCapability = struct {
    desc: [:0]const u8,
    termcap: [:0]const u8,
    value: ?*const u8 = null,

    fn get(self: *@This(), p: **u8) ?*const u8 {
        self.value = @ptrCast(tgetstr(@ptrCast(&self.termcap[0]), @ptrCast(p)));
        return self.value;
    }
};

const IntCapability = struct {
    desc: [:0]const u8,
    termcap: [:0]const u8,
    value: ?i32 = null,

    fn get(self: *@This()) void {
        self.value = tgetnum(self.termcap);
    }
};

const TermCapabilities = struct {
    // initialize
    term_init: StringCapability = .{
        .desc = "terminal init",
        .termcap = "ti",
    },
    term_end: StringCapability = .{
        .desc = "terminal end",
        .termcap = "te",
    },
    // clear
    clear_screen: StringCapability = .{
        .termcap = "cl",
        .desc = "clear screen",
    },
    clear_to_eod: StringCapability = .{
        .termcap = "cd",
        .desc = "clear to the end of display",
    },
    clear_to_eol: StringCapability = .{
        .desc = "clear to the end of line",
        .termcap = "ce",
    },
    // keypad
    cursor_right: StringCapability = .{
        .desc = "cursor right",
        .termcap = "kr",
    },
    cursor_left: StringCapability = .{
        .desc = "cursor left",
        .termcap = "kl",
    },
    // cursor local
    move_right_one_space: StringCapability = .{
        .desc = "move right one space",
        .termcap = "nd",
    },
    carriage_return: StringCapability = .{
        .desc = "carriage return",
        .termcap = "cr",
    },
    // tab
    back_tab: StringCapability = .{
        .desc = "back tab",
        .termcap = "bt",
    },
    tab: StringCapability = .{
        .desc = "tab",
        .termcap = "ta",
    },
    // cursor absolute
    cursor_move: StringCapability = .{
        .desc = "cursor move",
        .termcap = "cm",
    },
    cursor_save: StringCapability = .{
        .desc = "save cursor",
        .termcap = "sc",
    },
    cursor_restore: StringCapability = .{
        .desc = "restore cursor",
        .termcap = "rc",
    },
    // effect
    standout_start: StringCapability = .{
        .desc = "standout mode",
        .termcap = "so",
    },
    standout_end: StringCapability = .{
        .desc = "standout mode end",
        .termcap = "se",
    },
    underline_start: StringCapability = .{
        .desc = "underline mode",
        .termcap = "us",
    },
    underline_end: StringCapability = .{
        .desc = "underline mode end",
        .termcap = "ue",
    },
    bold_start: StringCapability = .{
        .desc = "bold mode",
        .termcap = "md",
    },
    bold_end: StringCapability = .{
        .desc = "bold mode end",
        .termcap = "me",
    },
    // edit
    append_line: StringCapability = .{
        .desc = "append line",
        .termcap = "al",
    },
    // scroll
    scroll_reverse: StringCapability = .{
        .desc = "scroll reverse",
        .termcap = "sr",
    },
    // charset
    alternative_charset_start: StringCapability = .{
        .desc = "alternative (graphic) charset start",
        .termcap = "as",
    },
    alternative_charset_end: StringCapability = .{
        .desc = "alternative (graphic) charset end",
        .termcap = "ae",
    },
    alternative_charset_enable: StringCapability = .{
        .desc = "enable alternative charset",
        .termcap = "eA",
    },
    line_graphic: StringCapability = .{
        .desc = "graphics charset pairs",
        .termcap = "ac",
    },
    // color
    color_default: StringCapability = .{
        .desc = "set default color pair to its original value",
        .termcap = "op",
    },

    LINES: IntCapability = .{
        .desc = "number of line",
        .termcap = "li",
    },
    COLS: IntCapability = .{
        .desc = "number of column",
        .termcap = "co",
    },
    bp: [1024]u8 = undefined,
    funcstr: [256]u8 = undefined,

    fn isGraphOk(self: @This()) bool {
        return self.alternative_charset_start.value != null and
            self.alternative_charset_end.value != null and
            self.line_graphic.value != null;
    }

    fn termcap_load(t: *@This(), name: [*:0]const u8) bool {
        if (tgetent(@ptrCast(&t.bp[0]), name) != 1) {
            return false;
        }

        var pt = &t.funcstr[0];
        _ = t.clear_to_eol.get(&pt);
        _ = t.clear_to_eod.get(&pt);
        if (t.move_right_one_space.get(&pt)) |value| {
            t.cursor_right.value = value;
        } else {
            _ = t.cursor_right.get(&pt);
        }

        if (tgetflag("bs") != 0) {
            t.cursor_left.value = &("\x08")[0];
        } else {
            t.cursor_left.value = tgetstr("le", @ptrCast(&pt));
            if (t.cursor_left.value == null) {
                t.cursor_left.value = tgetstr("kb", @ptrCast(&pt));
            }
            if (t.cursor_left.value == null) {
                t.cursor_left.value = tgetstr("kl", @ptrCast(&pt));
            }
        }
        _ = t.carriage_return.get(&pt);
        _ = t.tab.get(&pt);
        _ = t.cursor_save.get(&pt);
        _ = t.cursor_restore.get(&pt);
        _ = t.standout_start.get(&pt);
        _ = t.standout_end.get(&pt);
        _ = t.underline_start.get(&pt);
        _ = t.underline_end.get(&pt);
        _ = t.bold_start.get(&pt);
        _ = t.bold_end.get(&pt);
        _ = t.clear_screen.get(&pt);
        _ = t.cursor_move.get(&pt);
        _ = t.append_line.get(&pt);
        _ = t.scroll_reverse.get(&pt);
        _ = t.term_init.get(&pt);
        _ = t.term_end.get(&pt);
        _ = t.move_right_one_space.get(&pt);
        _ = t.alternative_charset_enable.get(&pt);
        _ = t.alternative_charset_start.get(&pt);
        _ = t.alternative_charset_end.get(&pt);
        _ = t.line_graphic.get(&pt);
        _ = t.color_default.get(&pt);
        t.LINES.get();
        t.COLS.get();
        return true;
    }
};

var s_caps = TermCapabilities{};

export fn termcap_interface_load(term: [*:0]const u8) bool {
    return s_caps.termcap_load(term);
}

export fn termcap_graph_ok() bool {
    return s_caps.isGraphOk();
}

export fn termcap_int_line() c_int {
    return s_caps.LINES.value orelse 0;
}

export fn termcap_int_cols() c_int {
    return s_caps.COLS.value orelse 0;
}

export fn termcap_str_orig_pair() ?*const u8 {
    return s_caps.color_default.value;
}

export fn termcap_str_exit_attribute_mode() ?*const u8 {
    return s_caps.bold_end.value;
}

export fn termcap_str_te() ?*const u8 {
    if (s_caps.term_end.value) |te| {
        return te;
    } else {
        return s_caps.clear_screen.value;
    }
}

export fn termcap_str_reset() ?*const u8 {
    return s_caps.standout_end.value;
}

export fn termcap_str_acs_chars() ?*const u8 {
    return s_caps.line_graphic.value;
}

export fn termcap_str_enter_ca_mode() ?*const u8 {
    return s_caps.term_init.value;
}

export fn termcap_str_clear() ?*const u8 {
    return s_caps.clear_screen.value;
}

export fn termcap_str_clr_eol() ?*const u8 {
    return s_caps.clear_to_eol.value;
}

export fn termcap_str_cursor_mv() ?*const u8 {
    return s_caps.cursor_move.value;
}

export fn termcap_str_exit_alt_charset_mode() ?*const u8 {
    return s_caps.alternative_charset_end.value;
}

export fn termcap_str_cursor_right() ?*const u8 {
    return s_caps.move_right_one_space.value;
}

export fn termcap_str_enter_standout_mode() ?*const u8 {
    return s_caps.standout_start.value;
}

export fn termcap_str_enter_underline_mode() ?*const u8 {
    return s_caps.underline_start.value;
}

export fn termcap_str_enter_bold_mode() ?*const u8 {
    return s_caps.bold_start.value;
}

export fn termcap_str_ena_acs() ?*const u8 {
    return s_caps.alternative_charset_enable.value;
}

export fn termcap_str_enter_alt_charset_mode() ?*const u8 {
    return s_caps.alternative_charset_start.value;
}
