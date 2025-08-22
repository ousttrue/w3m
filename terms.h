#pragma once

extern int LINES, COLS;
extern int get_pixel_per_cell(int* ppc, int* ppl);
char getch(void);
void setlinescols(void);
void flush_tty(void);
