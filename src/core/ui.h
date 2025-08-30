#pragma once
#include <Str.h>
#include <wc.h>

extern char* CurrentDir;
extern int CurrentPid;
extern wc_ces InnerCharset;
extern wc_ces DisplayCharset;
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
void concatMessageList(Str tmp);
void renderFrame(struct UI ui);
void ui_bell();
