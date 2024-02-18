#pragma once
#include "html/lineprop.h"
#include <stddef.h>

extern bool displayLink;

class TermScreen;
void _displayBuffer(TermScreen *screen, int width);
