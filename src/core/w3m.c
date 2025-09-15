#include "w3m.h"
#include "document_renderer.h"
#include "MapArea.h"
#include "HtmlTagParsed.h"
#include "page_info.h"
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
#include "image_loader.h"
#include "keymap.h"
#include "linein.h"
#include "local_cgi.h"
#include "menu.h"
#include "myctype.h"
#include "Anchor.h"
#include "AnchorList.h"
#include "progress.h"
#include "quote.h"
#include "rc.h"
#include "search.h"
#include "ssl_util.h"
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
#include "buffer_util.h"
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

#define PACKAGE "w3m"
#define HELP_FILE "w3mhelp-w3m_en.html"
#define BOOKMARK "bookmark.html"

char* mkd_tmp_dir = (NULL);

int UseDictCommand = (true);
char* DictCommand = ("file:///$LIB/w3mdict" CGI_EXTENSION);
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

void set_buffer_environ(struct UI ui);
static void save_buffer_position(struct Buffer* buf);

static int check_target = true;

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
        ui.current_buffer->document.pos = a->start.pos;
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
    if (activeImage && displayImage && ui.document->img && !ui.document->image_loaded) {
        loadImage(ui.document, IMG_FLAG_NEXT, false);
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
    ui.current_buffer->document.pos = 0;
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

void follow_map(struct UI ui, struct KeyValue* arg)
{
    const char* name = tag_get_value(arg, "link");

    struct Anchor* an = retrieveAnchor(ui.current_buffer->document.img, getBufferPosition(ui));
    int x, y;
    // x = ui.current_buffer->cursorX;
    // y = ui.current_buffer->cursorY;
    struct MapArea* a = follow_map_menu(ui, &ui.current_buffer->document, name, an, x, y);
    if (a == NULL || a->url == NULL || *(a->url) == '\0') {
        return;
    }
    if (*(a->url) == '#') {
        gotoLabel(ui, a->url + 1);
        return;
    }

    struct Url p_url = parseUrl(a->url, makeBaseUrl(&ui.current_buffer->document));
    pushHashHist(URLHist, parsedURL2Str(&p_url)->ptr);
    struct Content c = getContent(a->url, makeBaseUrl(&ui.current_buffer->document),
        NULL, parsedURL2Str(&ui.current_buffer->content.url)->ptr, UI_TTY);
    pushContent(c, ui.viewport.size.x, ui.use_graphic);
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
    const char* file = guessSaveName(ui.current_buffer->content.document_header, ui.current_buffer->content.url.file);
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
    a = (only_img ? NULL : retrieveAnchor(ui.current_buffer->document.href, getBufferPosition(ui)));
    if (a == NULL) {
        a = (only_img ? NULL : retrieveAnchor(ui.current_buffer->document.formitem, getBufferPosition(ui)));
        if (a == NULL) {
            a = retrieveAnchor(ui.current_buffer->document.img, getBufferPosition(ui));
            if (a == NULL)
                return;
        } else
            s = Strnew_charp(form2str((struct FormItem*)a->url));
    }
    if (s == NULL) {
        pu = parseUrl(a->url, makeBaseUrl(&ui.current_buffer->document));
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
    buf->content.sourcefile = ui.current_buffer->content.sourcefile;
    buf->document.charset = ui.current_buffer->document.charset;
    buf->clone = ui.current_buffer->clone;
    (*buf->clone)++;

    pushBuffer(buf);
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
            query_from_followform(ui.current_buffer, getBufferPosition(ui),
                &query, ui.current_buffer->form_submit, multipart);
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
    struct Content c = getContent(url->ptr, NULL, post, NO_REFERER, UI_TTY /*, true*/);

    struct Buffer* buf = makeBuffer(&c, ui.viewport.size.x, ui.use_graphic);
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
    repBuffer(ui.current_buffer, buf);
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
    ui.current_buffer->document.cols = 0;
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
    ui.document->image_flag = IMG_FLAG_AUTO;
}

DEFUN(stopI, STOP_IMAGE, "Stop loading and drawing of images")
{
    if (!activeImage)
        return;
    /*
     * if (!(ui.current_buffer->type && is_html_type(ui.current_buffer->type)))
     * return;
     */
    ui.document->image_flag = IMG_FLAG_SKIP;
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
    e = buf->document.pos;
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
    struct Content c = getContent(dictcmd, NULL, NULL, NO_REFERER, UI_TTY);
    pushContent(c, ui.viewport.size.x, ui.use_graphic);
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

void set_buffer_environ(struct UI ui)
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
        char* s = GetWord(buf);
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

static Str DownloadListBuffer_html()
{
    if (!FirstDL)
        return 0;

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

    return src;
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
                delBuffer(ui.current_buffer);
        }
        return;
    }
    int reload = checkDownloadList();

    struct Content c = makeContentFromHtmlUtf8(DownloadListBuffer_html());
    if (!c.page) {
        return;
    }
    // buf->bufferprop |= (BP_INTERNAL | BP_NO_URL);
    if (replace) {
        // COPY_BUFROOT(buf, ui.current_buffer);
        // restorePosition(buf, ui.current_buffer);
    }
    pushContent(c, ui.viewport.size.x, ui.use_graphic);
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

static void
resetPos(struct UI ui, struct BufferPos* b)
{
    struct Buffer buf = {
        .document = {
            .topLineIndex = b->top_linenumber,
            .currentLineIndex = b->cur_linenumber,
            .pos = b->pos,
            .currentColumn = b->currentColumn,
        },
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
