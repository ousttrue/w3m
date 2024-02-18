#pragma once
#include "html/lineprop.h"
#include <stddef.h>

extern bool displayLink;

extern bool showLineNum;
int _INIT_BUFFER_WIDTH();
int INIT_BUFFER_WIDTH();

class TermScreen;
void _displayBuffer(TermScreen *screen);
void addChar(TermScreen *screen, char c, Lineprop mode);
void addMChar(TermScreen *screen, char *p, Lineprop mode, size_t len);
