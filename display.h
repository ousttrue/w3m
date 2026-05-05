#pragma once
#include "line.h"

#define disp_message_nomouse disp_message

struct CmdArgs;
struct Buffer;

enum DisplayBufferMode {
    B_NORMAL = 0,
    B_FORCE_REDRAW = 1,
    B_REDRAW = 2,
    B_SCROLL = 3,
    B_REDRAW_IMAGE = 4,
};

void displayBuffer(struct CmdArgs* args, enum DisplayBufferMode mode);

void addChar(char c, Lineprop mode);
void addMChar(char* c, Lineprop mode, size_t len);
