#pragma once

enum DisplayMode {
    B_NORMAL = 0,
    B_FORCE_REDRAW = 1,
    B_REDRAW = 2,
    B_SCROLL = 3,
    B_REDRAW_IMAGE = 4,
};

struct Buffer;
void displayBuffer(struct Buffer* buf, enum DisplayMode mode);

void screen_wc_addstr(char* s);
void screen_wc_addstr_width(char* s, int width);
void screen_wc_addnstr_sup(char* s, int n);
