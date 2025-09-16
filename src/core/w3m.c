#include "w3m.h"
#include "Buffer.h"
#include "HtmlTagParsed.h"
#include "follow_anchor.h"
#include "Document.h"
#include "TermEntry.h"
#include "buffer_list.h"
#include "cookie.h"
#include "display.h"
#include "downloadlist.h"
#include "form.h"
#include "graphicchar.h"
#include "history.h"
#include "image_loader.h"
#include "keymap.h"
#include "linein.h"
#include "local_cgi.h"
#include "Anchor.h"
#include "AnchorList.h"
#include "MapArea.h"
#include "putc.h"
#include "rc.h"
#include "screen_effects.h"
#include "ssl_util.h"
#include "term_renderer.h"
#include "runtime.h"
#include "buffer_util.h"
#include "funcname1.h"
#include "proxy.h"
#include "term_size.h"
#include "tty.h"
#include "screen.h"

#include "myctype.h"
#include "alloc.h"
#include <wc.h>
#include "wtf.h"

#include <gc/gc.h>

#include <stdarg.h>
#include <errno.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <unistd.h>

#define PACKAGE "w3m"
#define HELP_FILE "w3mhelp-w3m_en.html"
#define BOOKMARK "bookmark.html"

bool g_running = true;

char* mkd_tmp_dir = (NULL);

int use_mark = (false);
int confirm_on_quit = (true);
int CurrentKey;
const char* CurrentKeyData;
const char* CurrentCmdData;

#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 255
#endif

#define DSTR_LEN 256

const char* config_file = (NULL);
char FollowLocale = (true);

struct Hist* LoadHist;
struct Hist* SaveHist;
struct Hist* URLHist;
struct Hist* ShellHist;
struct Hist* TextHist;

static int need_resize_screen = false;

static int display_ok = false;
int prev_key = -1;

static int check_target = true;

char QuietMessage = (false);
int showLineNum = (false);
const char* BookmarkFile = (NULL);

#define DISPLAY_CHARSET WC_CES_UTF_8
wc_ces DisplayCharset = DISPLAY_CHARSET;
wc_ces BookmarkCharset = (SYSTEM_CHARSET);

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
        .document = Currentbuf ? &Currentbuf->document : 0,
        .content = Currentbuf ? &Currentbuf->content : 0,
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
    pu = parseUrl(url, makeBaseUrl(&buf->document));
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

static struct MapArea*
retrieveCurrentMapArea(struct UI ui)
{
    struct Anchor* a_img;
    a_img = retrieveAnchor(ui.current_buffer->document.img, getBufferPosition(ui));
    if (!(a_img && a_img->image && a_img->image->map))
        return 0;

    struct Anchor* a_form = retrieveAnchor(ui.current_buffer->document.formitem, getBufferPosition(ui));
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

static Str make_lastline_message(struct UI ui)
{
    Str s = NULL;
    int sl = 0;
    if (displayLink) {
        struct MapArea* a = retrieveCurrentMapArea(ui);
        if (a)
            s = make_lastline_link(ui.current_buffer, a->alt, a->url);
        else {
            struct Anchor* a = retrieveAnchor(ui.current_buffer->document.href, getBufferPosition(ui));
            const char* p = NULL;
            if (a && a->title && *a->title)
                p = a->title;
            else {
                struct Anchor* a_img = retrieveAnchor(ui.current_buffer->document.href, getBufferPosition(ui));
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
    if (ui.current_buffer->content.ssl_certificate)
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

    struct Anchor* a = retrieveAnchor(ui.current_buffer->document.href, getBufferPosition(ui));
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

/*
 * List of error messages
 */
Str message_list_panel_html()
{
    Str tmp = Strnew();
    Strcat_charp(tmp,
        "<html><head><title>List of error messages</title></head><body>"
        "<h1>List of error messages</h1><table cellpadding=0>\n");
    concatMessageList(tmp);
    Strcat_charp(tmp, "</table></body></html>");
    return tmp;
}

static void*
die_oom(size_t bytes)
{
    fprintf(stderr, "Out of memory: %lu bytes unavailable!\n", (unsigned long)bytes);
    exit(1);
    /*
     * Suppress compiler warning: function might return no value
     * This code is never reached.
     */
    return NULL;
}

static GC_warn_proc orig_GC_warn_proc = NULL;
#define GC_WARN_KEEP_MAX (20)

static void
wrap_GC_warn_proc(char* msg, GC_word arg)
{

    /* *INDENT-OFF* */
    static struct {
        char* msg;
        GC_word arg;
    } msg_ring[GC_WARN_KEEP_MAX];
    /* *INDENT-ON* */
    static int i = 0;
    static int n = 0;
    static int lock = 0;
    int j;

    j = (i + n) % (sizeof(msg_ring) / sizeof(msg_ring[0]));
    msg_ring[j].msg = msg;
    msg_ring[j].arg = arg;

    if (n < sizeof(msg_ring) / sizeof(msg_ring[0]))
        ++n;
    else
        ++i;

    if (!lock) {
        lock = 1;

        for (; n > 0; --n, ++i) {
            i %= sizeof(msg_ring) / sizeof(msg_ring[0]);

            printf(msg_ring[i].msg, (unsigned long)msg_ring[i].arg);
            sleep_till_anykey(1000, 1);
        }

        lock = 0;
    }
}

static const char* currentdir()
{
    char* path = NewAtom_N(char, MAXPATHLEN);
    getcwd(path, MAXPATHLEN);
    return path;
}

void initialize()
{
    wc_uint8 auto_detect;
    if (!getenv("GC_LARGE_ALLOC_WARN_INTERVAL"))
        set_environ("GC_LARGE_ALLOC_WARN_INTERVAL", "30000");
    GC_INIT();
    GC_set_oom_fn(die_oom);
    setlocale(LC_ALL, "");

    NO_proxy_domains = newTextList();
    initDeleteFile();
    CurrentDir = (char*)currentdir();
    CurrentPid = (int)getpid();
    BookmarkFile = NULL;
    config_file = NULL;

    {
        char hostname[HOST_NAME_MAX + 2];
        if (gethostname(hostname, HOST_NAME_MAX + 2) == 0) {
            size_t hostname_len;
            /* Don't use hostname if it is truncated.  */
            hostname[HOST_NAME_MAX + 1] = '\0';
            hostname_len = strlen(hostname);
            if (hostname_len <= HOST_NAME_MAX)
                HostName = allocStr(hostname, (int)hostname_len);
        }
    }

    char* Locale = NULL;
    if (non_null(Locale = getenv("LC_ALL")) || non_null(Locale = getenv("LC_CTYPE")) || non_null(Locale = getenv("LANG"))) {
        DisplayCharset = wc_guess_locale_charset(Locale, DisplayCharset);
        DocumentCharset = wc_guess_locale_charset(Locale, DocumentCharset);
        SystemCharset = wc_guess_locale_charset(Locale, SystemCharset);
    }

    /* initializations */
    init_rc();

    LoadHist = newHist();
    SaveHist = newHist();
    ShellHist = newHist();
    TextHist = newHist();
    URLHist = newHist();

    if (FollowLocale && Locale) {
        DisplayCharset = wc_guess_locale_charset(Locale, DisplayCharset);
        SystemCharset = wc_guess_locale_charset(Locale, SystemCharset);
    }
    // auto_detect = WcOption.auto_detect;
    BookmarkCharset = DocumentCharset;

    char* p;
    if (!non_null(HTTP_proxy) && ((p = getenv("HTTP_PROXY")) || (p = getenv("http_proxy")) || (p = getenv("HTTP_proxy"))))
        HTTP_proxy = p;
    if (!non_null(HTTPS_proxy) && ((p = getenv("HTTPS_PROXY")) || (p = getenv("https_proxy")) || (p = getenv("HTTPS_proxy"))))
        HTTPS_proxy = p;
    if (HTTPS_proxy == NULL && non_null(HTTP_proxy))
        HTTPS_proxy = HTTP_proxy;
    if (!non_null(NO_proxy) && ((p = getenv("NO_PROXY")) || (p = getenv("no_proxy")) || (p = getenv("NO_proxy"))))
        NO_proxy = p;

    if (!non_null(Editor) && (p = getenv("EDITOR")) != NULL)
        Editor = p;

    CurrentKey = -1;
    if (BookmarkFile == NULL)
        BookmarkFile = rcFile(BOOKMARK);

    fmInit();
    // mySignal(SIGWINCH, resize_hook);

    sync_with_option();
    initCookie();
    if (UseHistory)
        loadHistory(URLHist);

    // mySignal(SIGCHLD, sig_chld);
    // mySignal(SIGPIPE, SigPipe);

    orig_GC_warn_proc = GC_get_warn_proc();
    GC_set_warn_proc((void*)wrap_GC_warn_proc);
}

void resetTerm(void)
{
    struct TermEntry* t = getTermEntry();
    writestr(t->op); /* turn off */
    writestr(t->me);
    if (!Do_not_use_ti_te) {
        if (t->te && *t->te)
            writestr(t->te);
        else
            writestr(t->cl);
    }
    writestr(t->se); /* reset terminal */
}

void fmTerm(void)
{
    struct VirtualTerm* vt = getScreen();
    vt_move(vt, getScreen()->ROWS - 1, 0);
    vt_clrtoeolx(vt);
    // refresh(ttyWriter());
    if (activeImage)
        loadImage((struct UI) {}, NULL, IMG_FLAG_STOP, false);
    resetTerm();
    flush_tty();
    TerminalSet(NULL);
    close_tty();
}

static void deleteFiles()
{
    while (Firstbuf) {
        struct Buffer* buf = Firstbuf->nextBuffer;
        discardBuffer(Firstbuf);
        Firstbuf = buf;
    }

    deinitDeleteFile();
}

static void w3m_exit(int i)
{
    stopDownload();
    deleteFiles();
    free_ssl_ctx();
    if (mkd_tmp_dir)
        if (rmdir(mkd_tmp_dir) != 0) {
            fprintf(stderr, "Can't remove temporary directory (%s)!\n", mkd_tmp_dir);
            exit(1);
        }
    exit(i);
}

static void reset_exit_with_value(int _dummy, int rval)
{
    resetTerm();
    flush_tty();
    TerminalSet(NULL);
    close_tty();

    w3m_exit(rval);
}

void reset_error_exit(int _dummy)
{
    reset_exit_with_value(0, 1);
}

void reset_exit(int _dummy)
{
    reset_exit_with_value(0, 0);
}

/*
 * Screen initialize
 */
int initscr(void)
{
    struct TermEntry* t = initTerm();

    if (!t) {
        abort();
    }
    if (t->ti && !Do_not_use_ti_te) {
        writestr(t->ti);
    }
    setgraphchar(t);
    setlinescols(get_tty_fd());
    return 0;
}

/*
 * Initialize routine.
 */
void fmInit(void)
{
    set_tty();
    // set_int();
    initscr();
    struct VirtualTerm* vt = getScreen();
    vt_setupscreen(vt, getLines(), getCols());
    termClear(ttyWriter());
    term_raw();
    term_noecho();
    if (displayImage)
        initImage();
}

static void set_buffer_environ(struct UI ui)
{
    static struct Buffer* prev_buf = NULL;
    static struct LineList* prev_line = NULL;
    static int prev_pos = -1;

    struct Buffer* buf = ui.current_buffer;
    if (buf == NULL)
        return;

    if (buf != prev_buf) {
        set_environ("W3M_SOURCEFILE", buf->content.sourcefile);
        set_environ("W3M_TITLE", buf->document.title);
        set_environ("W3M_URL", parsedURL2Str(&buf->content.url)->ptr);
        set_environ("W3M_TYPE", contentTypeStr(buf->content.cc.content_type));
        set_environ("W3M_CHARSET", wc_ces_to_charset(buf->document.charset));
    }
    struct LineList* l = currentLine(&buf->document);
    if (l && (buf != prev_buf || l != prev_line || buf->document.pos != prev_pos)) {
        struct Anchor* a;
        struct Url pu;
        const char* s = GetWord(buf);
        set_environ("W3M_CURRENT_WORD", s ? s : "");
        a = retrieveAnchor(ui.current_buffer->document.href, getBufferPosition(ui));
        if (a) {
            pu = parseUrl(a->url, makeBaseUrl(&buf->document));
            set_environ("W3M_CURRENT_LINK", parsedURL2Str(&pu)->ptr);
        } else
            set_environ("W3M_CURRENT_LINK", "");
        a = retrieveAnchor(ui.current_buffer->document.img, getBufferPosition(ui));
        if (a) {
            pu = parseUrl(a->url, makeBaseUrl(&buf->document));
            set_environ("W3M_CURRENT_IMG", parsedURL2Str(&pu)->ptr);
        } else
            set_environ("W3M_CURRENT_IMG", "");
        a = retrieveAnchor(ui.current_buffer->document.formitem, getBufferPosition(ui));
        if (a)
            set_environ("W3M_CURRENT_FORM", form2str((struct FormItem*)a->url));
        else
            set_environ("W3M_CURRENT_FORM", "");
        // set_environ("W3M_CURRENT_LINE", Sprintf("%ld", l->real_linenumber)->ptr);
        // set_environ("W3M_CURRENT_COLUMN", Sprintf("%d", buf->currentColumn + buf->cursorX + 1)->ptr);
    } else if (!l) {
        set_environ("W3M_CURRENT_WORD", "");
        set_environ("W3M_CURRENT_LINK", "");
        set_environ("W3M_CURRENT_IMG", "");
        set_environ("W3M_CURRENT_FORM", "");
        set_environ("W3M_CURRENT_LINE", "0");
        set_environ("W3M_CURRENT_COLUMN", "0");
    }
    prev_buf = buf;
    prev_line = l;
    prev_pos = buf->document.pos;
}

static void
save_buffer_position(struct Buffer* buf)
{
    if (!buf->document.firstLine)
        return;

    struct BufferPos* b = buf->undo;
    if (b
        && b->top_linenumber == buf->document.topLineIndex
        && b->cur_linenumber == buf->document.currentLineIndex
        && b->currentColumn == buf->document.currentColumn
        && b->pos == buf->document.pos)
        return;
    b = New(struct BufferPos);
    b->top_linenumber = buf->document.topLineIndex;
    b->cur_linenumber = buf->document.currentLineIndex;
    b->currentColumn = buf->document.currentColumn;
    b->pos = buf->document.pos;
    b->next = NULL;
    b->prev = buf->undo;
    if (buf->undo)
        buf->undo->next = b;
    buf->undo = b;
}

void onKeyInput(unsigned char c)
{
    struct TermEntry* t = getTermEntry();
    bool use_graphic = graph_ok(t);

    static unsigned char g_keylog[] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    static int g_i = 0;

    g_keylog[g_i % sizeof(g_keylog)] = c;
    if (IS_ASCII(c)) { /* Ascii */

        set_buffer_environ(getUI());
        save_buffer_position(getUI().current_buffer);
        {
            CurrentKey = c;
            unsigned char prev = g_keylog[(g_i - 1) % sizeof(g_keylog)];
            CommandFunc func = (prev == 0x1b) ? EscKeymap[c]
                                              : GlobalKeymap[c];
            func(getUI());
        }
        if (updateCursor(getUI().current_buffer)) {
            termClear(ttyWriter());
        }
        bufToScreen(getUI());
        renderFrame(getUI());
    }
    ++g_i;

    prev_key = CurrentKey;
    CurrentKey = -1;
    CurrentKeyData = NULL;
}

void pcmap(void)
{
}

static void
escKeyProc(int c, int esc, unsigned char* map)
{
    if (CurrentKey >= 0 && CurrentKey & K_MULTI) {
        unsigned char** mmap;
        mmap = (unsigned char**)getKeyData(MULTI_KEY(CurrentKey));
        if (!mmap)
            return;
        switch (esc) {
        case K_ESCD:
            map = mmap[3];
            break;
        case K_ESCB:
            map = mmap[2];
            break;
        case K_ESC:
            map = mmap[1];
            break;
        default:
            map = mmap[0];
            break;
        }
        esc |= (CurrentKey & ~0xFFFF);
    }
    CurrentKey = esc | c;
    if (map)
        w3mFuncList[(int)map[c]].func(getUI());
}

// void _quitfm(bool confirm)
// {
//     const char* ans = "y";
//     if (checkDownloadList())
//         /* FIXME: gettextize? */
//         ans = inputChar(getUI(), "Download process retains. "
//                                  "Do you want to exit w3m? (y/n)");
//     else if (confirm)
//         /* FIXME: gettextize? */
//         ans = inputChar(getUI(), "Do you want to exit w3m? (y/n)");
//     if (!(ans && TOLOWER(*ans) == 'y')) {
//
//         return;
//     }
//
//     term_title(""); /* XXX */
//     if (activeImage)
//         termImage();
//     fmTerm();
//     save_cookies();
//     if (UseHistory && SaveURLHist)
//         saveHistory(URLHist, URLHistSize);
//     w3m_exit(0);
// }

//
// TOOD: input blocking on coroutine
//
int exec_cmd(char* cmd)
{
    fmTerm();
    int rv = system(cmd);
    if (rv) {
        printf("\n[Hit any key]");
        fflush(stdout);
        fmInit();
        {
            // GetChFunc getch = event_begin_input(-1);
            // getch();
            // event_end_input(getch);
        }

        return rv;
    }
    fmInit();

    return 0;
}

#include <sys/signalfd.h>
int create_signalfd(void)
{
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    // sigaddset(&mask, SIGTERM);
    // sigaddset(&mask, SIGUSR1);
    // sigaddset(&mask, SIGUSR2);

    sigprocmask(SIG_BLOCK, &mask, NULL);
    return signalfd(-1, &mask, SFD_CLOEXEC);
}

#include <sys/epoll.h>

enum IOBEventType {
    IOB_EVENT_ERROR,
    IOB_EVENT_TIMEOUT,
    IOB_EVENT_INPUT,
    IOB_EVENT_SIGNAL,
};

struct IOBEvent {
    enum IOBEventType type;
    int value;
};

struct IOBlocker {
    int epfd;

    struct epoll_event* event_buffer;
    int event_buffer_len;
    int event_enable;
    int event_pos;

    char input_buffer[128];
    int input_enable;
    int input_pos;

    int signalfd;
};

void iob_init(struct IOBlocker* iob, int max_event)
{
    iob->epfd = epoll_create1(EPOLL_CLOEXEC);

    // event buffer
    iob->event_buffer = malloc(sizeof(struct epoll_event) * max_event);
    iob->event_buffer_len = max_event;
    iob->event_enable = 0;
    iob->event_pos = 0;

    // add STDIN
    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN;
    ev.data.fd = STDIN_FILENO;
    if (epoll_ctl(iob->epfd, EPOLL_CTL_ADD, ev.data.fd, &ev) != 0) {
        perror("epoll_ctl");
        abort();
    }
    iob->input_enable = 0;
    iob->input_pos = 0;

    // add signalfd
    iob->signalfd = create_signalfd();
    if (iob->signalfd == -1) {
        perror("Failed to create signal file descriptor");
        abort();
    }
    ev.events = EPOLLIN;
    ev.data.fd = iob->signalfd;
    if (epoll_ctl(iob->epfd, EPOLL_CTL_ADD, ev.data.fd, &ev) != 0) {
        perror("Failed to add signal file descriptor to epoll");
        abort();
    }
}

void iob_deinit(struct IOBlocker* iob)
{
    close(iob->epfd);
    if (iob->event_buffer) {
        free(iob->event_buffer);
    }
}

struct IOBEvent iob_wait(struct IOBlocker* iob, int timeout_ms)
{
    while (true) {
        if (iob->input_enable && iob->input_pos < iob->input_enable) {
            return (struct IOBEvent) {
                .type = IOB_EVENT_INPUT,
                .value = iob->input_buffer[iob->input_pos++],
            };
        }

        if (iob->event_enable && iob->event_pos < iob->event_enable) {
            struct epoll_event ev = iob->event_buffer[iob->event_pos++];
            if (ev.data.fd == STDIN_FILENO) {
                iob->input_pos = 0;
                iob->input_enable = read(ev.data.fd, iob->input_buffer, sizeof(iob->input_buffer));
                if (iob->input_enable < 0) {
                    // error
                    return (struct IOBEvent) {
                        .type = IOB_EVENT_ERROR,
                        .value = errno,
                    };
                }
            } else if (ev.data.fd == iob->signalfd) {
                struct signalfd_siginfo info = { 0 };
                if (read(iob->signalfd, &info, sizeof(info)) != sizeof(info)) {
                    abort();
                }
                return (struct IOBEvent) {
                    .type = IOB_EVENT_SIGNAL,
                    .value = info.ssi_signo,
                };
            } else {
                // not reach
                abort();
            }
        } else {
            // block
            iob->event_enable = epoll_wait(iob->epfd, iob->event_buffer, iob->event_buffer_len, timeout_ms);
            if (iob->event_enable == 0) {
                // timeout
                return (struct IOBEvent) {
                    .type = IOB_EVENT_TIMEOUT,
                };
            } else if (iob->event_enable < 0) {
                // error / signal
                return (struct IOBEvent) {
                    .type = IOB_EVENT_ERROR,
                    .value = errno,
                };
            } else {
                iob->event_pos = 0;
            }
        }
    }
}

void main_loop(int argc, char** argv)
{
    initialize();
    parseArgs(argc, argv);
    // onFrame();
    onKeyInput(0);

    // init
    struct IOBlocker iob;
    memset(&iob, 0, sizeof(iob));
    iob_init(&iob, 12);

    int timeout_ms = -1;
    while (g_running) {
        struct IOBEvent ev = iob_wait(&iob, timeout_ms);
        switch (ev.type) {
        case IOB_EVENT_ERROR:
            printf("error: %d\n", ev.value);
            g_running = false;
            break;

        case IOB_EVENT_TIMEOUT:
            break;

        case IOB_EVENT_INPUT:
            if (ev.value == 3 // <C-c>
            ) {
                // raw mode
                g_running = false;
            } else {
                // printf("input: %d\n", ev.value);
                onKeyInput(ev.value);
            }
            break;

        case IOB_EVENT_SIGNAL:
            switch (ev.value) {
            case SIGINT:
                g_running = false;
                break;
            }
            break;
        }
    }

    // finalize
    iob_deinit(&iob);

    fmTerm();
}
