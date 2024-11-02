#include "termsize.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

struct TermSize g_size = {-1, -1};

struct TermSize term_size() { return g_size; }

bool getWindowSize(int *rows, int *cols) {
  CONSOLE_SCREEN_BUFFER_INFO csbi;
  if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi) == 0) {
    return false;
  }

  *cols = csbi.srWindow.Right - csbi.srWindow.Left + 1;
  *rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
  return true;
}

struct TermSize term_setlinescols() {
  getWindowSize(&g_size.lines, &g_size.cols);
  return g_size;
}
