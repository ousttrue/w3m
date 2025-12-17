#pragma once

#ifdef USE_IMAGE
extern void put_image_osc5379(char* url, int x, int y, int w, int h, int sx, int sy, int sw, int sh);
extern void put_image_sixel(char* url, int x, int y, int w, int h, int sx, int sy, int sw, int sh, int n_terminal_image);
extern void put_image_iterm2(char* url, int x, int y, int w, int h);
extern void put_image_kitty(char* url, int x, int y, int w, int h, int sx, int sy, int sw, int sh, int c, int r);
extern int get_pixel_per_cell(int* ppc, int* ppl);
#endif

char getch(void);
