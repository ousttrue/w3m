#include "message.h"
#include "tab.h"
#include "buffer.h"
#include "terms.h"
#include "file.h"
#include "w3m_rc.h"
#include "textlist.h"
#include "indep.h"

static GeneralList* message_list = NULL;
static char* delayed_msg = NULL;

void displayDelayedMessage()
{
    if (delayed_msg != NULL) {
        disp_message(delayed_msg, FALSE);
        delayed_msg = NULL;
    }
}

void record_err_message(char* s)
{
    if (fmInitialized()) {
        if (!message_list)
            message_list = newGeneralList();
        if (message_list->nitem >= TTY_LINES())
            popValue(message_list);
        pushValue(message_list, allocStr(s, -1));
    }
}

/*
 * List of error messages
 */
struct Buffer*
message_list_panel(void)
{
    Str tmp = Strnew_size(TTY_LINES() * TTY_COLS());
    ListItem* p;

    /* FIXME: gettextize? */
    Strcat_charp(tmp,
        "<html><head><title>List of error messages</title></head><body>"
        "<h1>List of error messages</h1><table cellpadding=0>\n");
    if (message_list)
        for (p = message_list->last; p; p = p->prev)
            Strcat_m_charp(tmp, "<tr><td><pre>", html_quote(p->ptr),
                "</pre></td></tr>\n", NULL);
    else
        Strcat_charp(tmp, "<tr><td>(no message recorded)</td></tr>\n");
    Strcat_charp(tmp, "</table></body></html>");
    return loadHTMLString(tmp);
}

void message(char* s, int return_x, int return_y)
{
    if (!fmInitialized())
        return;
    screen_move(LASTLINE(), 0);
    screen_wc_addstr_width(s, TTY_COLS() - 1);
    screen_clrtoeolx();
    screen_move(return_y, return_x);
}

void disp_err_message(char* s, int redraw_current)
{
    record_err_message(s);
    disp_message(s, redraw_current);
}

void disp_message_nsec(char* s, int redraw_current, int sec, int purge, int mouse)
{
    if (getRuntime()->QuietMessage)
        return;
    if (!fmInitialized()) {
        fprintf(stderr, "%s\n", conv_to_system(s));
        return;
    }
    if (CurrentTab() != NULL && Currentbuf != NULL)
        message(s, Currentbuf->cursorX + Currentbuf->rootX,
            Currentbuf->cursorY + Currentbuf->rootY);
    else
        message(s, LASTLINE(), 0);
}

void disp_message(char* s, int redraw_current)
{
    disp_message_nsec(s, redraw_current, 10, FALSE, TRUE);
}

void set_delayed_message(char* s)
{
    delayed_msg = allocStr(s, -1);
}

