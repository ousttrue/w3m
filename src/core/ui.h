#pragma once
#include "geometry.h"
#include <Str.h>
#include <wc.h>

extern char QuietMessage;

extern wc_ces InnerCharset;
extern wc_ces DisplayCharset;
extern wc_ces SystemCharset;
extern wc_ces BookmarkCharset;
extern int showLineNum;

#define Str_conv_to_system(x) wc_Str_conv_strict((x), InnerCharset, SystemCharset)
#define Str_conv_to_halfdump(x) (ExtHalfdump ? wc_Str_conv((x), InnerCharset, DisplayCharset) : (x))
#define conv_from_system(x) wc_conv((x), SystemCharset, InnerCharset)->ptr
#define conv_to_system(x) wc_conv_strict((x), InnerCharset, SystemCharset)->ptr

const char* url_quote_conv(const char* x, wc_ces c);

struct Buffer;
extern struct Buffer* Currentbuf;
extern struct Buffer* Firstbuf;

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

struct VirtualTerm;

struct UI {
    struct VirtualTerm* vt;
    bool use_graphic;
    struct Rect viewport;
    struct Int2 cursor;
};

void cursorUp(int n);
void cursorDown(int n);
void cursorUpDown(int n);
void cursorRight(int n);
void cursorLeft(int n);
void cursorHome();
bool applyCursor();

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
