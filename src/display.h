#pragma once
#include "html/lineprop.h"
#include <stddef.h>

extern bool displayLink;
extern bool showLineNum;

int _INIT_BUFFER_WIDTH();
int INIT_BUFFER_WIDTH();
extern bool FoldLine;
int FOLD_BUFFER_WIDTH();

void setResize();
void _displayBuffer();

void addChar(char c, Lineprop mode);
void addMChar(char *p, Lineprop mode, size_t len);
