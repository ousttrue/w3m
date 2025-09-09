#include "ui.h"
#include "quote.h"
#include "w3m.h"
#include "maparea.h"
#include "screen.h"
#include "screen_effects.h"
#include "putc.h"
#include "term_renderer.h"
#include "TermEntry.h"
#include "tty.h"
#include "display.h"
#include "buffer.h"
#include "graphicchar.h"
#include <myctype.h>
#include <math.h>
#include <stdarg.h>
#include <wc.h>
#include <wtf.h>

char QuietMessage = (false);
char* CurrentDir = 0;
int CurrentPid = -1;
int showLineNum = (false);

#define DISPLAY_CHARSET WC_CES_UTF_8
#define SYSTEM_CHARSET WC_CES_UTF_8
wc_ces InnerCharset = WC_CES_WTF; /* Don't change */
wc_ces DisplayCharset = DISPLAY_CHARSET;
// filesystem charset
wc_ces SystemCharset = SYSTEM_CHARSET;
wc_ces BookmarkCharset = (SYSTEM_CHARSET);

Buffer* Currentbuf = 0;
Buffer* Firstbuf = 0;

const char* url_quote_conv(const char* x, wc_ces c)
{
    return url_quote(wc_conv_strict((x), InnerCharset, (c))->ptr);
}

struct UI getUI()
{
    int rootX = 0;
    if (showLineNum) {
        if (Currentbuf->lastLine && Currentbuf->lastLine->real_linenumber > 0)
            rootX = (int)(log(Currentbuf->lastLine->real_linenumber + 0.1)
                        / log(10))
                + 2;
        if (rootX < 5)
            rootX = 5;
        if (rootX > getScreen()->COLS)
            rootX = getScreen()->COLS;
    }
    int rootY = 0;

    struct VirtualTerm* vt = getScreen();
    struct TermEntry* t = getTermEntry();
    struct UI ui = {
        .vt = vt,
        .use_graphic = graph_ok(t),
        .viewport = {
            .x = rootX,
            .y = rootY,
            .cols = vt->COLS - rootX,
            .rows = vt->ROWS - rootY,
        },
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

static Str make_lastline_link(Buffer* buf, char* title, char* url)
{
    Str s = NULL, u;
    struct Url pu;
    char* p;
    int l = getScreen()->COLS - 1, i;

    if (title && *title) {
        s = Strnew_m_charp("[", title, "]", NULL);
        for (p = s->ptr; *p; p++) {
            if (IS_CNTRL(*p) || IS_SPACE(*p))
                *p = ' ';
        }
        if (url)
            Strcat_charp(s, " ");
        l -= get_Str_strwidth(s);
        if (l <= 0)
            return s;
    }
    if (!url)
        return s;
    parseUrl(url, &pu, baseURL(buf));
    u = parsedURL2Str(&pu);
    if (DecodeURL)
        u = Strnew_charp(url_decode2(u->ptr, buf));
    Lineprop* pr;
    u = checkType(u, &pr, NULL);
    if (l <= 4 || l >= get_Str_strwidth(u)) {
        if (!s)
            return u;
        Strcat(s, u);
        return s;
    }
    if (!s)
        s = Strnew_size(getScreen()->COLS);
    i = (l - 2) / 2;
    while (i && pr[i] & PC_WCHAR2)
        i--;
    Strcat_charp_n(s, u->ptr, i);
    Strcat_charp(s, "..");
    i = get_Str_strwidth(u) - (getScreen()->COLS - 1 - get_Str_strwidth(s));
    while (i < u->length && pr[i] & PC_WCHAR2)
        i++;
    Strcat_charp(s, &u->ptr[i]);
    return s;
}

static Str make_lastline_message(Buffer* buf)
{
    Str msg, s = NULL;
    int sl = 0;

    if (displayLink) {
        MapArea* a = retrieveCurrentMapArea(buf);
        if (a)
            s = make_lastline_link(buf, a->alt, a->url);
        else {
            Anchor* a = retrieveCurrentAnchor(buf);
            char* p = NULL;
            if (a && a->title && *a->title)
                p = a->title;
            else {
                Anchor* a_img = retrieveCurrentImg(buf);
                if (a_img && a_img->title && *a_img->title)
                    p = a_img->title;
            }
            if (p || a)
                s = make_lastline_link(buf, p, a ? a->url : NULL);
        }
        if (s) {
            sl = get_Str_strwidth(s);
            if (sl >= getScreen()->COLS - 3)
                return s;
        }
    }

    msg = Strnew();
    if (displayLineInfo && buf->currentLine != NULL && buf->lastLine != NULL) {
        int cl = buf->currentLine->real_linenumber;
        int ll = buf->lastLine->real_linenumber;
        int r = (int)((double)cl * 100.0 / (double)(ll ? ll : 1) + 0.5);
        Strcat(msg, Sprintf("%d/%d (%d%%)", cl, ll, r));
    } else
        /* FIXME: gettextize? */
        Strcat_charp(msg, "Viewing");
    if (buf->ssl_certificate)
        Strcat_charp(msg, "[SSL]");
    Strcat_charp(msg, " <");
    Strcat_charp(msg, buf->buffername);

    if (s) {
        int l = getScreen()->COLS - 3 - sl;
        if (get_Str_strwidth(msg) > l) {
            char* p;
            for (p = msg->ptr; *p; p += get_mclen(p)) {
                l -= get_mcwidth(p);
                if (l < 0)
                    break;
            }
            l = p - msg->ptr;
            Strtruncate(msg, l);
        }
        Strcat_charp(msg, "> ");
        Strcat(msg, s);
    } else {
        Strcat_charp(msg, ">");
    }
    return msg;
}

void renderFrame(struct UI ui)
{
    struct TermEntry* t = getTermEntry();
    // bool use_graphic = graph_ok(t);

    // int cursorRow = ui.vt->CurLine;
    // int cursorCol = ui.vt->CurColumn;

    Buffer* buf = Currentbuf;
    int cursorRow = buf->cursorY;
    int cursorCol = buf->cursorX;
    drawAnchorCursor(ui, buf);

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
