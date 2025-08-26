#pragma once

void put_image_osc5379(int cursorX, int cursorY,
    char* url, int x, int y, int w, int h, int sx, int sy, int sw, int sh);
void put_image_sixel(int cursorX, int cursorY,
    char* url, int x, int y, int w, int h, int sx, int sy, int sw, int sh, int n_terminal_image);
void put_image_iterm2(int cursorX, int cursorY,
    char* url, int x, int y, int w, int h);
void put_image_kitty(int cursorX, int cursorY,
    char* url, int x, int y, int w, int h, int sx, int sy, int sw, int sh, int c, int r);
