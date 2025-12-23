#pragma once

void displayDelayedMessage();
void record_err_message(char* s);
struct Buffer* message_list_panel(void);
void message(char* s, int return_x, int return_y);
void disp_err_message(char* s, int redraw_current);
void disp_message_nsec(char* s, int redraw_current, int sec, int purge, int mouse);
void disp_message(char* s, int redraw_current);
void set_delayed_message(char* s);
