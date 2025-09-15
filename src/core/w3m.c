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
#include "rc.h"
#include "ssl_util.h"
#include "term_renderer.h"
#include "ui.h"
#include "runtime.h"
#include "buffer_util.h"
#include "funcname1.h"
#include "proxy.h"
#include "term_size.h"
#include "tty.h"
#include "screen.h"
#include "myctype.h"
#include "alloc.h"

#include <errno.h>
#include <event_poller.h>

#include <wc.h>

#include <gc/gc.h>
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

static int display_ok = false;
int prev_key = -1;

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

    char* input_buffer;
    int input_buffer_len;
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
    iob->input_buffer = 0;
    iob->input_buffer_len = 0;
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
    if (iob->input_buffer) {
        free(iob->input_buffer);
    }
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
                // read STDIN
                int ready_read_size;
                if (ioctl(ev.data.fd, FIONREAD, &ready_read_size) == -1) {
                    // error
                    return (struct IOBEvent) {
                        .type = IOB_EVENT_ERROR,
                        .value = errno,
                    };
                }
                if (ready_read_size == 0) {
                    // no input
                    return (struct IOBEvent) {
                        .type = IOB_EVENT_ERROR,
                        .value = errno,
                    };
                }
                if (!iob->input_buffer) {
                    iob->input_buffer = malloc(ready_read_size);
                    iob->input_buffer_len = ready_read_size;
                } else if (ready_read_size > iob->input_buffer_len) {
                    iob->input_buffer = realloc(iob->input_buffer, ready_read_size);
                    iob->input_buffer_len = ready_read_size;
                }
                iob->input_pos = 0;
                iob->input_enable = read(ev.data.fd, iob->input_buffer, iob->input_buffer_len);
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
