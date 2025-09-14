#include "ui.h"
#include "image.h"
#include "form.h"
#include "keymap.h"
#include "buffer_list.h"
#include "runtime.h"
#include "AnchorList.h"
#include "Anchor.h"
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
#include <stdlib.h>
#include <wc.h>
#include <wtf.h>

char QuietMessage = (false);
int showLineNum = (false);

#define DISPLAY_CHARSET WC_CES_UTF_8
wc_ces DisplayCharset = DISPLAY_CHARSET;
wc_ces BookmarkCharset = (SYSTEM_CHARSET);

const char* url_quote_conv(const char* x, wc_ces c)
{
    return url_quote(wc_conv_strict((x), InnerCharset, (c))->ptr);
}

static struct Int2 viewport_cursor = {
    .x = 0,
    .y = 0,
};

static struct Int2 cursor_delta = {
    .x = 0,
    .y = 0,
};

// struct Int2 cursorDelta()
// {
//     struct Int2 cd = cursor_delta;
//     cursor_delta = (struct Int2) { 0, 0 };
//     return cd;
// }

void cursorUp(int n)
{
    cursorUpDown(-n);
}

void cursorDown(int n)
{
    cursorUpDown(n);
}

void cursorUpDown(int n)
{
    cursor_delta.y += n;
}

void cursorRight(int n)
{
    cursor_delta.x += n;
}

void cursorLeft(int n)
{
    cursor_delta.x -= n;
}

void cursorHome()
{
    cursor_delta.x = 0;
    cursor_delta.y = 0;
}

const char* searchKeyData()
{
    const char* data = NULL;
    if (CurrentKeyData != NULL && *CurrentKeyData != '\0')
        data = CurrentKeyData;
    else if (CurrentCmdData != NULL && *CurrentCmdData != '\0')
        data = CurrentCmdData;
    else if (CurrentKey >= 0)
        data = getKeyData(CurrentKey);
    CurrentKeyData = NULL;
    CurrentCmdData = NULL;
    if (data == NULL || *data == '\0')
        return NULL;
    return allocStr(data, -1);
}

static int
searchKeyNum(void)
{
    int n = 1;
    const char* d = searchKeyData();
    if (d != NULL)
        n = atoi(d);
    return n;
}

struct UI getUI()
{
    int rootX = 0;
    if (showLineNum) {
        if (rootX < 5)
            rootX = 5;
        if (rootX > getScreen()->COLS)
            rootX = getScreen()->COLS;
    }
    int rootY = 0;

    struct VirtualTerm* vt = getScreen();
    struct TermEntry* t = getTermEntry();

    struct UI ui = {
        .current_buffer = Currentbuf,
        .vt = vt,
        .use_graphic = graph_ok(t),
        .viewport = {
            .offset = {
                .x = rootX,
                .y = rootY,
            },
            .size = {
                .x = vt->COLS - rootX,
                .y = vt->ROWS - rootY,
            },
        },
        .viewport_cursor = viewport_cursor,
        .term_cursor = {
            .x = rootX + viewport_cursor.x,
            .y = rootY + viewport_cursor.y,
        },
        .searchkey_num = searchKeyNum(),
    };
    return ui;
}
// short cursorX;
// short cursorY;

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

static Str make_lastline_link(struct Buffer* buf, const char* title, const char* url)
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
    pu = parseUrl(url, baseURL(buf));
    u = parsedURL2Str(&pu);
    if (DecodeURL)
        u = Strnew_charp(url_decode2(u->ptr, buf ? buf->document.charset : 0));
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

static Str make_lastline_message(struct UI ui)
{
    Str s = NULL;
    int sl = 0;
    if (displayLink) {
        struct MapArea* a = retrieveCurrentMapArea(ui);
        if (a)
            s = make_lastline_link(ui.current_buffer, a->alt, a->url);
        else {
            struct Anchor* a = retrieveCurrentAnchor(ui);
            const char* p = NULL;
            if (a && a->title && *a->title)
                p = a->title;
            else {
                struct Anchor* a_img = retrieveCurrentImg(ui);
                if (a_img && a_img->title && *a_img->title)
                    p = a_img->title;
            }
            if (p || a)
                s = make_lastline_link(ui.current_buffer, p, a ? a->url : NULL);
        }
        if (s) {
            sl = get_Str_strwidth(s);
            if (sl >= getScreen()->COLS - 3)
                return s;
        }
    }

    Str msg = Strnew();
    // if (displayLineInfo && currentLine(buf) != NULL && lastLine(buf) != NULL) {
    //     int cl = currentLine(buf)->real_linenumber;
    //     int ll = lastLine(buf)->real_linenumber;
    //     int r = (int)((double)cl * 100.0 / (double)(ll ? ll : 1) + 0.5);
    //     Strcat(msg, Sprintf("%d/%d (%d%%)", cl, ll, r));
    // } else
    Strcat_charp(msg, "Viewing");
    if (ui.current_buffer->ssl_certificate)
        Strcat_charp(msg, "[SSL]");
    Strcat_charp(msg, " <");
    Strcat_charp(msg, ui.current_buffer->document.title);

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
    struct Buffer* buf = ui.current_buffer;
    struct TermEntry* t = getTermEntry();
    // bool use_graphic = graph_ok(t);

    // int cursorRow = ui.vt->CurLine;
    // int cursorCol = ui.vt->CurColumn;

    struct Anchor* a = retrieveCurrentAnchor(ui);
    struct BufferPoint bp = getBufferPosition(ui);
    ui_printStatus("STATUS: (%d, %d), (%d, %d) a(%d, %d=%d) %s",
        // "top=%d key=[%02x > %02x > %02x > %02x > %02x > %02x > %02x > %02x]",
        ui.viewport_cursor.y, ui.viewport_cursor.x,
        bp.line, bp.pos,
        a ? a->start.line : -1,
        a ? a->start.pos : -1,
        a ? a->end.pos : -1,
        a ? a->title : "--"
        // g_keylog[(g_i - 0) % sizeof(g_keylog)],
        // g_keylog[(g_i - 1) % sizeof(g_keylog)],
        // g_keylog[(g_i - 2) % sizeof(g_keylog)],
        // g_keylog[(g_i - 3) % sizeof(g_keylog)],
        // g_keylog[(g_i - 4) % sizeof(g_keylog)],
        // g_keylog[(g_i - 5) % sizeof(g_keylog)],
        // g_keylog[(g_i - 6) % sizeof(g_keylog)],
        // g_keylog[(g_i - 7) % sizeof(g_keylog)]
    );

    // int cursorRow = buf->cursorY;
    // int cursorCol = buf->cursorX;
    drawAnchorCursor(ui);

    Str msg = make_lastline_message(ui);
    if (buf->document.firstLine == NULL) {
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

    MOVE(ttyWriter(), ui.term_cursor.y, ui.term_cursor.x);
    flushWriter(ttyWriter());
}

void ui_bell()
{
    termBell(ttyWriter());
}

void ui_cursor_set_x(int x)
{
    if (Currentbuf->document.firstLine == NULL)
        return;
    while (currentLine(&Currentbuf->document)->prev && currentLine(&Currentbuf->document)->bpos)
        cursorUp(1);
    Currentbuf->document.pos = 0;
}

bool updateCursor(struct Buffer* buf)
{
    bool hasScroll = false;
    struct VirtualTerm* vt = getScreen();

    int x = viewport_cursor.x + cursor_delta.x;
    if (x < 0) {
        // left
        x = 0;
        hasScroll = true;
    } else if (x >= vt->COLS) {
        // right
        x = vt->COLS - 1;
        hasScroll = true;
    }

    int y = viewport_cursor.y + cursor_delta.y;
    if (y < 0) {
        // up
        buf->document.topLineIndex += y;
        y = 0;
        hasScroll = true;
    } else if (y >= vt->ROWS) {
        // down
        buf->document.topLineIndex += (1 + y - vt->ROWS);
        y = vt->ROWS - 1;
        hasScroll = true;
    }

    cursor_delta = (struct Int2) {
        .x = 0,
        .y = 0,
    };
    viewport_cursor = (struct Int2) {
        .x = x,
        .y = y,
    };
    return hasScroll;
}

struct BufferPoint getBufferPosition(struct UI ui)
{
    struct LineList* l = getLine(&ui.current_buffer->document, ui.viewport_cursor.y);
    if (!l) {
        return (struct BufferPoint) { 0, 0 };
    }
    int pos = columnPos(&l->l, ui.viewport_cursor.x);
    return (struct BufferPoint) {
        .line = ui.viewport_cursor.y,
        .pos = pos,
    };
}

struct Anchor*
retrieveCurrentAnchor(struct UI ui)
{
    return retrieveAnchor(ui.current_buffer->document.href, getBufferPosition(ui));
}

struct Anchor*
retrieveCurrentImg(struct UI ui)
{
    return retrieveAnchor(ui.current_buffer->document.img, getBufferPosition(ui));
}

struct Anchor*
retrieveCurrentForm(struct UI ui)
{
    return retrieveAnchor(ui.current_buffer->document.formitem, getBufferPosition(ui));
}

struct Anchor*
retrieveCurrentMap(struct UI ui)
{
    struct Anchor* a = retrieveCurrentForm(ui);
    if (!a || !a->url)
        return NULL;

    struct FormItem* fi = (struct FormItem*)a->url;
    if (fi->parent->method == FORM_METHOD_INTERNAL && !Strcmp_charp(fi->parent->action, "map"))
        return a;
    return NULL;
}

struct MapArea*
retrieveCurrentMapArea(struct UI ui)
{
    struct Anchor* a_img;
    a_img = retrieveCurrentImg(ui);
    if (!(a_img && a_img->image && a_img->image->map))
        return 0;

    struct Anchor* a_form = retrieveCurrentForm(ui);
    if (!(a_form && a_form->url))
        return 0;

    struct FormItem* fi;
    fi = (struct FormItem*)a_form->url;
    if (!(fi && fi->parent && fi->parent->item))
        return 0;
    fi = fi->parent->item;

    struct MapList* ml;
    ml = searchMapList(&ui.current_buffer->document, fi->value ? fi->value->ptr : 0);
    if (!ml)
        return 0;

    int n = searchMapArea(&ui.current_buffer->document, ml, a_img);
    if (n < 0)
        return 0;

    ListItem* al = ml->area->first;
    for (int i = 0; al != 0; i++, al = al->next) {
        struct MapArea* a;
        a = (struct MapArea*)al->ptr;
        if (a && i == n)
            return a;
    }
    return 0;
}
