#include "tty.h"
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <stdarg.h>
#include <stdio.h>

DWORD g_fdwSaveOldMode = 0;
HANDLE g_hStdin;

INPUT_RECORD g_irInBuf[128];
DWORD g_cNumRead = 0;
size_t g_current = 0;

// stream に対してANSIエスケープシーケンスを有効化
// 成功すれば true, 失敗した場合は false を返す
bool enable_virtual_terminal_processing() {
  HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
  DWORD mode = 0;
  if (!GetConsoleMode(handle, &mode)) {
    // 失敗
    return false;
  }
  if (!SetConsoleMode(handle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING)) {
    // 失敗
    // 古いWindowsコンソールの場合は GetLastError() == ERROR_INVALID_PARAMETER
    return false;
  }
  return true;
}

void tty_open() {
  g_hStdin = GetStdHandle(STD_INPUT_HANDLE);
  enable_virtual_terminal_processing();
}
void tty_close() {}

//
// input
//
void tty_cbreak() {}
void tty_crmode() {}
void tty_nocrmode() {}

void tty_raw() {

  if (!GetConsoleMode(g_hStdin, &g_fdwSaveOldMode)) {
    // ErrorExit("GetConsoleMode");
    return;
  }

  DWORD fdwMode = ENABLE_WINDOW_INPUT | ENABLE_MOUSE_INPUT;
  if (!SetConsoleMode(g_hStdin, fdwMode)) {
    // ErrorExit("SetConsoleMode");
    return;
  }
}

void tty_cooked() { SetConsoleMode(g_hStdin, g_fdwSaveOldMode); }
char tty_getch() {
  if (g_current >= g_cNumRead) {
    g_current = 0;
    if (!ReadConsoleInput(g_hStdin,       // input buffer handle
                          g_irInBuf,      // buffer to read into
                          128,            // size of read buffer
                          &g_cNumRead)) { // number of records read
      return 0;
      // ErrorExit("ReadConsoleInput");
    }
  }

  // Dispatch the events to the appropriate handler.
  for (; g_current < g_cNumRead;) {
    auto record = g_irInBuf[g_current++];
    switch (record.EventType) {
    case KEY_EVENT: // keyboard input
      // KeyEventProc(irInBuf[i].Event.KeyEvent);
      {
        auto e = record.Event.KeyEvent;
        if (e.bKeyDown) {
          return e.uChar.AsciiChar;
        }
      }
      break;

    case MOUSE_EVENT: // mouse input
      // MouseEventProc(irInBuf[i].Event.MouseEvent);
      break;

    case WINDOW_BUFFER_SIZE_EVENT: // scrn buf. resizing
      // ResizeEventProc(irInBuf[i].Event.WindowBufferSizeEvent);
      break;

    case FOCUS_EVENT: // disregard focus events

    case MENU_EVENT: // disregard menu events
      break;

    default:
      // ErrorExit("Unknown event type");
      break;
    }
  }

  return 0;
}

int tty_sleep_till_anykey(int sec, bool purge) { return tty_getch(); }

//
// output
//
void tty_echo() {}
void tty_noecho() {}
void tty_flush() { FlushFileBuffers(GetStdHandle(STD_OUTPUT_HANDLE)); }
int tty_putc(int c) { putc(c, stdout); }
void tty_printf(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vprintf(fmt, ap);
  va_end(ap);
}
