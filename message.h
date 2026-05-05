#pragma once
#include "str_view.h"

void message(const char* s, int return_x, int return_y);
void disp_message(const char* s, int redraw_current);
void disp_err_message(const char* s, int redraw_current);
void disp_message_nsec(const char* s, int redraw_current, int sec, int purge, int mouse);
void set_delayed_message(const char* s);
void displayDilayedMessage();
void record_err_message(const char* s);
struct str_view message_list_panel(void);
