#pragma once

#include <stdio.h>
struct Runtime {
    int tty_input;
    FILE* tty_output_f;
    int lines;
    int cols;
};
struct Runtime* getRuntime();

inline static int TTY_LINES() { return getRuntime()->lines; }
inline static int TTY_COLS() { return getRuntime()->cols; }
inline static int LASTLINE() { return getRuntime()->lines - 1; }
void tty_set_cols(int cols);
