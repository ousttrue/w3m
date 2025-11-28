#pragma once
#include <stdbool.h>
#include <gcstr.h>

extern int highIntensityColors;
extern int QuietMessage;
extern char PermitSaveToPipe;
extern char PreserveTimestamp;
extern char AutoUncompress;

void tui_enter();
void tui_exit();
int tui_exec(const char* cmd);
void myExec(const char* command);
void mySystem(const char* command, int background);
void tui_setup_child(int child, int i, int f);
void tui_record_err_message(char* s);
void tui_disp_message_nsec(char* s, int redraw_current, int sec, int purge, int mouse);
void tui_message(const char* s);
void tui_disp_err_message(char* s, int redraw_current);
void tui_disp_message(char* s, int redraw_current);
void tui_disp_message_nomouse(char* s, int redraw_current);
void tui_set_delayed_message(char* s);
void tui_render_delayed_msg();
struct Buffer* tui_message_list_panel();
void tui_render_screen();
void tui_showProgress(long long current_content_length, long long* linelen, long long* trbyte);
const char* inputAnswer(const char* prompt);
void tui_input_user_pw(const char* realm, Str* uname, Str* pwd);
Str tui_input_pw();
void tui_GC_warn_proc(const char* msg, unsigned long arg);

char* searchKeyData(void);
int tui_doFileCopy(const char* tmpf, const char* defstr, bool download);
inline static int doFileCopy(const char* tmpf, const char* defstr)
{
    return tui_doFileCopy(tmpf, defstr, false);
}
int doFileMove(char* tmpf, char* defstr);
int tui_checkOverWrite(const char* path);
int tui_checkCopyFile(const char* path1, const char* path2);
struct URLFile;
int tui_doFileSave(struct URLFile* uf, const char* defstr);

