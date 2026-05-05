#include "message.h"
#include <w3m.h>
#include "global.h"
#include "screen.h"
#include "wc_util.h"
#include "textlist.h"
#include "indep.h"
#include <stdio.h>

#include "tab.h"
#include "buffer.h"

static const char* delayed_msg = NULL;

static GeneralList* message_list = NULL;

void record_err_message(const char* s)
{
    if (fmInitialized) {
        if (!message_list)
            message_list = newGeneralList();
        if (message_list->nitem >= LINES)
            popValue(message_list);
        pushValue(message_list, allocStr(s, -1));
    }
}

void message(const char* s, int return_x, int return_y)
{
    if (!fmInitialized)
        return;
    sc_move((LINES - 1), 0);
    sc_addnstr(s, COLS - 1);
    sc_clrtoeolx();
    sc_move(return_y, return_x);
}

void disp_message_nsec(const char* s, int redraw_current, int sec, int purge, int mouse)
{
    if (QuietMessage)
        return;
    if (!fmInitialized) {
        fprintf(stderr, "%s\n", conv_to_system(s));
        return;
    }

    if (CurrentTab != NULL && Currentbuf != NULL)
        message(s, Currentbuf->cursorX + Currentbuf->rootX,
            Currentbuf->cursorY + Currentbuf->rootY);
    else
        message(s, (LINES - 1), 0);

    // tty_write_sc();
    // int ch = getch_timeout(sec, args);
    // if (!purge && ch > 0) {
    //     unget(ch);
    // }
    // if (CurrentTab != NULL && Currentbuf != NULL && redraw_current)
    //     displayBuffer(args, B_NORMAL);
}

void disp_message(const char* s, int redraw_current)
{
    disp_message_nsec(s, redraw_current, 10, false, true);
}

void disp_err_message(const char* s, int redraw_current)
{
    record_err_message(s);
    disp_message(s, redraw_current);
}

void set_delayed_message(const char* s)
{
    delayed_msg = allocStr(s, -1);
}

void displayDilayedMessage()
{
    if (delayed_msg != NULL) {
        disp_message(delayed_msg, false);
        delayed_msg = NULL;
        tty_write_sc();
    }
}

struct str_view message_list_panel(void)
{
    Str tmp = Strnew_size(LINES * COLS);
    Strcat_charp(tmp,
        "<html><head><title>List of error messages</title></head><body>"
        "<h1>List of error messages</h1><table cellpadding=0>\n");
    if (message_list)
        for (ListItem* p = message_list->last; p; p = p->prev)
            Strcat_m_charp(tmp, "<tr><td><pre>", html_quote(p->ptr),
                "</pre></td></tr>\n", NULL);
    else
        Strcat_charp(tmp, "<tr><td>(no message recorded)</td></tr>\n");
    Strcat_charp(tmp, "</table></body></html>");
    return (struct str_view) { .ptr = tmp->ptr, .len = tmp->length };
}
