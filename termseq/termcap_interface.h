#pragma once

bool termcap_interface_load(const char *term);
bool termcap_graph_ok();
int termcap_int_line();
int termcap_int_cols();
const char *termcap_str_orig_pair();
const char *termcap_str_exit_attribute_mode();
const char *termcap_str_te();
const char *termcap_str_reset();
const char *termcap_str_acs_chars();
const char *termcap_str_enter_ca_mode();
const char *termcap_str_clear();
const char *termcap_str_cursor_mv();
const char *termcap_str_clr_eol();
const char *termcap_str_exit_alt_charset_mode();
const char *termcap_str_cursor_right();
const char *termcap_str_enter_standout_mode();
const char *termcap_str_enter_underline_mode();
const char *termcap_str_enter_bold_mode();
const char *termcap_str_ena_acs();
const char *termcap_str_enter_alt_charset_mode();
