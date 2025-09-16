#pragma once
#include "geometry.h"
#include <wc.h>

extern char* mkd_tmp_dir;
extern int use_mark;
extern const char* config_file;
extern char FollowLocale;
extern int confirm_on_quit;
extern int CurrentKey;
extern const char* CurrentKeyData;
extern const char* CurrentCmdData;
extern char QuietMessage;
extern wc_ces DisplayCharset;
extern wc_ces BookmarkCharset;
extern int showLineNum;
extern const char* BookmarkFile;

// entry point
void main_loop(int argc, char** argv);

// raw mode
// TODO: UI
void fmInit();
// normal mode
// TODO: UI
void fmTerm();

void cursorUp(int n);
void cursorDown(int n);
void cursorUpDown(int n);
void cursorRight(int n);
void cursorLeft(int n);
void cursorHome();
struct Buffer;
bool updateCursor(struct Buffer* buf);
const char* searchKeyData();

void message(struct UI ui, enum MessageSeverity, const char* s);
inline static void error_message(struct UI ui, const char* s)
{
    message(ui, MSG_ERR, s);
}
void set_delayed_message(char* s);
void concatMessageList(Str tmp);
void renderFrame(struct UI ui);
void ui_bell();
void ui_printStatus(const char* fmt, ...);
void ui_cursor_set_x(int x);

// (line, bytepos) in buffer from cursor (row, col)
struct BufferPoint getBufferPosition(struct UI ui);
Str message_list_panel_html();
