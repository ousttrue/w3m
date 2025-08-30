#pragma once
#include <Str.h>

enum MessageSeverity
{
    MSG_INFO,
    MSG_ERR,
};

void message(enum MessageSeverity, const char* s);
// extern void disp_err_message(char* s, int redraw_current);
// extern void disp_message_nsec(char* s, int redraw_current, int sec, int purge, int mouse);
// extern void disp_message(char* s, int redraw_current);

void concatMessageList(Str tmp);
