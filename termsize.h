#pragma once
#include <stdbool.h>

extern int LINES, COLS;
#define LASTLINE (LINES - 1)
void term_setlinescols();
void resize_screen_if_updated();
