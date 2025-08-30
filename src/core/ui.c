#include "ui.h"
#include "screen.h"
#include "putc.h"
#include "term_renderer.h"
#include "tty.h"
#include "display.h"
#include "buffer.h"
#include <wc.h>
#include <wtf.h>

char* CurrentDir;
int CurrentPid;

wc_ces InnerCharset = WC_CES_WTF; /* Don't change */
#define DISPLAY_CHARSET WC_CES_UTF_8
wc_ces DisplayCharset = DISPLAY_CHARSET;

Buffer* Currentbuf = 0;
Buffer* Firstbuf = 0;

struct UI getUI()
{
    struct UI ui = {
        .vt = getScreen(),
    };
    return ui;
}

// static GeneralList* message_list = NULL;
//
// void record_err_message(char* s)
// {
//     if (!message_list)
//         message_list = newGeneralList();
//     if (message_list->nitem >= getScreen()->ROWS)
//         popValue(message_list);
//     pushValue(message_list, allocStr(s, -1));
// }

void concatMessageList(Str tmp)
{
    // if (message_list)
    //     for (p = message_list->last; p; p = p->prev)
    //         Strcat_m_charp(tmp, "<tr><td><pre>", html_quote(p->ptr),
    //             "</pre></td></tr>\n", NULL);
    // else
    //     Strcat_charp(tmp, "<tr><td>(no message recorded)</td></tr>\n");
}

void message(struct UI ui, enum MessageSeverity severity, const char* s)
{
    struct VirtualTerm* vt = ui.vt;
    int row = vt->CurLine;
    int col = vt->CurColumn;
    move(vt, vt->ROWS - 1, 0);
    addnstr(vt, s, vt->COLS - 1);
    clrtoeolx(vt);
    move(vt, row, col);
}

// void disp_err_message(char* s, int redraw_current)
// {
//     record_err_message(s);
//     disp_message(s, redraw_current);
// }
//
// void disp_message_nsec(char* s, int redraw_current, int sec, int purge, int mouse)
// {
//     if (QuietMessage)
//         return;
//     if (Currentbuf != NULL)
//         message(s, Currentbuf->cursorX + Currentbuf->rootX,
//             Currentbuf->cursorY + Currentbuf->rootY);
//     else
//         message(s, getScreen()->ROWS - 1, 0);
//     // refresh(ttyWriter());
//     sleep_till_anykey(sec * 1000, purge);
// }
//
// void disp_message(char* s, int redraw_current)
// {
//     disp_message_nsec(s, redraw_current, 10, FALSE, TRUE);
// }

void renderFrame(struct UI ui)
{
    int cursorRow = ui.vt->CurLine;
    int cursorCol = ui.vt->CurColumn;
    struct Frame* frame = screenToFrame(ui.vt);
    wc_putc_init(InnerCharset, DisplayCharset);
    refreshFrame(ttyWriter(), frame);
    wc_putc_end(ttyWriter());
    MOVE(ttyWriter(), cursorRow, cursorCol);
    flushWriter(ttyWriter());
}

void ui_bell()
{
    termBell(ttyWriter());
}
