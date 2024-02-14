#pragma once
#include "lineprop.h"
#include <stddef.h>

extern bool displayLink;
extern bool showLineNum;

int _INIT_BUFFER_WIDTH();
int INIT_BUFFER_WIDTH();
extern bool FoldLine;
int FOLD_BUFFER_WIDTH();

void setResize();
void displayBuffer();

void fmInit(void);
void fmTerm(void);

void addChar(char c, Lineprop mode);
void addMChar(char *p, Lineprop mode, size_t len);
