#include "term/termcon.h"
#include <stdlib.h>
#include <stdio.h>
#include "termcap_interface.h"
#include "term/tty.h"

#if defined(__DJGPP__)
#define DEFAULT_TERM "dosansi"
#else
#define DEFAULT_TERM 0 /* XXX */
#endif

static void reset_error_exit() {
  // term_reset();
  termcon_color_default();
  termcon_attribute_clear();
  // tty_flush();
  // tty_close();
  // w3m_exit(1);
}

void termcon_initialize() {
  auto ent = getenv("TERM") ? getenv("TERM") : DEFAULT_TERM;
  if (ent == NULL) {
    fprintf(stderr, "TERM is not set\n");
    reset_error_exit();
  }

  if (!termcap_interface_load(ent)) {
    /* Can't find termcap entry */
    fprintf(stderr, "Can't find termcap entry %s\n", ent);
    reset_error_exit();
  }

  auto ti = termcap_str_enter_ca_mode();
  if (ti) {
    tty_puts(ti);
  }
}

void termcon_finalize() { tty_puts(termcap_str_te()); }
void termcon_color_default() { tty_puts(termcap_str_orig_pair()); }
void termcon_attribute_clear() { tty_puts(termcap_str_exit_attribute_mode()); }

void termcon_standout_start() { tty_puts(termcap_str_enter_standout_mode()); }
// void termcon_standout_end();
void termcon_bold_start() { tty_puts(termcap_str_enter_bold_mode()); }
// void termcon_bold_end();
void termcon_underline_start() { tty_puts(termcap_str_enter_underline_mode()); }
// void termcon_underline_end();

bool termcon_acs_has() { return termcap_graph_ok(); }
void termcon_acs_enable() { tty_puts(termcap_str_ena_acs()); }
const char *termcon_acs_map() { return termcap_str_acs_chars(); }
void termcon_altchar_end() { tty_puts(termcap_str_exit_alt_charset_mode()); }
void termcon_altchar_start() { tty_puts(termcap_str_enter_alt_charset_mode()); }
void termcon_clear() {
  // tty_puts(termcap_str_clear());
  tty_puts(termcap_str_reset());
}
void termcon_clear_eol() { tty_puts(termcap_str_clr_eol()); }
extern const char *tgoto(const char *fmt, int col, int line);
void termcon_cursor_move(int line, int col) {
  tty_puts(tgoto(termcap_str_cursor_mv(), col, line));
}
void termcon_cursor_right() { tty_puts(termcap_str_cursor_right()); }
