#include "termsize.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

int LINES = 0;
int COLS = 0;

bool getWindowSize(int *rows, int *cols) {
  CONSOLE_SCREEN_BUFFER_INFO csbi;
  if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi) == 0) {
    return false;
  }

  *cols = csbi.srWindow.Right - csbi.srWindow.Left + 1;
  *rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
  return true;
}

void term_setlinescols() { getWindowSize(&LINES, &COLS); }
