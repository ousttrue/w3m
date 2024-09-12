const TermCapabilities = @import("TermCapabilities.zig");

var s_caps = TermCapabilities{};

export fn termcap_interface_load(term: [*:0]const u8) bool {
    return s_caps.load(term);
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
