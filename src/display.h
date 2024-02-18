#pragma once
#include "html/lineprop.h"
#include <stddef.h>

extern bool displayLink;

class TermScreen;
void _displayBuffer(TermScreen *screen, int width);
void addChar(TermScreen *screen, char c, Lineprop mode);
void addMChar(TermScreen *screen, char *p, Lineprop mode, size_t len);
