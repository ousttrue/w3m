#pragma once
#include "geometry.h"
#include <Str.h>
#include <wc.h>

extern char QuietMessage;

extern wc_ces DisplayCharset;
extern wc_ces BookmarkCharset;
extern int showLineNum;
extern const char* BookmarkFile;

// key input(blocking) or draw require UI or query tty
//
// form input
// lineinput
// search
// menu
// image
// message(cookie, etc...)
//
enum MessageSeverity {
    MSG_INFO,
    MSG_ERR,
};

void cursorUp(int n);
void cursorDown(int n);
void cursorUpDown(int n);
void cursorRight(int n);
void cursorLeft(int n);
void cursorHome();
bool updateCursor(struct Buffer* buf);
const char* searchKeyData();

struct UI getUI();
void message(struct UI ui, enum MessageSeverity, const char* s);
inline static void error_message(const char* s)
{
    message(getUI(), MSG_ERR, s);
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
