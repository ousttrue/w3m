#pragma once
#include "Str.h"
#include <stdbool.h>

extern int LINES, COLS;
#define LASTLINE (LINES - 1)
void term_setlinescols();

void term_fmTerm();
void term_fmInit();
bool term_graph_ok();
void term_clear();
void term_title(const char *s);
void term_bell();
void term_refresh();
void term_message(const char *msg);
Str term_inputpwd();
void term_input(const char *msg);

void setup_child(int child, int i, int f);
