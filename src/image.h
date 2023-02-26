#pragma once
void put_image_iterm2(char *url, int x, int y, int w, int h);
void put_image_kitty(char *url, int x, int y, int w, int h, int sx, int sy,
                     int sw, int sh, int c, int r);
void put_image_osc5379(char *url, int x, int y, int w, int h, int sx, int sy,
                       int sw, int sh);
void put_image_sixel(char *url, int x, int y, int w, int h, int sx, int sy,
                     int sw, int sh, int n_terminal_image);
