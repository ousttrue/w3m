#pragma once
#include <Str.h>
#include <wc.h>

extern wc_ces InnerCharset;
extern wc_ces DisplayCharset;

// key input(blocking) or draw require UI
//
// form input
// lineinput
// search
// menu
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
// extern void disp_err_message(char* s, int redraw_current);
// extern void disp_message_nsec(char* s, int redraw_current, int sec, int purge, int mouse);
// extern void disp_message(char* s, int redraw_current);

void concatMessageList(Str tmp);

void renderFrame(struct UI ui);
