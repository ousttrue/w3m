#pragma once

struct CmdArgs;

void message(const char* s, int return_x, int return_y);
void disp_message(struct CmdArgs* args, const char* s, int redraw_current);
void disp_err_message(struct CmdArgs* args, const char* s, int redraw_current);
void disp_message_nsec(struct CmdArgs* args, const char* s, int redraw_current, int sec, int purge, int mouse);
void set_delayed_message(const char* s);
void displayDilayedMessage(struct CmdArgs* args);
