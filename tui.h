#pragma once
#include <stdbool.h>

extern int fmInitialized;
extern int highIntensityColors;

void tui_fmInit();
void tui_fmTerm();
int tui_exec(const char* cmd);
void tui_record_err_message(char* s);
void tui_disp_message_nsec(char* s, int redraw_current, int sec, int purge, int mouse);
void tui_message(char* s, int return_x, int return_y);
void tui_disp_err_message(char* s, int redraw_current);
void tui_disp_message(char* s, int redraw_current);
void tui_disp_message_nomouse(char* s, int redraw_current);
void tui_set_delayed_message(char* s);
void tui_render_delayed_msg();
struct _Buffer* tui_message_list_panel();
void tui_render_screen();
