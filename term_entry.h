#pragma once
#include <stdbool.h>

extern int Do_not_use_ti_te;

struct TermEntry {
    char funcstr[256];
    char* cd;
    char* ce;
    char* kr;
    char* kl;
    char* cr;
    char* bt;
    char* ta;
    char* sc;
    char* rc;
    char* so;
    char* se;
    char* us;
    char* ue;
    char* cl;
    char* cm;
    char* al;
    char* sr;
    char* md;
    char* me;
    char* ti;
    char* te;
    char* nd;
    char* as;
    char* ae;
    char* eA;
    char* ac;
    char* op;
};

struct TermEntry* initTerm();
void resetTerm();
struct TermEntry* getTermEntry();

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
