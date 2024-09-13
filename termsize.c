#include "termsize.h"
#include <sys/ioctl.h>
#include <stdlib.h>

#define MAX_LINE 200
#define MAX_COLUMN 400
int LINES = -1;
int COLS = -1;

// Try to get the number of columns in the current terminal. If the ioctl()
// call fails the function will try to query the terminal itself.
// Returns 0 on success, -1 on error.

static bool get_term_lines_cols(int *rows, int *cols) {
  struct winsize ws;
  if (ioctl(1, TIOCGWINSZ, &ws) != -1 && ws.ws_col > 0) {
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return true;
  }

  return false;
}

void term_setlinescols() {
  if (get_term_lines_cols(&LINES, &COLS)) {
    return;
  }

  // fallback
  char *p;
  int i;
  if (LINES <= 0 && (p = getenv("LINES")) != NULL && (i = atoi(p)) >= 0)
    LINES = i;
  if (COLS <= 0 && (p = getenv("COLUMNS")) != NULL && (i = atoi(p)) >= 0)
    COLS = i;
  // if (LINES <= 0)
  //   LINES = tgetnum("li");
  // if (COLS <= 0)
  //   COLS = tgetnum("co");
  if (COLS > MAX_COLUMN)
    COLS = MAX_COLUMN;
  if (LINES > MAX_LINE)
    LINES = MAX_LINE;
}
