#pragma once
#include "line.h"

#define disp_message_nomouse disp_message

/* Flags for displayBuffer() */
#define B_NORMAL 0
#define B_FORCE_REDRAW 1
#define B_REDRAW 2
#define B_SCROLL 3
#define B_REDRAW_IMAGE 4

void fmInit(void);
void fmTerm(void);

struct CmdArgs;
struct Buffer;
void displayBuffer(struct CmdArgs *args, struct Buffer* buf, int mode);
void addChar(char c, Lineprop mode);
void addMChar(char* c, Lineprop mode, size_t len);
void record_err_message(const char* s);
struct Buffer* message_list_panel(void);
void message(const char* s, int return_x, int return_y);
void disp_err_message(struct CmdArgs *args, const char* s, int redraw_current);
void disp_message_nsec(struct CmdArgs* args, const char* s, int redraw_current, int sec, int purge, int mouse);
static inline void disp_message(struct CmdArgs* args, const char* s, int redraw_current)
{
    disp_message_nsec(args, s, redraw_current, 10, false, true);
}

void set_delayed_message(char* s);
