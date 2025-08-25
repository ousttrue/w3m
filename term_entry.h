#pragma once
#include <stdbool.h>

enum GrahicCharType {
    GRAPHIC_CHAR_CHARSET = 0,
    GRAPHIC_CHAR_DEC = 1,
    GRAPHIC_CHAR_ASCII = 2,
};

extern enum GrahicCharType UseGraphicChar;
extern int Do_not_use_ti_te;

void initTerm();
void resetTerm();
bool graph_ok();
char graphchar(char c);

void write_T_op();
void write_T_ce();
void write_T_ae();
void write_T_me();
void write_T_nd();
void write_T_so();
void write_T_us();
void write_T_md();
void write_T_eA();
void write_T_as();
void write_T_cl();
void MOVE(int line, int column);

void put_image_osc5379(int cursorX, int cursorY,
    char* url, int x, int y, int w, int h, int sx, int sy, int sw, int sh);
void put_image_sixel(int cursorX, int cursorY,
    char* url, int x, int y, int w, int h, int sx, int sy, int sw, int sh, int n_terminal_image);
void put_image_iterm2(int cursorX, int cursorY,
    char* url, int x, int y, int w, int h);
void put_image_kitty(int cursorX, int cursorY,
    char* url, int x, int y, int w, int h, int sx, int sy, int sw, int sh, int c, int r);
