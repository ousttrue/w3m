#pragma once
#include <stdbool.h>
#include <gcstr/gcstr.h>

extern int fmInitialized;
extern int highIntensityColors;
extern int QuietMessage;

void tui_enter();
void tui_exit();
int tui_exec(const char* cmd);
void myExec(const char* command);
void mySystem(const char* command, int background);
void tui_setup_child(int child, int i, int f);
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
void tui_showProgress(long long current_content_length, long long* linelen, long long* trbyte);
const char* inputAnswer(const char* prompt);
void tui_input_pw(const char* realm, Str* uname, Str* pwd);
