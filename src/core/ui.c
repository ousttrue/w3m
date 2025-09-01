#include "ui.h"
#include "indep.h"
#include "screen.h"
#include "screen_effects.h"
#include "putc.h"
#include "term_renderer.h"
#include "tty.h"
#include "display.h"
#include "buffer.h"
#include <stdarg.h>
#include <wc.h>
#include <wtf.h>

char* CurrentDir = 0;
int CurrentPid = -1;

#define DISPLAY_CHARSET WC_CES_UTF_8
#define SYSTEM_CHARSET WC_CES_UTF_8
#define DOCUMENT_CHARSET WC_CES_UTF_8
wc_ces InnerCharset = WC_CES_WTF; /* Don't change */
wc_ces DisplayCharset = DISPLAY_CHARSET;
// filesystem charset
wc_ces SystemCharset = SYSTEM_CHARSET;
wc_ces DocumentCharset = (DOCUMENT_CHARSET);
wc_ces BookmarkCharset = (SYSTEM_CHARSET);

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

void status(struct UI ui, const char* s)
{
    struct VirtualTerm* vt = ui.vt;
    int row = vt->CurLine;
    int col = vt->CurColumn;
    vt_move(vt, vt->ROWS - 3, 0);
    vt_addnstr(vt, s, vt->COLS - 1);
    vt_clrtoeolx(vt);
    vt_move(vt, row, col);
}

void message(struct UI ui, enum MessageSeverity severity, const char* s)
{
    struct VirtualTerm* vt = ui.vt;
    int row = vt->CurLine;
    int col = vt->CurColumn;
    vt_move(vt, vt->ROWS - 2, 0);
    vt_addnstr(vt, s, vt->COLS - 1);
    vt_clrtoeolx(vt);
    vt_move(vt, row, col);
}

static char* delayed_msg = NULL;
void set_delayed_message(char* s)
{
    delayed_msg = allocStr(s, -1);
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

static char g_status[512];

void ui_printStatus(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsnprintf(g_status, sizeof(g_status), fmt, args);
    va_end(args);
}

void renderFrame(struct UI ui)
{
    int cursorRow = ui.vt->CurLine;
    int cursorCol = ui.vt->CurColumn;

    Buffer* buf = Currentbuf;
    drawAnchorCursor(buf);

    Str msg = make_lastline_message(buf);
    if (buf->firstLine == NULL) {
        /* FIXME: gettextize? */
        Strcat_charp(msg, "\tNo Line");
    }
    // if (delayed_msg != NULL) {
    //     message(getUI(), MSG_INFO, delayed_msg);
    //     delayed_msg = NULL;
    //     // refresh(ttyWriter());
    // }
    vt_standout(ui.vt);
    status(getUI(), g_status);
    message(getUI(), MSG_INFO, msg->ptr);
    vt_standend(ui.vt);
    // term_title(conv_to_system(buf->buffername));
    // refresh(ttyWriter());
    // if (activeImage && displayImage && buf->img && buf->image_loaded) {
    //     drawImage();
    // }
    // if (buf != save_current_buf) {
    //     saveBufferInfo();
    //     save_current_buf = buf;
    // }
    // if (buf->check_url & CHK_URL) {
    //     chkURLBuffer(buf);
    //     renderToScreen();
    // }

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

void ui_cursor_set_x(int x)
{
    if (Currentbuf->firstLine == NULL)
        return;
    while (Currentbuf->currentLine->prev && Currentbuf->currentLine->bpos)
        cursorUp0(Currentbuf, 1);
    Currentbuf->pos = 0;
    arrangeCursor(Currentbuf);
}
