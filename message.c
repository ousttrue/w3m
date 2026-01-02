#include "message.h"
#include "anchor.h"
#include "maparea.h"
#include "myctype.h"
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

void record_err_message(const char* s)
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

void message(const char* s)
{
    if (!fmInitialized())
        return;
    struct Vec2 pos = screen_position();
    screen_move(LASTLINE(), 0);
    screen_wc_addstr_width(s, TTY_COLS() - 1);
    screen_clrtoeolx();
    screen_move(pos.y, pos.x);
}

void disp_err_message(const char* s, int redraw_current)
{
    record_err_message(s);
    disp_message(s, redraw_current);
}

void disp_message_nsec(const char* s, int redraw_current, int sec, int purge, int mouse)
{
    if (getRuntime()->QuietMessage)
        return;
    if (!fmInitialized()) {
        fprintf(stderr, "%s\n", conv_to_system(s));
        return;
    }
    message(s);
    screen_move(LASTLINE(), 0);
}

void disp_message(const char* s, int redraw_current)
{
    disp_message_nsec(s, redraw_current, 10, FALSE, TRUE);
}

void set_delayed_message(const char* s)
{
    delayed_msg = allocStr(s, -1);
}

static Str
make_lastline_link(struct Buffer* buf, const char* title, const char* url)
{
    Str s = NULL, u;
    Lineprop* pr;
    struct Url pu;
    char* p;
    int l = TTY_COLS() - 1, i;

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
    parseURL2(url, &pu, baseURL(buf));
    u = parsedURL2Str(&pu);
    if (getRuntime()->DecodeURL)
        u = Strnew_charp(url_decode2(u->ptr, buf));
    u = checkType(u, &pr, NULL);
    if (l <= 4 || l >= get_Str_strwidth(u)) {
        if (!s)
            return u;
        Strcat(s, u);
        return s;
    }
    if (!s)
        s = Strnew_size(TTY_COLS());
    i = (l - 2) / 2;
    while (i && pr[i] & PC_WCHAR2)
        i--;
    Strcat_charp_n(s, u->ptr, i);
    Strcat_charp(s, "..");
    i = get_Str_strwidth(u) - (TTY_COLS() - 1 - get_Str_strwidth(s));
    while (i < u->length && pr[i] & PC_WCHAR2)
        i++;
    Strcat_charp(s, &u->ptr[i]);
    return s;
}

static Str
make_lastline_message(struct Buffer* buf)
{
    Str msg, s = NULL;
    int sl = 0;

    if (getRuntime()->displayLink) {
        struct MapArea* a = retrieveCurrentMapArea(buf);
        if (a)
            s = make_lastline_link(buf, a->alt, a->url);
        else {
            struct Anchor* a = retrieveCurrentAnchor(buf);
            const char* p = NULL;
            if (a && a->title && *a->title)
                p = a->title;
            else {
                struct Anchor* a_img = retrieveCurrentImg(buf);
                if (a_img && a_img->title && *a_img->title)
                    p = a_img->title;
            }
            if (p || a)
                s = make_lastline_link(buf, p, a ? a->url : NULL);
        }
        if (s) {
            sl = get_Str_strwidth(s);
            if (sl >= TTY_COLS() - 3)
                return s;
        }
    }

    msg = Strnew();
    if (getRuntime()->displayLineInfo && buf->doc.currentLine != NULL && buf->doc.lastLine != NULL) {
        int cl = buf->doc.currentLine->real_linenumber;
        int ll = buf->doc.lastLine->real_linenumber;
        int r = (int)((double)cl * 100.0 / (double)(ll ? ll : 1) + 0.5);
        Strcat(msg, Sprintf("%d/%d (%d%%)", cl, ll, r));
    } else {
        msg = Sprintf("%s", msg->ptr);
    }
    Strcat_charp(msg, "Viewing");
    if (buf->content.ssl_certificate)
        Strcat_charp(msg, "[SSL]");
    Strcat_charp(msg, " <");
    Strcat_charp(msg, buf->doc.title);

    if (s) {
        int l = TTY_COLS() - 3 - sl;
        if (get_Str_strwidth(msg) > l) {

            const char* p;
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

void displayMsg(struct Buffer* buf)
{
    Str msg = make_lastline_message(buf);
    if (buf->doc.firstLine == NULL) {
        Strcat_charp(msg, "\tNo Line");
    }
    displayDelayedMessage();
    screen_standout();
    message(msg->ptr);
    screen_move(buf->doc.cursorY + buf->doc.rootY, buf->doc.cursorX + buf->doc.rootX);
    screen_standend();
    term_title(conv_to_system(buf->doc.title));
}
