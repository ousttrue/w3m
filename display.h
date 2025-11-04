#pragma once
#include "Line.h"

void fmInit(void);
void fmTerm(void);
void record_err_message(char* s);
void disp_message_nsec(char* s, int redraw_current, int sec, int purge, int mouse);
struct _Buffer;
void displayBuffer(struct _Buffer* buf, int mode);
void loadImage(struct _Buffer* buf, int flag);
void addChar(char c, Lineprop mode);
void addMChar(char* c, Lineprop mode, size_t len);
void message(char* s, int return_x, int return_y);
void disp_err_message(char* s, int redraw_current);
void disp_message(char* s, int redraw_current);
void disp_message_nomouse(char* s, int redraw_current);
void set_delayed_message(char* s);
