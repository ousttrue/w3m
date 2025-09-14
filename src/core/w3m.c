#include "w3m.h"
#include "follow_anchor.h"
#include "Document.h"
#include "HttpRequest.h"
#include "KeyValue.h"
#include "LinkList.h"
#include "TermEntry.h"
#include "buffer_list.h"
#include "cookie.h"
#include "display.h"
#include "downloadlist.h"
#include "form.h"
#include "graphicchar.h"
#include "history.h"
#include "html_quote.h"
#include "http_message.h"
#include "image.h"
#include "keymap.h"
#include "linein.h"
#include "local_cgi.h"
#include "maparea.h"
#include "menu.h"
#include "myctype.h"
#include "platform.h"
#include "Anchor.h"
#include "AnchorList.h"
#include "progress.h"
#include "quote.h"
#include "rc.h"
#include "search.h"
#include "ssl_util.h"
#include "str_util.h"
#include "term_renderer.h"
#include "ucs.h"
#include "ui.h"
#include "alloc.h"
#include "runtime.h"
#include "defun_macro.h"
#include "Content.h"
#include <gc/gc.h>
#include <locale.h>
#include <stdlib.h>
#include "buffer.h"
#include "funcname1.h"
#include "proxy.h"
#include "term_size.h"
#include "tty.h"
#include "screen.h"
#include <wc.h>
#include "util.h"
#include "wtf.h"
#include <event_poller.h>
#include <string.h>
#include <sys/param.h>
#include <unistd.h>
#include "mailcap.h"
#include "defun.h"
#include "../defun.h"
#include "regex.h"
#include "buffer_loader.h"

#define PACKAGE "w3m"
#define HELP_FILE "w3mhelp-w3m_en.html"
#define BOOKMARK "bookmark.html"

char* mkd_tmp_dir = (NULL);

int DefaultURLString = (DEFAULT_URL_CURRENT);
int UseDictCommand = (true);
char* DictCommand = ("file:///$LIB/w3mdict" CGI_EXTENSION);
const char* BookmarkFile = (NULL);
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

typedef struct _Event {
    int cmd;
    void* data;
    struct _Event* next;
} Event;
static Event* CurrentEvent = NULL;
static Event* LastEvent = NULL;

static AlarmEvent DefaultAlarm = {
    0, AL_UNSET, FUNCNAME_nulcmd, NULL
};
static AlarmEvent* CurrentAlarm = &DefaultAlarm;
static void SigAlarm(int _dummy);

static int need_resize_screen = false;
void resize_hook(int _dummy);

static char* getCurWord(struct Buffer* buf, int* spos, int* epos);

static int display_ok = false;
int prev_key = -1;

void set_buffer_environ(struct Buffer*);
static void save_buffer_position(struct Buffer* buf);

static int check_target = true;

/*
 * List of error messages
 */
static struct Content message_list_panel(struct UI ui)
{
    Str tmp = Strnew_size(getScreen()->ROWS * getScreen()->COLS);
    Strcat_charp(tmp,
        "<html><head><title>List of error messages</title></head><body>"
        "<h1>List of error messages</h1><table cellpadding=0>\n");
    concatMessageList(tmp);
    Strcat_charp(tmp, "</table></body></html>");
    return (struct Content) {
        .url = {},
        .page = tmp,
        .cc = {
            .content_type = CONTENTTYPE_TEXT_HTML,
            .charset = WC_CES_UTF_8,
        },
    };
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

// static void
// sig_chld(int signo)
// {
//     int p_stat;
//     pid_t pid;
//
//     while ((pid = waitpid(-1, &p_stat, WNOHANG)) > 0) {
//         DownloadList* d;
//
//         if (WIFEXITED(p_stat)) {
//             for (d = FirstDL; d != NULL; d = d->next) {
//                 if (d->pid == pid) {
//                     d->err = WEXITSTATUS(p_stat);
//                     break;
//                 }
//             }
//         }
//     }
//     mySignal(SIGCHLD, sig_chld);
// }

// static void
// SigPipe(int _dummy)
// {
//     mySignal(SIGPIPE, SigPipe);
// }

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

#include <libintl.h>
#define _(String) gettext(String)
#define N_(String) (String)

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
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);

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
        loadImage(NULL, IMG_FLAG_STOP, false);
    resetTerm();
    flush_tty();
    TerminalSet(NULL);
    close_tty();
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

// void error_dump(int _dummy)
// {
//     mySignal(SIGIOT, SIG_DFL);
//     resetTerm();
//     flush_tty();
//     TerminalSet(NULL);
//     close_tty();
//
//     abort();
// }

// void set_int(void)
// {
//     mySignal(SIGHUP, reset_exit);
//     mySignal(SIGINT, reset_exit);
//     mySignal(SIGQUIT, reset_exit);
//     mySignal(SIGTERM, reset_exit);
//     mySignal(SIGILL, error_dump);
//     mySignal(SIGIOT, error_dump);
//     mySignal(SIGFPE, error_dump);
// #ifdef SIGBUS
//     mySignal(SIGBUS, error_dump);
// #endif /* SIGBUS */
//     /* mySignal(SIGSEGV, error_dump); */
// }

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

// static struct Buffer* loadNormalBuf(struct UI ui, struct Buffer* buf)
// {
//     pushBuffer(ui, buf);
//     return buf;
// }

static void
resize_screen(void)
{
    need_resize_screen = false;
    setlinescols(get_tty_fd());
    vt_setupscreen(getScreen(), getLines(), getCols());
    vt_clear(getScreen());
}

bool onFrame()
{
    struct UI ui = getUI();

    struct TermEntry* t = getTermEntry();
    bool use_graphic = graph_ok(t);

    updateDownload();
    if (ui.current_buffer->submit) {
        struct Anchor* a = ui.current_buffer->submit;
        ui.current_buffer->submit = NULL;
        gotoLine(&ui.current_buffer->document, a->start.line);
        ui.current_buffer->pos = a->start.pos;
        _followForm(ui, true, false);
        return false;
    }
    /* event processing */
    if (CurrentEvent) {
        CurrentKey = -1;
        CurrentKeyData = NULL;
        CurrentCmdData = (char*)CurrentEvent->data;
        w3mFuncList[CurrentEvent->cmd].func(ui);

        if (updateCursor(getUI().current_buffer)) {
        }
        termClear(ttyWriter());
        bufToScreen(ui);
        renderFrame(ui);

        CurrentCmdData = NULL;
        CurrentEvent = CurrentEvent->next;
        return false;
    }
    /* get keypress event */
    if (ui.current_buffer->event) {
        if (ui.current_buffer->event->status != AL_UNSET) {
            CurrentAlarm = ui.current_buffer->event;
            if (CurrentAlarm->sec == 0) { /* refresh (0sec) */
                ui.current_buffer->event = NULL;
                CurrentKey = -1;
                CurrentKeyData = NULL;
                CurrentCmdData = (char*)CurrentAlarm->data;
                w3mFuncList[CurrentAlarm->cmd].func(ui);

                if (updateCursor(getUI().current_buffer)) {
                }
                termClear(ttyWriter());
                bufToScreen(ui);
                renderFrame(ui);

                CurrentCmdData = NULL;
                return false;
            }
        } else
            ui.current_buffer->event = NULL;
    }
    if (!ui.current_buffer->event)
        CurrentAlarm = &DefaultAlarm;
    // if (CurrentAlarm->sec > 0) {
    //     mySignal(SIGALRM, SigAlarm);
    //     alarm(CurrentAlarm->sec);
    // }

    // mySignal(SIGWINCH, resize_hook);
    if (activeImage && displayImage && ui.current_buffer->document.img && !ui.current_buffer->image_loaded) {
        loadImage(ui.current_buffer, IMG_FLAG_NEXT, false);
        bufToScreen(ui);
        renderFrame(ui);
        // continue;
    }
    if (need_resize_screen) {
        resize_screen();
        bufToScreen(ui);
        renderFrame(ui);
    }

    return true;
}

void onKeyInput(unsigned char c)
{
    struct TermEntry* t = getTermEntry();
    bool use_graphic = graph_ok(t);

    static unsigned char g_keylog[] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    static int g_i = 0;

    if (CurrentAlarm->sec > 0) {
        alarm(0);
    }

    g_keylog[g_i % sizeof(g_keylog)] = c;
    if (IS_ASCII(c)) { /* Ascii */

        set_buffer_environ(getUI().current_buffer);
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

void pushEvent(int cmd, void* data)
{
    Event* event;

    event = New(Event);
    event->cmd = cmd;
    event->data = data;
    event->next = NULL;
    if (CurrentEvent)
        LastEvent->next = event;
    else
        CurrentEvent = event;
    LastEvent = event;
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

/* movLW, movRW */
/*
 * From: Takashi Nishimoto <g96p0935@mse.waseda.ac.jp> Date: Mon, 14 Jun
 * 1999 09:29:56 +0900
 */

static wc_uint32
getChar(const char* p)
{
    return wc_any_to_ucs(wtf_parse1((wc_uchar**)&p));
}

/* Go to specified line */
void _goLine(struct UI ui, const char* l)
{
    if (l == NULL || *l == '\0' || currentLine(&ui.current_buffer->document) == NULL) {

        return;
    }
    ui.current_buffer->pos = 0;
    if (*l == '^') {
        ui.current_buffer->document.topLineIndex = ui.current_buffer->document.currentLineIndex = ui.current_buffer->document.firstLine->linenumber;
    } else if (*l == '$') {
        ui.current_buffer->document.topLineIndex = ui.current_buffer->document.allLine - (getScreen()->ROWS + 1) / 2;
        ui.current_buffer->document.currentLineIndex = lastLine(&ui.current_buffer->document)->linenumber;
    }
    // else
    //     gotoRealLine(ui.current_buffer, atoi(l));
}

/* follow HREF link in the buffer */
static void bufferA(struct UI ui)
{
    followAnchor(ui, false);
}

/* process form */
void followForm(struct UI ui)
{
    _followForm(ui, false, false);
}

/* go to the next left/right anchor */
static void
nextX(struct UI ui, int d, int dy)
{
    struct HmarkerList* hl = ui.current_buffer->document.hmarklist;
    struct Anchor *an, *pan;
    int i, x, y, n = ui.searchkey_num;

    if (ui.current_buffer->document.firstLine == NULL)
        return;
    if (!hl || hl->nmark == 0)
        return;

    an = retrieveCurrentAnchor(ui.current_buffer);
    if (an == NULL)
        an = retrieveCurrentForm(ui.current_buffer);

    struct LineList* l;
    l = currentLine(&ui.current_buffer->document);
    x = ui.current_buffer->pos;
    y = l->linenumber;
    pan = NULL;
    for (i = 0; i < n; i++) {
        if (an)
            x = (d > 0) ? an->end.pos : an->start.pos - 1;
        an = NULL;
        while (1) {
            for (; x >= 0 && x < l->l.len; x += d) {
                struct BufferPoint bp = { .line = y, .pos = x };
                an = retrieveAnchor(ui.current_buffer->document.href, bp);
                if (!an)
                    an = retrieveAnchor(ui.current_buffer->document.formitem, bp);
                if (an) {
                    pan = an;
                    break;
                }
            }
            if (!dy || an)
                break;
            l = (dy > 0) ? l->next : l->prev;
            if (!l)
                break;
            x = (d > 0) ? 0 : l->l.len - 1;
            y = l->linenumber;
        }
        if (!an)
            break;
    }

    if (pan == NULL)
        return;
    gotoLine(&ui.current_buffer->document, y);
    ui.current_buffer->pos = pan->start.pos;
}

/* go to the next downward/upward anchor */
static void
nextY(struct UI ui, int d)
{
    struct HmarkerList* hl = ui.current_buffer->document.hmarklist;
    struct Anchor *an, *pan;
    int i, x, y, n = ui.searchkey_num;
    int hseq;

    if (ui.current_buffer->document.firstLine == NULL)
        return;
    if (!hl || hl->nmark == 0)
        return;

    an = retrieveCurrentAnchor(ui.current_buffer);
    if (an == NULL)
        an = retrieveCurrentForm(ui.current_buffer);

    x = ui.current_buffer->pos;
    y = currentLine(&ui.current_buffer->document)->linenumber + d;
    pan = NULL;
    hseq = -1;
    for (i = 0; i < n; i++) {
        if (an)
            hseq = abs(an->hseq);
        an = NULL;
        for (; y >= 0 && y <= lastLine(&ui.current_buffer->document)->linenumber; y += d) {
            struct BufferPoint bp = { .line = y, .pos = x };
            an = retrieveAnchor(ui.current_buffer->document.href, bp);
            if (!an)
                an = retrieveAnchor(ui.current_buffer->document.formitem, bp);
            if (an && hseq != abs(an->hseq)) {
                pan = an;
                break;
            }
        }
        if (!an)
            break;
    }

    if (pan == NULL)
        return;
    gotoLine(&ui.current_buffer->document, pan->start.line);
}

/* go to the next left anchor */
DEFUN(nextL, NEXT_LEFT, "Move left to the next hyperlink")
{
    nextX(ui, -1, 0);
}

/* go to the next left-up anchor */
DEFUN(nextLU, NEXT_LEFT_UP, "Move left or upward to the next hyperlink")
{
    nextX(ui, -1, -1);
}

/* go to the next right anchor */
DEFUN(nextR, NEXT_RIGHT, "Move right to the next hyperlink")
{
    nextX(ui, 1, 0);
}

/* go to the next right-down anchor */
DEFUN(nextRD, NEXT_RIGHT_DOWN, "Move right or downward to the next hyperlink")
{
    nextX(ui, 1, 1);
}

/* go to the next downward anchor */
DEFUN(nextD, NEXT_DOWN, "Move downward to the next hyperlink")
{
    nextY(ui, 1);
}

/* go to the next upward anchor */
DEFUN(nextU, NEXT_UP, "Move upward to the next hyperlink")
{
    nextY(ui, -1);
}

/* go to the next bufferr */
DEFUN(nextBf, NEXT, "Switch to the next buffer")
{
    setCurrentBuffer(prevBuffer(Firstbuf, ui.current_buffer));
}

/* go to the previous bufferr */
DEFUN(prevBf, PREV, "Switch to the previous buffer")
{
    setCurrentBuffer(ui.current_buffer->nextBuffer);
}

static int
checkBackBuffer(struct Buffer* buf)
{
    if (buf->nextBuffer)
        return true;

    return false;
}

/* delete current buffer and back to the previous buffer */
DEFUN(backBf, BACK, "Close current buffer and return to the one below in stack")
{
    if (!checkBackBuffer(ui.current_buffer)) {
        /* FIXME: gettextize? */
        message(getUI(), MSG_INFO, "Can't go back...");
        return;
    }

    delBuffer(ui, ui.current_buffer);
}

DEFUN(deletePrevBuf, DELETE_PREVBUF, "Delete previous buffer (mainly for local CGI-scripts)")
{
    struct Buffer* buf = ui.current_buffer->nextBuffer;
    if (buf)
        delBuffer(ui, buf);
}

/* go to specified URL */
static void
goURL0(struct UI ui, char* prompt, int relative)
{
    const char* url;
    const char* referer;
    struct Url p_url, *current;
    struct Buffer* cur_buf = ui.current_buffer;
    const int* no_referer_ptr;

    url = searchKeyData();
    if (url == NULL) {
        struct Hist* hist = copyHist(URLHist);
        struct Anchor* a;

        current = baseURL(ui.current_buffer);
        if (current) {
            char* c_url = parsedURL2Str(current)->ptr;
            if (DefaultURLString == DEFAULT_URL_CURRENT)
                url = url_decode2(c_url, 0);
            else
                pushHist(hist, c_url);
        }
        a = retrieveCurrentAnchor(ui.current_buffer);
        if (a) {
            char* a_url;
            p_url = parseUrl(a->url, current);
            a_url = parsedURL2Str(&p_url)->ptr;
            if (DefaultURLString == DEFAULT_URL_LINK)
                url = url_decode2(a_url, ui.current_buffer->document.charset);
            else
                pushHist(hist, a_url);
        }
        url = inputLineHist(getUI(), prompt, url, IN_URL, hist);
        if (url != NULL)
            SKIP_BLANKS(url);
    }
    if (relative) {
        current = baseURL(ui.current_buffer);
        if ((no_referer_ptr && *no_referer_ptr) || current == NULL || current->scheme == SCM_LOCAL || current->scheme == SCM_LOCAL_CGI || current->scheme == SCM_DATA)
            referer = NO_REFERER;
        else
            referer = parsedURL2RefererStr(&ui.current_buffer->content.url)->ptr;
        url = url_quote(url);
    } else {
        current = NULL;
        referer = NULL;
        url = url_quote(url);
    }
    if (url == NULL || *url == '\0') {

        return;
    }
    if (*url == '#') {
        gotoLabel(ui, url + 1);
        return;
    }
    p_url = parseUrl(url, current);

    pushHashHist(URLHist, parsedURL2Str(&p_url)->ptr);
    struct Content c = loadGeneralFile(url, current, NULL, referer, UI_TTY);
    pushContent(ui, c);
    if (ui.current_buffer != cur_buf) /* success */
        pushHashHist(URLHist, parsedURL2Str(&ui.current_buffer->content.url)->ptr);
}

DEFUN(goURL, GOTO, "Open specified document in a new buffer")
{
    goURL0(ui, "Goto URL: ", false);
}

DEFUN(goHome, GOTO_HOME, "Open home page in a new buffer")
{
    const char* url;
    if ((url = getenv("HTTP_HOME")) != NULL || (url = getenv("WWW_HOME")) != NULL) {
        struct Url p_url;
        struct Buffer* cur_buf = ui.current_buffer;
        SKIP_BLANKS(url);
        url = url_quote(url);
        p_url = parseUrl(url, NULL);
        pushHashHist(URLHist, parsedURL2Str(&p_url)->ptr);
        struct Content c = loadGeneralFile(url, NULL, NULL, NULL, UI_TTY);
        pushContent(ui, c);
        if (ui.current_buffer != cur_buf) /* success */
            pushHashHist(URLHist, parsedURL2Str(&ui.current_buffer->content.url)->ptr);
    }
}

DEFUN(gorURL, GOTO_RELATIVE, "Go to relative address")
{
    goURL0(ui, "Goto relative URL: ", true);
}

/* load bookmark */
DEFUN(ldBmark, BOOKMARK VIEW_BOOKMARK, "View bookmarks")
{
    struct Content c = loadGeneralFile(BookmarkFile, NULL, NULL, NO_REFERER, UI_TTY);
    pushContent(ui, c);
}

#define W3MBOOKMARK_CMDNAME "w3mbookmark"
// #define W3MBOOKMARK_CMDNAME "w3mbookmark.exe"

/* Add current to bookmark */
DEFUN(adBmark, ADD_BOOKMARK, "Add current page to bookmarks")
{
    Str tmp = Sprintf("mode=panel&cookie=%s&bmark=%s&url=%s&title=%s"
                      "&charset=%s",
        (Str_form_quote(localCookie()))->ptr,
        (Str_form_quote(Strnew_charp(BookmarkFile)))->ptr,
        (Str_form_quote(parsedURL2Str(&ui.current_buffer->content.url)))->ptr,
        (Str_form_quote(wc_conv_strict(ui.current_buffer->document.title,
             InnerCharset,
             BookmarkCharset)))
            ->ptr,
        wc_ces_to_charset(BookmarkCharset));
    struct Form* post = newFormList(NULL, "post", NULL, NULL, NULL, NULL, NULL);
    post->body = tmp->ptr;
    post->length = tmp->length;
    struct Content c = loadGeneralFile("file:///$LIB/" W3MBOOKMARK_CMDNAME, NULL, post, NO_REFERER, UI_TTY);
    pushContent(ui, c);
}

/* option setting */
DEFUN(ldOpt, OPTIONS, "Display options setting panel")
{
    struct Content c = load_option_panel(ui);
    pushContent(ui, c);
}

/* set an option */
DEFUN(setOpt, SET_OPTION, "Set option")
{
    CurrentKeyData = NULL; /* not allowed in w3m-control: */

    const char* opt = searchKeyData();
    if (opt == NULL || *opt == '\0' || strchr(opt, '=') == NULL) {
        if (opt != NULL && *opt != '\0') {
            const char* v = get_param_option(opt);
            opt = Sprintf("%s=%s", opt, v ? v : "")->ptr;
        }
        opt = inputStrHist(getUI(), "Set option: ", opt, TextHist);
        if (opt == NULL || *opt == '\0') {

            return;
        }
    }
    if (set_param_option(opt))
        sync_with_option();
}

/* error message list */
DEFUN(msgs, MSGS, "Display error messages")
{
    struct Content c = message_list_panel(ui);
    pushContent(ui, c);
}

/* page info */
DEFUN(pginfo, INFO, "Display information about the current document")
{
    struct Content c = page_info_panel(ui, ui.current_buffer);
    pushContent(ui, c);
}

void follow_map(struct UI ui, struct KeyValue* arg)
{
    const char* name = tag_get_value(arg, "link");
    int x, y;
    struct Url p_url;

    struct Anchor* an;
    an = retrieveCurrentImg(ui.current_buffer);
    // x = ui.current_buffer->cursorX;
    // y = ui.current_buffer->cursorY;
    MapArea* a;
    a = follow_map_menu(ui, ui.current_buffer, name, an, x, y);
    if (a == NULL || a->url == NULL || *(a->url) == '\0') {
        return;
    }
    if (*(a->url) == '#') {
        gotoLabel(ui, a->url + 1);
        return;
    }
    p_url = parseUrl(a->url, baseURL(ui.current_buffer));
    pushHashHist(URLHist, parsedURL2Str(&p_url)->ptr);
    struct Content c = loadGeneralFile(a->url, baseURL(ui.current_buffer),
        NULL, parsedURL2Str(&ui.current_buffer->content.url)->ptr, UI_TTY);
    pushContent(ui, c);
}

/* link menu */
DEFUN(linkMn, LINK_MENU, "Pop up link element menu")
{
    struct LinkList* l = link_menu(ui);
    struct Url p_url;

    if (!l || !l->url)
        return;
    if (*(l->url) == '#') {
        gotoLabel(ui, l->url + 1);
        return;
    }
    p_url = parseUrl(l->url, baseURL(ui.current_buffer));
    pushHashHist(URLHist, parsedURL2Str(&p_url)->ptr);
    struct Content c = loadGeneralFile(l->url, baseURL(ui.current_buffer),
        NULL, parsedURL2Str(&ui.current_buffer->content.url)->ptr, UI_TTY);
    pushContent(ui, c);
}

typedef struct Anchor* (*AnchorMenuFunc)(struct UI ui, struct Buffer*);

static void
anchorMn(struct UI ui, AnchorMenuFunc menu_func, int go)
{
    if (!ui.current_buffer->document.href || !ui.current_buffer->document.hmarklist)
        return;

    struct Anchor* a = menu_func(ui, ui.current_buffer);
    if (!a || a->hseq < 0)
        return;

    struct BufferPoint* po = &ui.current_buffer->document.hmarklist->marks[a->hseq];
    gotoLine(&ui.current_buffer->document, po->line);
    ui.current_buffer->pos = po->pos;

    if (go)
        followAnchor(ui, false);
}

/* accesskey */
DEFUN(accessKey, ACCESSKEY, "Pop up accesskey menu")
{
    anchorMn(ui, accesskey_menu, true);
}

/* list menu */
DEFUN(listMn, LIST_MENU, "Pop up menu for hyperlinks to browse to")
{
    anchorMn(ui, list_menu, true);
}

DEFUN(movlistMn, MOVE_LIST_MENU, "Pop up menu to navigate between hyperlinks")
{
    anchorMn(ui, list_menu, false);
}

/* link,anchor,image list */
DEFUN(linkLst, LIST, "Show all URLs referenced")
{
    struct Content c = link_list_panel(ui, ui.current_buffer);
    pushContent(ui, c);
}

/* cookie list */
DEFUN(cooLst, COOKIE, "View cookie list")
{
    struct Content c = cookie_list_panel(ui);
    pushContent(ui, c);
}

/* History page */
DEFUN(ldHist, HISTORY, "Show browsing history")
{
    struct Content c = historyBuffer(ui, URLHist);
    pushContent(ui, c);
}

/* download HREF link */
DEFUN(svA, SAVE_LINK, "Save hyperlink target")
{
    CurrentKeyData = NULL; /* not allowed in w3m-control: */
    followAnchor(ui, true);
}

/* download IMG link */
DEFUN(svI, SAVE_IMAGE, "Save inline image")
{
    CurrentKeyData = NULL; /* not allowed in w3m-control: */
    followImage(ui, true);
}

/* save buffer */
DEFUN(svBuf, PRINT SAVE_SCREEN, "Save rendered document")
{
    CurrentKeyData = NULL; /* not allowed in w3m-control: */
    const char* file = searchKeyData();
    const char* qfile = NULL;
    if (file == NULL || *file == '\0') {
        /* FIXME: gettextize? */
        qfile = inputLineHist(getUI(), "Save buffer to: ", NULL, IN_COMMAND, SaveHist);
        if (qfile == NULL || *qfile == '\0') {

            return;
        }
    }
    file = conv_to_system(qfile ? qfile : file);
    bool is_pipe;
    FILE* f;
    if (*file == '|') {
        is_pipe = true;
        f = popen(file + 1, "w");
    } else {
        if (qfile) {
            file = unescape_spaces(Strnew_charp(qfile))->ptr;
            file = conv_to_system(file);
        }
        file = expandPath(file);
        if (!notExistsOrOverWrite(file)) {
            return;
        }
        f = fopen(file, "w");
        is_pipe = false;
    }
    if (f == NULL) {
        /* FIXME: gettextize? */
        char* emsg = Sprintf("Can't open %s", conv_from_system(file))->ptr;
        message(getUI(), MSG_ERR, emsg);
        return;
    }
    saveBuffer(ui.current_buffer, f, true);
    if (is_pipe)
        pclose(f);
    else
        fclose(f);
}

int _doFileCopy(const char* tmpf, const char* defstr, int download)
{
    // Str msg;
    // // Str filen;
    // char *p, *q = NULL;
    // pid_t pid;
    // char* lock;
    // struct stat st;
    // long long size = 0;
    // bool is_pipe = false;
    //
    // // if (fmInitialized)
    // {
    //     p = searchKeyData();
    //     if (p == NULL || *p == '\0') {
    //         /* FIXME: gettextize? */
    //         q = inputLineHist(getUI(), "(Download)Save file to: ",
    //             defstr, IN_COMMAND, SaveHist);
    //         if (q == NULL || *q == '\0')
    //             return false;
    //         p = conv_to_system(q);
    //     }
    //     if (*p == '|' && PermitSaveToPipe)
    //         is_pipe = true;
    //     else {
    //         if (q) {
    //             p = unescape_spaces(Strnew_charp(q))->ptr;
    //             p = conv_to_system(p);
    //         }
    //         p = expandPath(p);
    //         if (!notExistsOrOverWrite(p))
    //             return -1;
    //     }
    //     if (!canCopyFile(tmpf, p)) {
    //         msg = Sprintf("Can't copy. %s and %s are identical.",
    //             conv_from_system(tmpf), conv_from_system(p));
    //         message(getUI(), MSG_ERR, msg->ptr);
    //         return -1;
    //     }
    //     if (!download) {
    //         if (_MoveFile(tmpf, p) < 0) {
    //             /* FIXME: gettextize? */
    //             msg = Sprintf("Can't save to %s", conv_from_system(p));
    //             message(getUI(), MSG_ERR, msg->ptr);
    //         }
    //         return -1;
    //     }
    //     lock = tmpfname(TMPF_DFL, ".lock")->ptr;
    //
    //     symlink(p, lock);
    //
    //     flush_tty();
    //     pid = fork();
    //     if (!pid) {
    //         setup_child(false, 0, -1);
    //         if (!_MoveFile(tmpf, p) && PreserveTimestamp && !is_pipe && !stat(tmpf, &st))
    //             setModtime(p, st.st_mtime);
    //         unlink(lock);
    //         exit(0);
    //     }
    //     if (!stat(tmpf, &st))
    //         size = st.st_size;
    //     addDownloadList(pid, conv_from_system(tmpf), p, lock, size);
    // }
    //
    // // else {
    // //     q = searchKeyData();
    // //     if (q == NULL || *q == '\0') {
    // //         /* FIXME: gettextize? */
    // //         printf("(Download)Save file to: ");
    // //         fflush(stdout);
    // //         filen = Strfgets(stdin);
    // //         if (filen->length == 0)
    // //             return -1;
    // //         q = filen->ptr;
    // //     }
    // //     for (p = q + strlen(q) - 1; IS_SPACE(*p); p--)
    // //         ;
    // //     *(p + 1) = '\0';
    // //     if (*q == '\0')
    // //         return -1;
    // //     p = q;
    // //     if (*p == '|' && PermitSaveToPipe)
    // //         is_pipe = true;
    // //     else {
    // //         p = expandPath(p);
    // //         if (!notExistsOrOverWrite(p))
    // //             return -1;
    // //     }
    // //     if (checkCopyFile(tmpf, p) < 0) {
    // //         /* FIXME: gettextize? */
    // //         printf("Can't copy. %s and %s are identical.", tmpf, p);
    // //         return -1;
    // //     }
    // //     if (_MoveFile(tmpf, p) < 0) {
    // //         /* FIXME: gettextize? */
    // //         printf("Can't save to %s\n", p);
    // //         return -1;
    // //     }
    // //     if (PreserveTimestamp && !is_pipe && !stat(tmpf, &st))
    // //         setModtime(p, st.st_mtime);
    // // }
    return 0;
}
inline static int doFileCopy(const char* tmpf, const char* defstr)
{
    return _doFileCopy(tmpf, defstr, false);
}
int doFileMove(const char* tmpf, const char* defstr)
{
    int ret = doFileCopy(tmpf, defstr);
    unlink(tmpf);
    return ret;
}

/* save source */
DEFUN(svSrc, DOWNLOAD SAVE, "Save document source")
{
    if (ui.current_buffer->content.sourcefile == NULL)
        return;
    CurrentKeyData = NULL; /* not allowed in w3m-control: */
    PermitSaveToPipe = true;
    // if (ui.current_buffer->real_scheme == SCM_LOCAL)
    //     file = conv_from_system(guessSaveName(NULL, ui.current_buffer->content.url.real_file));
    // else
    const char* file = guessSaveName(ui.current_buffer->document_header, ui.current_buffer->content.url.file);
    doFileCopy(ui.current_buffer->content.sourcefile, file);
    PermitSaveToPipe = false;
}

static void
_peekURL(struct UI ui, int only_img)
{

    struct Anchor* a;
    struct Url pu;
    static Str s = NULL;
    static Lineprop* p = NULL;
    Lineprop* pp;
    static int offset = 0, n;

    if (ui.current_buffer->document.firstLine == NULL)
        return;
    if (CurrentKey == prev_key && s != NULL) {
        if (s->length - offset >= getCols())
            offset++;
        else if (s->length <= offset) /* bug ? */
            offset = 0;
        goto disp;
    } else {
        offset = 0;
    }
    s = NULL;
    a = (only_img ? NULL : retrieveCurrentAnchor(ui.current_buffer));
    if (a == NULL) {
        a = (only_img ? NULL : retrieveCurrentForm(ui.current_buffer));
        if (a == NULL) {
            a = retrieveCurrentImg(ui.current_buffer);
            if (a == NULL)
                return;
        } else
            s = Strnew_charp(form2str((struct FormItem*)a->url));
    }
    if (s == NULL) {
        pu = parseUrl(a->url, baseURL(ui.current_buffer));
        s = parsedURL2Str(&pu);
    }
    if (DecodeURL)
        s = Strnew_charp(url_decode2(s->ptr, ui.current_buffer->document.charset));
    s = checkType(s, &pp, NULL);
    p = NewAtom_N(Lineprop, s->length);
    memcpy((void*)p, (void*)pp, s->length * sizeof(Lineprop));
disp:
    n = ui.searchkey_num;
    if (n > 1 && s->length > (n - 1) * (getCols() - 1))
        offset = (n - 1) * (getCols() - 1);
    while (offset < s->length && p[offset] & PC_WCHAR2)
        offset++;
    message(getUI(), MSG_INFO, &s->ptr[offset]);
}

/* peek URL */
DEFUN(peekURL, PEEK_LINK, "Show target address")
{
    _peekURL(ui, 0);
}

/* peek URL of image */
DEFUN(peekIMG, PEEK_IMG, "Show image address")
{
    _peekURL(ui, 1);
}

/* show current URL */
static Str
currentURL(struct UI ui)
{
    if (!ui.current_buffer
        // || ui.current_buffer->bufferprop & BP_INTERNAL
    )
        return Strnew_size(0);
    return parsedURL2Str(&ui.current_buffer->content.url);
}

DEFUN(curURL, PEEK, "Show current address")
{
    static Str s = NULL;
    static Lineprop* p = NULL;
    Lineprop* pp;
    static int offset = 0, n;

    // if (ui.current_buffer->bufferprop & BP_INTERNAL)
    //     return;
    if (CurrentKey == prev_key && s != NULL) {
        if (s->length - offset >= getCols())
            offset++;
        else if (s->length <= offset) /* bug ? */
            offset = 0;
    } else {
        offset = 0;
        s = currentURL(ui);
        if (DecodeURL)
            s = Strnew_charp(url_decode2(s->ptr, 0));
        s = checkType(s, &pp, NULL);
        p = NewAtom_N(Lineprop, s->length);
        memcpy(p, pp, s->length * sizeof(Lineprop));
    }
    n = ui.searchkey_num;
    if (n > 1 && s->length > (n - 1) * (getCols() - 1))
        offset = (n - 1) * (getCols() - 1);
    while (offset < s->length && p[offset] & PC_WCHAR2)
        offset++;
    message(getUI(), MSG_INFO, &s->ptr[offset]);
}
/* view HTML source */

DEFUN(vwSrc, SOURCE VIEW, "Toggle between HTML shown or processed")
{
    if (ui.current_buffer->content.cc.content_type == CONTENTTYPE_UNKNOWN) {
        return;
    }
    if (ui.current_buffer->content.sourcefile == NULL) {
        return;
    }

    struct Buffer* buf = newBuffer();

    if (ui.current_buffer->content.cc.content_type == CONTENTTYPE_TEXT_HTML) {
        buf->content.cc.content_type = CONTENTTYPE_TEXT_PLAIN;
        buf->document.title = Sprintf("source of %s", ui.current_buffer->document.title)->ptr;
    } else if (ui.current_buffer->content.cc.content_type == CONTENTTYPE_TEXT_PLAIN) {
        buf->content.cc.content_type = CONTENTTYPE_TEXT_HTML;
        buf->document.title = Sprintf("HTML view of %s", ui.current_buffer->document.title)->ptr;
    } else {
        return;
    }
    buf->content.url = ui.current_buffer->content.url;
    buf->filename = ui.current_buffer->filename;
    buf->content.sourcefile = ui.current_buffer->content.sourcefile;
    buf->document.charset = ui.current_buffer->document.charset;
    buf->clone = ui.current_buffer->clone;
    (*buf->clone)++;

    pushBuffer(ui, buf);
}

/* reload */
DEFUN(reload, RELOAD, "Load current document anew")
{
    // if (ui.current_buffer->bufferprop & BP_INTERNAL) {
    //     if (!strcmp(ui.current_buffer->document.title, DOWNLOAD_LIST_TITLE)) {
    //         ldDL(ui);
    //         return;
    //     }
    //     /* FIXME: gettextize? */
    //     message(getUI(), MSG_ERR, "Can't reload...");
    //     return;
    // }
    if (ui.current_buffer->content.url.scheme == SCM_LOCAL && !strcmp(ui.current_buffer->content.url.file, "-")) {
        /* file is std input */
        /* FIXME: gettextize? */
        message(getUI(), MSG_ERR, "Can't reload stdin");
        return;
    }

    int multipart = 0;

    struct Form* post;
    if (ui.current_buffer->form_submit) {
        post = ui.current_buffer->form_submit->parent;
        if (post->method == FORM_METHOD_POST
            && post->enctype == FORM_ENCTYPE_MULTIPART) {
            Str query;
            struct stat st;
            multipart = 1;
            query_from_followform(ui, &query, ui.current_buffer->form_submit, multipart);
            stat(post->body, &st);
            post->length = st.st_size;
        }
    } else {
        post = NULL;
    }
    Str url = parsedURL2Str(&ui.current_buffer->content.url);
    message(getUI(), MSG_INFO, "Reloading...");
    // refresh(ttyWriter());
    wc_ces old_charset = DocumentCharset;
    if (ui.current_buffer->document.charset != WC_CES_US_ASCII)
        DocumentCharset = ui.current_buffer->document.charset;
    // SearchHeader = ui.current_buffer->search_header;
    DefaultType = contentTypeStr(ui.current_buffer->content.cc.content_type);
    struct Content c = loadGeneralFile(url->ptr, NULL, post, NO_REFERER, UI_TTY /*, true*/);

    struct Buffer* buf = makeBuffer(ui, &c);
    DocumentCharset = old_charset;
    // SearchHeader = false;
    DefaultType = NULL;

    if (multipart)
        unlink(post->body);
    if (buf == NULL) {
        /* FIXME: gettextize? */
        message(getUI(), MSG_ERR, "Can't reload...");
        return;
    } else if (buf) {

        return;
    }

    // struct Buffer *fbuf = NULL;
    // if (fbuf != NULL)
    //     Firstbuf = deleteBuffer(Firstbuf, fbuf);
    repBuffer(ui, ui.current_buffer, buf);
    // if ((buf->content.cc.content_type == CONTENTTYPE_TEXT_PLAIN && sbuf.content.cc.content_type == CONTENTTYPE_TEXT_HTML)
    //     || (buf->content.cc.content_type == CONTENTTYPE_TEXT_HTML && sbuf.content.cc.content_type == CONTENTTYPE_TEXT_PLAIN)) {
    //     vwSrc(ui);
    //     if (ui.current_buffer != buf)
    //         Firstbuf = deleteBuffer(Firstbuf, buf);
    // }
    // ui.current_buffer->form_submit = sbuf.form_submit;
    if (ui.current_buffer->document.firstLine) {
        // COPY_BUFROOT(ui.current_buffer, &sbuf);
        // restorePosition(ui.current_buffer, &sbuf);
    }
}

/* reshape */
DEFUN(reshape, RESHAPE, "Re-render document")
{
    ui.current_buffer->width = 0;
}

static void
_docCSet(struct UI ui, wc_ces charset)
{
    // if (ui.current_buffer->bufferprop & BP_INTERNAL)
    //     return;
    if (ui.current_buffer->content.sourcefile == NULL) {
        message(getUI(), MSG_INFO, "Can't reload...");
        return;
    }
    ui.current_buffer->document.charset = charset;
}

void change_charset(struct UI ui, struct KeyValue* arg)
{
    abort();
    // struct Buffer* buf = ui.current_buffer->linkBuffer[LB_N_INFO];
    // if (buf == NULL)
    //     return;
    // delBuffer(ui, ui.current_buffer);
    // ui.current_buffer = buf;
    // if (ui.current_buffer->bufferprop & BP_INTERNAL)
    //     return;
    // wc_ces charset;
    // charset = ui.current_buffer->document.charset;
    // for (; arg; arg = arg->next) {
    //     if (!strcmp(arg->arg, "charset"))
    //         charset = atoi(arg->value);
    // }
    // _docCSet(ui, charset);
}

DEFUN(docCSet, CHARSET, "Change the character encoding for the current document")
{
    const char* cs = searchKeyData();
    if (cs == NULL || *cs == '\0')
        /* FIXME: gettextize? */
        cs = inputStr(getUI(), "Document charset: ",
            wc_ces_to_charset(ui.current_buffer->document.charset));
    wc_ces charset = wc_guess_charset_short(cs, 0);
    if (charset == 0) {
        return;
    }
    _docCSet(ui, charset);
}

DEFUN(defCSet, DEFAULT_CHARSET, "Change the default character encoding")
{
    const char* cs = searchKeyData();
    if (cs == NULL || *cs == '\0')
        /* FIXME: gettextize? */
        cs = inputStr(getUI(), "Default document charset: ",
            wc_ces_to_charset(DocumentCharset));
    wc_ces charset = wc_guess_charset_short(cs, 0);
    if (charset != 0)
        DocumentCharset = charset;
}

/* mark URL-like patterns as anchors */
void chkURLBuffer(struct Buffer* buf)
{
    static char* url_like_pat[] = {
        "https?://[a-zA-Z0-9][a-zA-Z0-9:%\\-\\./?=~_\\&+@#,\\$;]*[a-zA-Z0-9_/=\\-]",
        "file:/[a-zA-Z0-9:%\\-\\./=_\\+@#,\\$;]*",
        "ftp://[a-zA-Z0-9][a-zA-Z0-9:%\\-\\./=_+@#,\\$]*[a-zA-Z0-9_/]",
#ifndef USE_W3MMAILER /* see also chkExternalURIBuffer() */
        "mailto:[^<> 	][^<> 	]*@[a-zA-Z0-9][a-zA-Z0-9\\-\\._]*[a-zA-Z0-9]",
#endif
        "https?://[a-zA-Z0-9:%\\-\\./_@]*\\[[a-fA-F0-9:][a-fA-F0-9:\\.]*\\][a-zA-Z0-9:%\\-\\./?=~_\\&+@#,\\$;]*",
        "ftp://[a-zA-Z0-9:%\\-\\./_@]*\\[[a-fA-F0-9:][a-fA-F0-9:\\.]*\\][a-zA-Z0-9:%\\-\\./=_+@#,\\$]*",
        NULL
    };
    for (int i = 0; url_like_pat[i]; i++) {
        reAnchor(buf, url_like_pat[i]);
    }
    buf->check_url = true;
}

DEFUN(chkURL, MARK_URL, "Turn URL-like strings into hyperlinks")
{
    chkURLBuffer(ui.current_buffer);
}

DEFUN(chkWORD, MARK_WORD, "Turn current word into hyperlink")
{
    char* p;
    int spos, epos;
    p = getCurWord(ui.current_buffer, &spos, &epos);
    if (p == NULL)
        return;
    reAnchorWord(ui.current_buffer, currentLine(&ui.current_buffer->document), spos, epos);
}

/* show current line number and number of lines in the entire document */
// DEFUN(curlno, LINE_INFO, "Display current position in document")
// {
//     struct Line* l = currentLine(&ui.current_buffer->document);
//     Str tmp;
//     int cur = 0, all = 0, col = 0, len = 0;
//
//     if (l != NULL) {
//         cur = l->real_linenumber;
//         col = l->bwidth + ui.current_buffer->currentColumn + ui.current_buffer->cursorX + 1;
//         while (l->next && l->next->bpos)
//             l = l->next;
//         if (l->width < 0)
//             l->width = COLPOS(l, l->len);
//         len = l->bwidth + l->width;
//     }
//     if (lastLine(&ui.current_buffer->document))
//         all = lastLine(&ui.current_buffer->document)->real_linenumber;
//     tmp = Sprintf("line %d/%d (%d%%) col %d/%d", cur, all,
//         (int)((double)cur * 100.0 / (double)(all ? all : 1)
//             + 0.5),
//         col, len);
//     Strcat_charp(tmp, "  ");
//     Strcat_charp(tmp, wc_ces_to_charset_desc(ui.current_buffer->document_charset));
//
//     message(getUI(), MSG_INFO, tmp->ptr);
// }

DEFUN(dispI, DISPLAY_IMAGE, "Restart loading and drawing of images")
{
    if (!displayImage)
        initImage();
    if (!activeImage)
        return;
    displayImage = true;
    /*
     * if (!(ui.current_buffer->type && is_html_type(ui.current_buffer->type)))
     * return;
     */
    ui.current_buffer->image_flag = IMG_FLAG_AUTO;
}

DEFUN(stopI, STOP_IMAGE, "Stop loading and drawing of images")
{
    if (!activeImage)
        return;
    /*
     * if (!(ui.current_buffer->type && is_html_type(ui.current_buffer->type)))
     * return;
     */
    ui.current_buffer->image_flag = IMG_FLAG_SKIP;
}

DEFUN(dispVer, VERSION, "Display the version of w3m")
{
    message(getUI(), MSG_INFO, Sprintf("w3m version %s", w3m_version)->ptr);
}

DEFUN(wrapToggle, WRAP_TOGGLE, "Toggle wrapping mode in searches")
{
    if (WrapSearch) {
        WrapSearch = false;
        /* FIXME: gettextize? */
        message(getUI(), MSG_INFO, "Wrap search off");
    } else {
        WrapSearch = true;
        /* FIXME: gettextize? */
        message(getUI(), MSG_INFO, "Wrap search on");
    }
}

static char*
getCurWord(struct Buffer* buf, int* spos, int* epos)
{
    char* p;
    struct LineList* l = currentLine(&buf->document);
    int b, e;

    *spos = 0;
    *epos = 0;
    if (l == NULL)
        return NULL;
    p = l->l.lineBuf;
    e = buf->pos;
    while (e > 0 && !wc_is_ucs_alnum(getChar(&p[e])))
        e = prevChar(e, &l->l);
    if (!wc_is_ucs_alnum(getChar(&p[e])))
        return NULL;
    b = e;
    while (b > 0) {
        int tmp = b;
        tmp = prevChar(tmp, &l->l);
        if (!wc_is_ucs_alnum(getChar(&p[tmp])))
            break;
        b = tmp;
    }
    while (e < l->l.len && wc_is_ucs_alnum(getChar(&p[e])))
        e = nextChar(e, &l->l);
    *spos = b;
    *epos = e;
    return &p[b];
}

static char*
GetWord(struct Buffer* buf)
{
    int b, e;
    char* p;

    if ((p = getCurWord(buf, &b, &e)) != NULL) {
        return Strnew_charp_n(p, e - b)->ptr;
    }
    return NULL;
}

#define DICTBUFFERNAME "*dictionary*"

static void
execdict(struct UI ui, const char* word)
{
    if (!UseDictCommand || word == NULL || *word == '\0') {
        return;
    }

    const char* w = conv_to_system(word);
    if (*w == '\0') {
        return;
    }

    const char* dictcmd = Sprintf("%s?%s", DictCommand, Str_form_quote(Strnew_charp(w))->ptr)->ptr;
    struct Content c = loadGeneralFile(dictcmd, NULL, NULL, NO_REFERER, UI_TTY);
    pushContent(ui, c);
}

DEFUN(dictword, DICT_WORD, "Execute dictionary command (see README.dict)")
{
    execdict(ui, inputStr(getUI(), "(dictionary)!", ""));
}

DEFUN(dictwordat, DICT_WORD_AT,
    "Execute dictionary command for word at cursor")
{
    execdict(ui, GetWord(ui.current_buffer));
}

void set_buffer_environ(struct Buffer* buf)
{
    static struct Buffer* prev_buf = NULL;
    static struct LineList* prev_line = NULL;
    static int prev_pos = -1;

    if (buf == NULL)
        return;

    if (buf != prev_buf) {
        set_environ("W3M_SOURCEFILE", buf->content.sourcefile);
        set_environ("W3M_FILENAME", buf->filename);
        set_environ("W3M_TITLE", buf->document.title);
        set_environ("W3M_URL", parsedURL2Str(&buf->content.url)->ptr);
        set_environ("W3M_TYPE", contentTypeStr(buf->content.cc.content_type));
        set_environ("W3M_CHARSET", wc_ces_to_charset(buf->document.charset));
    }
    struct LineList* l = currentLine(&buf->document);
    if (l && (buf != prev_buf || l != prev_line || buf->pos != prev_pos)) {
        struct Anchor* a;
        struct Url pu;
        char* s = GetWord(buf);
        set_environ("W3M_CURRENT_WORD", s ? s : "");
        a = retrieveCurrentAnchor(buf);
        if (a) {
            pu = parseUrl(a->url, baseURL(buf));
            set_environ("W3M_CURRENT_LINK", parsedURL2Str(&pu)->ptr);
        } else
            set_environ("W3M_CURRENT_LINK", "");
        a = retrieveCurrentImg(buf);
        if (a) {
            pu = parseUrl(a->url, baseURL(buf));
            set_environ("W3M_CURRENT_IMG", parsedURL2Str(&pu)->ptr);
        } else
            set_environ("W3M_CURRENT_IMG", "");
        a = retrieveCurrentForm(buf);
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
    prev_pos = buf->pos;
}

void deleteFiles()
{
    while (Firstbuf) {
        struct Buffer* buf = Firstbuf->nextBuffer;
        discardBuffer(Firstbuf);
        Firstbuf = buf;
    }

    deinitDeleteFile();
}

void w3m_exit(int i)
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

DEFUN(execCmd, COMMAND, "Invoke w3m function(s)")
{
    CurrentKeyData = NULL; /* not allowed in w3m-control: */
    const char* data = searchKeyData();
    if (data == NULL || *data == '\0') {
        data = inputStrHist(getUI(), "command [; ...]: ", "", TextHist);
        if (data == NULL) {

            return;
        }
    }
    /* data: FUNC [DATA] [; FUNC [DATA] ...] */
    while (*data) {
        SKIP_BLANKS(data);
        if (*data == ';') {
            data++;
            continue;
        }
        const char* p = getWord(&data);
        CommandFunc func = getFunc(p);
        p = getQWord(&data);
        CurrentKey = -1;
        CurrentKeyData = NULL;
        CurrentCmdData = *p ? p : NULL;
        func(ui);
        CurrentCmdData = NULL;
    }
}

static void SigAlarm(int _dummy)
{
    struct UI ui = getUI();
    if (CurrentAlarm->sec > 0) {
        CurrentKey = -1;
        CurrentKeyData = NULL;
        char* data;
        CurrentCmdData = data = (char*)CurrentAlarm->data;
        w3mFuncList[CurrentAlarm->cmd].func(getUI());
        CurrentCmdData = NULL;
        if (CurrentAlarm->status == AL_IMPLICIT_ONCE) {
            CurrentAlarm->sec = 0;
            CurrentAlarm->status = AL_UNSET;
        }
        if (ui.current_buffer->event) {
            if (ui.current_buffer->event->status != AL_UNSET)
                CurrentAlarm = ui.current_buffer->event;
            else
                ui.current_buffer->event = NULL;
        }
        if (!ui.current_buffer->event)
            CurrentAlarm = &DefaultAlarm;
        // if (CurrentAlarm->sec > 0) {
        //     mySignal(SIGALRM, SigAlarm);
        //     alarm(CurrentAlarm->sec);
        // }
    }
}

DEFUN(setAlarm, ALARM, "Set alarm")
{
    CurrentKeyData = NULL; /* not allowed in w3m-control: */
    const char* data = searchKeyData();
    if (data == NULL || *data == '\0') {
        data = inputStrHist(getUI(), "(Alarm)sec command: ", "", TextHist);
        if (data == NULL) {

            return;
        }
    }
    CommandFunc cmd = NULL;
    int sec = 0;
    if (*data) {
        sec = atoi(getWord(&data));
        if (sec > 0)
            cmd = getFunc(getWord(&data));
    }
    // if (cmd >= 0)
    {
        data = getQWord(&data);
        // TODO:
        // setAlarmEvent(&DefaultAlarm, sec, AL_EXPLICIT, cmd, data);
        // message(getUI(), MSG_INFO, Sprintf("%dsec %s %s", sec, w3mFuncList[cmd].id, data)->ptr);
    }
    // else {
    //     setAlarmEvent(&DefaultAlarm, 0, AL_UNSET, FUNCNAME_nulcmd, NULL);
    // }
}

AlarmEvent*
setAlarmEvent(AlarmEvent* event, int sec, short status, int cmd, void* data)
{
    if (event == NULL)
        event = New(AlarmEvent);
    event->sec = sec;
    event->status = status;
    event->cmd = cmd;
    event->data = data;
    return event;
}

DEFUN(reinit, REINIT, "Reload configuration file")
{
    const char* resource = searchKeyData();
    if (resource == NULL) {
        init_rc();
        sync_with_option();
        initCookie();
        return;
    }

    if (!strcasecmp(resource, "CONFIG") || !strcasecmp(resource, "RC")) {
        init_rc();
        sync_with_option();

        return;
    }

    if (!strcasecmp(resource, "COOKIE")) {
        initCookie();
        return;
    }

    if (!strcasecmp(resource, "KEYMAP")) {
        initKeymap(true);
        return;
    }

    if (!strcasecmp(resource, "MAILCAP")) {
        initMailcap();
        return;
    }

    if (!strcasecmp(resource, "MENU")) {
        initMenu();
        return;
    }

    if (!strcasecmp(resource, "MIMETYPES")) {
        // initMimeTypes();
        return;
    }

    message(getUI(), MSG_ERR, Sprintf("Don't know how to reinitialize '%s'", resource)->ptr);
}

DEFUN(defKey, DEFINE_KEY, "Define a binding between a key stroke combination and a command")
{
    CurrentKeyData = NULL; /* not allowed in w3m-control: */
    const char* data = searchKeyData();
    if (data == NULL || *data == '\0') {
        data = inputStrHist(getUI(), "Key definition: ", "", TextHist);
        if (data == NULL || *data == '\0') {

            return;
        }
    }
    setKeymap(allocStr(data, -1), -1);
}

static char*
convert_size3(long long size)
{
    Str tmp = Strnew();
    int n;

    do {
        n = size % 1000;
        size /= 1000;
        tmp = Sprintf(size ? ",%.3d%s" : "%d%s", n, tmp->ptr);
    } while (size);
    return tmp->ptr;
}

static struct Content DownloadListBuffer(struct UI ui)
{
    if (!FirstDL)
        return (struct Content) {};

    DownloadList* d;
    Str src = NULL;
    struct stat st;
    time_t cur_time;
    int duration, rate, eta;
    size_t size;

    cur_time = time(0);
    /* FIXME: gettextize? */
    src = Strnew_charp("<html><head><title>" DOWNLOAD_LIST_TITLE
                       "</title></head>\n<body><h1 align=center>" DOWNLOAD_LIST_TITLE "</h1>\n"
                       "<form method=internal action=download><hr>\n");
    for (d = LastDL; d != NULL; d = d->prev) {
        if (lstat(d->lock, &st))
            d->running = false;
        Strcat_charp(src, "<pre>\n");
        Strcat(src, Sprintf("%s\n  --&gt; %s\n  ", html_quote(d->url), html_quote(conv_from_system(d->save))));
        duration = cur_time - d->time;
        if (!stat(d->save, &st)) {
            size = st.st_size;
            if (!d->running) {
                if (!d->err)
                    d->size = size;
                duration = st.st_mtime - d->time;
            }
        } else
            size = 0;
        if (d->size) {
            int i, l = getCols() - 6;
            if (size < d->size)
                i = 1.0 * l * size / d->size;
            else
                i = l;
            l -= i;
            while (i-- > 0)
                Strcat_char(src, '#');
            while (l-- > 0)
                Strcat_char(src, '_');
            Strcat_char(src, '\n');
        }
        if ((d->running || d->err) && size < d->size)
            Strcat(src, Sprintf("  %s / %s bytes (%d%%)", convert_size3(size), convert_size3(d->size), (int)(100.0 * size / d->size)));
        else
            Strcat(src, Sprintf("  %s bytes loaded", convert_size3(size)));
        if (duration > 0) {
            rate = size / duration;
            Strcat(src, Sprintf("  %02d:%02d:%02d  rate %s/sec", duration / (60 * 60), (duration / 60) % 60, duration % 60, convert_size(rate, 1)));
            if (d->running && size < d->size && rate) {
                eta = (d->size - size) / rate;
                Strcat(src, Sprintf("  eta %02d:%02d:%02d", eta / (60 * 60), (eta / 60) % 60, eta % 60));
            }
        }
        Strcat_char(src, '\n');
        if (!d->running) {
            Strcat(src, Sprintf("<input type=submit name=ok%d value=OK>", d->pid));
            switch (d->err) {
            case 0:
                if (size < d->size)
                    Strcat_charp(src, " Download ended but probably not complete");
                else
                    Strcat_charp(src, " Download complete");
                break;
            case 1:
                Strcat_charp(src, " Error: could not open destination file");
                break;
            case 2:
                Strcat_charp(src, " Error: could not write to file (disk full)");
                break;
            default:
                Strcat_charp(src, " Error: unknown reason");
            }
        } else
            Strcat(src, Sprintf("<input type=submit name=stop%d value=STOP>", d->pid));
        Strcat_charp(src, "\n</pre><hr>\n");
    }
    Strcat_charp(src, "</form></body></html>");

    return (struct Content) {
        .page = src,
        .cc = {
            .content_type = CONTENTTYPE_TEXT_HTML,
            .charset = WC_CES_UTF_8,
        },
    };
}

void download_action(struct UI ui, struct KeyValue* arg)
{
    DownloadList* d;
    pid_t pid;

    for (; arg; arg = arg->next) {
        if (!strncmp(arg->arg, "stop", 4)) {
            pid = (pid_t)atoi(&arg->arg[4]);
            kill(pid, SIGKILL);
        } else if (!strncmp(arg->arg, "ok", 2))
            pid = (pid_t)atoi(&arg->arg[2]);
        else
            continue;
        for (d = FirstDL; d; d = d->next) {
            if (d->pid == pid) {
                unlink(d->lock);
                if (d->prev)
                    d->prev->next = d->next;
                else
                    FirstDL = d->next;
                if (d->next)
                    d->next->prev = d->prev;
                else
                    LastDL = d->prev;
                break;
            }
        }
    }
    ldDL(getUI());
}

void stopDownload(void)
{
    DownloadList* d;

    if (!FirstDL)
        return;
    for (d = FirstDL; d != NULL; d = d->next) {
        if (!d->running)
            continue;
        kill(d->pid, SIGKILL);
        unlink(d->lock);
    }
}

/* download panel */
DEFUN(ldDL, DOWNLOAD_LIST, "Display downloads panel")
{
    int replace = false;
    // if (ui.current_buffer->bufferprop & BP_INTERNAL && !strcmp(ui.current_buffer->document.title, DOWNLOAD_LIST_TITLE))
    //     replace = true;
    if (!FirstDL) {
        if (replace) {
            if (ui.current_buffer == Firstbuf && ui.current_buffer->nextBuffer == NULL) {
            } else
                delBuffer(ui, ui.current_buffer);
        }
        return;
    }
    int reload = checkDownloadList();

    struct Content c = DownloadListBuffer(ui);
    if (!c.page) {
        return;
    }
    // buf->bufferprop |= (BP_INTERNAL | BP_NO_URL);
    if (replace) {
        // COPY_BUFROOT(buf, ui.current_buffer);
        // restorePosition(buf, ui.current_buffer);
    }
    pushContent(ui, c);
    if (replace)
        deletePrevBuf(ui);
    if (reload)
        ui.current_buffer->event = setAlarmEvent(ui.current_buffer->event, 1, AL_IMPLICIT,
            FUNCNAME_reload, NULL);
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
        && b->currentColumn == buf->currentColumn
        && b->pos == buf->pos)
        return;
    b = New(struct BufferPos);
    b->top_linenumber = buf->document.topLineIndex;
    b->cur_linenumber = buf->document.currentLineIndex;
    b->currentColumn = buf->currentColumn;
    b->pos = buf->pos;
    b->next = NULL;
    b->prev = buf->undo;
    if (buf->undo)
        buf->undo->next = b;
    buf->undo = b;
}

static void
resetPos(struct UI ui, struct BufferPos* b)
{
    struct Buffer buf = {
        .document = {
            .topLineIndex = b->top_linenumber,
            .currentLineIndex = b->cur_linenumber,
        },
        .pos = b->pos,
        .currentColumn = b->currentColumn,
    };
    restorePosition(ui.current_buffer, &buf);
    ui.current_buffer->undo = b;
}

DEFUN(undoPos, UNDO, "Cancel the last cursor movement")
{
    if (!ui.current_buffer->document.firstLine)
        return;

    struct BufferPos* b = ui.current_buffer->undo;
    if (!b || !b->prev)
        return;

    resetPos(ui, b);
}

DEFUN(redoPos, REDO, "Cancel the last undo")
{
    if (!ui.current_buffer->document.firstLine)
        return;

    struct BufferPos* b = ui.current_buffer->undo;
    if (!b || !b->next)
        return;

    resetPos(ui, b);
}

DEFUN(cursorTop, CURSOR_TOP, "Move cursor to the top of the screen")
{
    if (ui.current_buffer->document.firstLine == NULL)
        return;
    ui.current_buffer->document.currentLineIndex = 0;
}

DEFUN(cursorMiddle, CURSOR_MIDDLE, "Move cursor to the middle of the screen")
{
    if (ui.current_buffer->document.firstLine == NULL)
        return;
    int offsety = (getScreen()->ROWS - 1) / 2;
    ui.current_buffer->document.currentLineIndex += offsety;
}

DEFUN(cursorBottom, CURSOR_BOTTOM, "Move cursor to the bottom of the screen")
{
    if (ui.current_buffer->document.firstLine == NULL)
        return;
    int offsety = getScreen()->ROWS - 1;
    ui.current_buffer->document.currentLineIndex += offsety;
}

Str myEditor(const char* cmd, const char* file, int line)
{
    Str tmp = NULL;
    int set_file = false, set_line = false;

    for (const char* p = cmd; *p; p++) {
        if (*p == '%' && *(p + 1) == 's' && !set_file) {
            if (tmp == NULL)
                tmp = Strnew_charp_n(cmd, (int)(p - cmd));
            Strcat_charp(tmp, file);
            set_file = true;
            p++;
        } else if (*p == '%' && *(p + 1) == 'd' && !set_line && line > 0) {
            if (tmp == NULL)
                tmp = Strnew_charp_n(cmd, (int)(p - cmd));
            Strcat(tmp, Sprintf("%d", line));
            set_line = true;
            p++;
        } else {
            if (tmp)
                Strcat_char(tmp, *p);
        }
    }
    if (!set_file) {
        if (tmp == NULL)
            tmp = Strnew_charp(cmd);
        if (!set_line && line > 1 && strcasestr(cmd, "vi"))
            Strcat(tmp, Sprintf(" +%d", line));
        Strcat_m_charp(tmp, " ", file, NULL);
    }
    return tmp;
}

void _quitfm(bool confirm)
{
    const char* ans = "y";
    if (checkDownloadList())
        /* FIXME: gettextize? */
        ans = inputChar(getUI(), "Download process retains. "
                                 "Do you want to exit w3m? (y/n)");
    else if (confirm)
        /* FIXME: gettextize? */
        ans = inputChar(getUI(), "Do you want to exit w3m? (y/n)");
    if (!(ans && TOLOWER(*ans) == 'y')) {

        return;
    }

    term_title(""); /* XXX */
    if (activeImage)
        termImage();
    fmTerm();
    save_cookies();
    if (UseHistory && SaveURLHist)
        saveHistory(URLHist, URLHistSize);
    w3m_exit(0);
}
