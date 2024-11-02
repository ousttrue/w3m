#pragma once
#include <stdbool.h>

extern int LINES, COLS;
void term_setlinescols();
void resize_screen_if_updated();
