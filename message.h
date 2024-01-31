#pragma once

void message(const char *s, int return_x, int return_y);

void record_err_message(const char *s);
struct Buffer;
Buffer *message_list_panel();
void disp_err_message(const char *s, int redraw_current);
void disp_message_nsec(const char *s, int redraw_current, int sec, int purge,
                       int mouse);
void disp_message(const char *s, int redraw_current);
void set_delayed_message(const char *s);

void refresh_message();
