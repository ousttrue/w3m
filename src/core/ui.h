#pragma once
#include <Str.h>
#include <wc.h>

extern char* CurrentDir;
extern int CurrentPid;
extern wc_ces InnerCharset;
extern wc_ces DisplayCharset;
extern wc_ces SystemCharset;
extern wc_ces DocumentCharset;
extern wc_ces BookmarkCharset;

#define Str_conv_from_system(x) wc_Str_conv((x), SystemCharset, InnerCharset)
#define Str_conv_to_system(x) wc_Str_conv_strict((x), InnerCharset, SystemCharset)
#define Str_conv_to_halfdump(x) (ExtHalfdump ? wc_Str_conv((x), InnerCharset, DisplayCharset) : (x))
#define conv_from_system(x) wc_conv((x), SystemCharset, InnerCharset)->ptr
#define conv_to_system(x) wc_conv_strict((x), InnerCharset, SystemCharset)->ptr
#define url_quote_conv(x, c) url_quote(wc_conv_strict((x), InnerCharset, (c))->ptr)

struct _Buffer;
extern struct _Buffer* Currentbuf;
extern struct _Buffer* Firstbuf;

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
};

struct UI getUI();
void message(struct UI ui, enum MessageSeverity, const char* s);
void set_delayed_message(char* s);
void concatMessageList(Str tmp);
void renderFrame(struct UI ui);
void ui_bell();
void ui_printStatus(const char* fmt, ...);
