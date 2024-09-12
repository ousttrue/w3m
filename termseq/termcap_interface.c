#include "termcap_interface.h"
#include "termcap_load.h"

TermCap s_caps;

bool termcap_interface_load(const char *term) {
  return termcap_load(&s_caps, term);
}

bool termcap_graph_ok() {
  return s_caps.T_as[0] != 0 && s_caps.T_ae[0] != 0 && s_caps.T_ac[0] != 0;
}

int termcap_int_line() { return s_caps.LINES; }

int termcap_int_cols() { return s_caps.COLS; }

const char *termcap_str_orig_pair() { return s_caps.T_op; }

const char *termcap_str_exit_attribute_mode() { return s_caps.T_me; }

const char *termcap_str_te() {
  if (s_caps.T_te && *s_caps.T_te)
    return s_caps.T_te;
  else
    return s_caps.T_cl;
}

const char *termcap_str_reset() { return s_caps.T_se; }

const char *termcap_str_acs_chars() { return s_caps.T_ac; }

const char *termcap_str_enter_ca_mode() { return s_caps.T_ti; }

const char *termcap_str_clear() { return s_caps.T_cl; }
const char *termcap_str_clr_eol() { return s_caps.T_ce; }

const char *termcap_str_cursor_mv() { return s_caps.T_cm; }

const char *termcap_str_exit_alt_charset_mode() { return s_caps.T_ae; }

const char *termcap_str_cursor_right() { return s_caps.T_nd; }

const char *termcap_str_enter_standout_mode() { return s_caps.T_so; }

const char *termcap_str_enter_underline_mode() { return s_caps.T_us; }

const char *termcap_str_enter_bold_mode() { return s_caps.T_md; }

const char *termcap_str_ena_acs() { return s_caps.T_eA; }

const char *termcap_str_enter_alt_charset_mode() { return s_caps.T_as; }
