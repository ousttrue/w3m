#pragma once
#include <stdbool.h>

void displayDelayedMessage();
void record_err_message(const char* s);
struct Buffer* message_list_panel(void);
void message(const char* s);
void disp_err_message(const char* s, int redraw_current);
void disp_message_nsec(const char* s, int redraw_current, int sec, int purge, int mouse);
void disp_message(const char* s, bool redraw_current);
void set_delayed_message(const char* s);
void displayMsg(struct Buffer* buf);
