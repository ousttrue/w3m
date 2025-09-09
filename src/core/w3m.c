#include "w3m.h"
#include "alloc.h"
#include "expandpath.h"
#include "defun_macro.h"
#include "mimetypes.h"
#include "Content.h"
#include "buffer_loader.h"
#include "file_copy.h"
#include "tmpfile.h"
#include "istream.h"
#include "progress.h"
#include "version.h"
#include "html_quote.h"
#include "quote.h"
#include <gc/gc.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/param.h>
#include "buffer.h"
#include "KeyValue.h"
#include "defun.h"
#include "linein.h"
#include "menu.h"
#include "keymap.h"
#include "downloadlist.h"
#include "funcname1.h"
#include "form.h"
#include "http.h"
#include "mysignal.h"
#include "proxy.h"
#include "maparea.h"
#include "ssl_util.h"
#include "mailcap.h"
#include "local.h"
#include "cookie.h"
#include "ui.h"
#include "search.h"
#include "str_util.h"
#include "history.h"
#include "image.h"
#include "putc.h"
#include "frame.h"
#include "term_renderer.h"
#include "term_size.h"
#include "graphicchar.h"
#include "tty.h"
#include "TermEntry.h"
#include "screen.h"
#include <stdio.h>
#include <signal.h>
#include <setjmp.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <time.h>
#include "display.h"
#include "myctype.h"
#include "regex.h"
#include "rc.h"
#include "wc.h"
#include "wtf.h"
#include "ucs.h"
#include "util.h"
#include <sys/epoll.h>
#include <assert.h>
#include <sys/signalfd.h>
#include <locale.h>
#include <unistd.h>

#include <event_poller.h>

#define PACKAGE "w3m"
#define HELP_FILE "w3mhelp-w3m_en.html"
#define HELP_CGI "w3mhelp"
#define BOOKMARK "bookmark.html"

char ArgvIsURL = true;
int DecodeURL = false;

int DefaultURLString = (DEFAULT_URL_CURRENT);
int UseDictCommand = (true);
char* DictCommand = ("file:///$LIB/w3mdict" CGI_EXTENSION);
char* BookmarkFile = (NULL);
int use_mark = (false);
int confirm_on_quit = (true);
int CurrentKey;
char* CurrentKeyData;
char* CurrentCmdData;

#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 255
#endif

#define DSTR_LEN 256

int clear_buffer = (true);
char* config_file = (NULL);
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
static MySignalHandler SigAlarm(int _dummy);

static int need_resize_screen = false;
MySignalHandler resize_hook(int _dummy);
static void resize_screen(void);

static void cmd_loadfile(char* path);
static void cmd_loadBuffer(Buffer* buf, int prop, int linkid);

static char* getCurWord(Buffer* buf, int* spos, int* epos);

static int display_ok = false;
int prev_key = -1;

void set_buffer_environ(Buffer*);
static void save_buffer_position(Buffer* buf);

static void _nextA(int);
static void _prevA(int);
static int check_target = true;
static int searchKeyNum(void);

/*
 * List of error messages
 */
Buffer*
message_list_panel(void)
{
    Str tmp = Strnew_size(getScreen()->ROWS * getScreen()->COLS);
    ListItem* p;

    /* FIXME: gettextize? */
    Strcat_charp(tmp,
        "<html><head><title>List of error messages</title></head><body>"
        "<h1>List of error messages</h1><table cellpadding=0>\n");

    concatMessageList(tmp);

    Strcat_charp(tmp, "</table></body></html>");
    return loadHTMLString(tmp);
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

static void
sig_chld(int signo)
{
    int p_stat;
    pid_t pid;

    while ((pid = waitpid(-1, &p_stat, WNOHANG)) > 0) {
        DownloadList* d;

        if (WIFEXITED(p_stat)) {
            for (d = FirstDL; d != NULL; d = d->next) {
                if (d->pid == pid) {
                    d->err = WEXITSTATUS(p_stat);
                    break;
                }
            }
        }
    }
    mySignal(SIGCHLD, sig_chld);
}

static void
SigPipe(int _dummy)
{
    mySignal(SIGPIPE, SigPipe);
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
    mySignal(SIGWINCH, resize_hook);

    sync_with_option();
    initCookie();
    if (UseHistory)
        loadHistory(URLHist);

    mySignal(SIGCHLD, sig_chld);
    mySignal(SIGPIPE, SigPipe);

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
    vt_move(vt, getLines() - 1, 0);
    vt_clrtoeolx(vt);
    // refresh(ttyWriter());
    if (activeImage)
        loadImage(NULL, IMG_FLAG_STOP, false);
    resetTerm();
    flush_tty();
    TerminalSet(NULL);
    close_tty();
}

static MySignalHandler
reset_exit_with_value(int _dummy, int rval)
{
    resetTerm();
    flush_tty();
    TerminalSet(NULL);
    close_tty();

    w3m_exit(rval);
}

MySignalHandler
reset_error_exit(int _dummy)
{
    reset_exit_with_value(SIGNAL_ARGLIST, 1);
}

MySignalHandler
reset_exit(int _dummy)
{
    reset_exit_with_value(SIGNAL_ARGLIST, 0);
}

MySignalHandler
error_dump(int _dummy)
{
    mySignal(SIGIOT, SIG_DFL);
    resetTerm();
    flush_tty();
    TerminalSet(NULL);
    close_tty();

    abort();
}

void set_int(void)
{
    mySignal(SIGHUP, reset_exit);
    mySignal(SIGINT, reset_exit);
    mySignal(SIGQUIT, reset_exit);
    mySignal(SIGTERM, reset_exit);
    mySignal(SIGILL, error_dump);
    mySignal(SIGIOT, error_dump);
    mySignal(SIGFPE, error_dump);
#ifdef SIGBUS
    mySignal(SIGBUS, error_dump);
#endif /* SIGBUS */
    /* mySignal(SIGSEGV, error_dump); */
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
    set_int();
    initscr();
    struct VirtualTerm* vt = getScreen();
    vt_setupscreen(vt, getLines(), getCols());
    termClear(ttyWriter());
    term_raw();
    term_noecho();
    if (displayImage)
        initImage();
}

static Str
conv_form_encoding(Str val, struct FormItem* fi, Buffer* buf)
{
    wc_ces charset = SystemCharset;

    if (fi->parent->charset)
        charset = fi->parent->charset;
    else if (buf->document_charset && buf->document_charset != WC_CES_US_ASCII)
        charset = buf->document_charset;
    return wc_Str_conv_strict(val, InnerCharset, charset);
}

static void
query_from_followform(Str* query, struct FormItem* fi, int multipart)
{
    struct FormItem* f2;
    FILE* body = NULL;

    if (multipart) {
        *query = tmpfname(TMPF_DFL, NULL);
        body = fopen((*query)->ptr, "w");
        if (body == NULL) {
            return;
        }
        fi->parent->body = (*query)->ptr;
        fi->parent->boundary = Sprintf("------------------------------%d%ld%ld%ld", CurrentPid,
            fi->parent, fi->parent->body, fi->parent->boundary)
                                   ->ptr;
    }
    *query = Strnew();
    for (f2 = fi->parent->item; f2; f2 = f2->next) {
        if (f2->name == NULL)
            continue;
        /* <ISINDEX> is translated into single text form */
        if (f2->name->length == 0 && (multipart || f2->type != FORM_INPUT_TEXT))
            continue;
        switch (f2->type) {
        case FORM_INPUT_RESET:
            /* do nothing */
            continue;
        case FORM_INPUT_SUBMIT:
        case FORM_INPUT_IMAGE:
            if (f2 != fi || f2->value == NULL)
                continue;
            break;
        case FORM_INPUT_RADIO:
        case FORM_INPUT_CHECKBOX:
            if (!f2->checked)
                continue;
        default:
            break;
        }
        if (multipart) {
            if (f2->type == FORM_INPUT_IMAGE) {
                int x = 0, y = 0;
                getMapXY(Currentbuf, retrieveCurrentImg(Currentbuf), &x, &y);
                *query = Strdup(conv_form_encoding(f2->name, fi, Currentbuf));
                Strcat_charp(*query, ".x");
                form_write_data(body, fi->parent->boundary, (*query)->ptr,
                    Sprintf("%d", x)->ptr);
                *query = Strdup(conv_form_encoding(f2->name, fi, Currentbuf));
                Strcat_charp(*query, ".y");
                form_write_data(body, fi->parent->boundary, (*query)->ptr,
                    Sprintf("%d", y)->ptr);
            } else if (f2->name && f2->name->length > 0 && f2->value != NULL) {
                /* not IMAGE */
                *query = conv_form_encoding(f2->value, fi, Currentbuf);
                if (f2->type == FORM_INPUT_FILE)
                    form_write_from_file(body, fi->parent->boundary,
                        conv_form_encoding(f2->name, fi,
                            Currentbuf)
                            ->ptr,
                        (*query)->ptr,
                        Str_conv_to_system(f2->value)->ptr);
                else
                    form_write_data(body, fi->parent->boundary,
                        conv_form_encoding(f2->name, fi,
                            Currentbuf)
                            ->ptr,
                        (*query)->ptr);
            }
        } else {
            /* not multipart */
            if (f2->type == FORM_INPUT_IMAGE) {
                int x = 0, y = 0;
                getMapXY(Currentbuf, retrieveCurrentImg(Currentbuf), &x, &y);
                Strcat(*query,
                    Str_form_quote(conv_form_encoding(f2->name, fi, Currentbuf)));
                Strcat(*query, Sprintf(".x=%d&", x));
                Strcat(*query,
                    Str_form_quote(conv_form_encoding(f2->name, fi, Currentbuf)));
                Strcat(*query, Sprintf(".y=%d", y));
            } else {
                /* not IMAGE */
                if (f2->name && f2->name->length > 0) {
                    Strcat(*query,
                        Str_form_quote(conv_form_encoding(f2->name, fi, Currentbuf)));
                    Strcat_char(*query, '=');
                }
                if (f2->value != NULL) {
                    if (fi->parent->method == FORM_METHOD_INTERNAL)
                        Strcat(*query, Str_form_quote(f2->value));
                    else {
                        Strcat(*query,
                            Str_form_quote(conv_form_encoding(f2->value, fi, Currentbuf)));
                    }
                }
            }
            if (f2->next)
                Strcat_char(*query, '&');
        }
    }
    if (multipart) {
        fprintf(body, "--%s--\r\n", fi->parent->boundary);
        fclose(body);
    } else {
        /* remove trailing & */
        while (Strlastchar(*query) == '&')
            Strshrink(*query, 1);
    }
}

static void pushBuffer(Buffer* buf)
{
    deleteImage(Currentbuf);
    if (clear_buffer)
        tmpClearBuffer(Currentbuf);

    Buffer* b;
    if (Firstbuf == Currentbuf) {
        buf->nextBuffer = Firstbuf;
        Firstbuf = Currentbuf = buf;
    } else if ((b = prevBuffer(Firstbuf, Currentbuf)) != NULL) {
        b->nextBuffer = buf;
        buf->nextBuffer = Currentbuf;
        Currentbuf = buf;
    }
    saveBufferInfo();
}

static Buffer* loadNormalBuf(Buffer* buf)
{
    pushBuffer(buf);
    return buf;
}

static Buffer*
loadLink(const char* url, const char* target, const char* referer, struct Form* post, bool do_download)
{
    Buffer* nfbuf;
    union frameset_element* f_element = NULL;
    struct Url *base, pu;
    const int* no_referer_ptr;

    message(getUI(), MSG_INFO, Sprintf("loading %s", url)->ptr);
    // refresh(ttyWriter());

    base = baseURL(Currentbuf);
    if ((no_referer_ptr && *no_referer_ptr) || base == NULL || base->scheme == SCM_LOCAL || base->scheme == SCM_LOCAL_CGI || base->scheme == SCM_DATA)
        referer = NO_REFERER;
    if (referer == NULL)
        referer = parsedURL2RefererStr(&Currentbuf->currentURL)->ptr;

    struct Content c = loadGeneralFile(url, baseURL(Currentbuf), post, referer);
    Buffer* buf = makeBuffer(&c, do_download);
    if (buf == NULL) {
        char* emsg = Sprintf("Can't load %s", url)->ptr;
        message(getUI(), MSG_ERR, emsg);
        return NULL;
    }

    pu = parseUrl(url, base);
    pushHashHist(URLHist, parsedURL2Str(&pu)->ptr);

    if (buf == NO_BUFFER) {
        return NULL;
    }

    if (do_download) /* download (thus no need to render frames) */
        return loadNormalBuf(buf);

    if (target == NULL || /* no target specified (that means this page is not a frame page) */
        !strcmp(target, "_top") /* this link is specified to be opened as an indivisual * page */
    ) {
        return loadNormalBuf(buf);
    }

    return loadNormalBuf(buf);
}

static struct FormItem* save_submit_formlist(struct FormItem* src)
{
    struct Form* list;
    struct Form* srclist;
    struct FormItem* srcitem;
    struct FormItem* item;
    struct FormItem* ret = NULL;
    struct FormSelectOptionItem* opt;
    struct FormSelectOptionItem* curopt;
    struct FormSelectOptionItem* srcopt;

    if (src == NULL)
        return NULL;
    srclist = src->parent;
    list = New(struct Form);
    list->method = srclist->method;
    list->action = Strdup(srclist->action);
    list->charset = srclist->charset;
    list->enctype = srclist->enctype;
    list->nitems = srclist->nitems;
    list->body = srclist->body;
    list->boundary = srclist->boundary;
    list->length = srclist->length;

    for (srcitem = srclist->item; srcitem; srcitem = srcitem->next) {
        item = New(struct FormItem);
        item->type = srcitem->type;
        item->name = Strdup(srcitem->name);
        item->value = Strdup(srcitem->value);
        item->checked = srcitem->checked;
        item->accept = srcitem->accept;
        item->size = srcitem->size;
        item->rows = srcitem->rows;
        item->maxlength = srcitem->maxlength;
        item->readonly = srcitem->readonly;
        opt = curopt = NULL;
        for (srcopt = srcitem->select_option; srcopt; srcopt = srcopt->next) {
            if (!srcopt->checked)
                continue;
            opt = New(struct FormSelectOptionItem);
            opt->value = Strdup(srcopt->value);
            opt->label = Strdup(srcopt->label);
            opt->checked = srcopt->checked;
            if (item->select_option == NULL) {
                item->select_option = curopt = opt;
            } else {
                curopt->next = opt;
                curopt = curopt->next;
            }
        }
        item->select_option = opt;
        if (srcitem->label)
            item->label = Strdup(srcitem->label);
        item->parent = list;
        item->next = NULL;

        if (list->lastitem == NULL) {
            list->item = list->lastitem = item;
        } else {
            list->lastitem->next = item;
            list->lastitem = item;
        }

        if (srcitem == src)
            ret = item;
    }

    return ret;
}

void do_submit(Anchor* a, struct FormItem* fi, bool do_download)
{
    Str tmp = Strnew();
    int multipart = (fi->parent->method == FORM_METHOD_POST && fi->parent->enctype == FORM_ENCTYPE_MULTIPART);
    query_from_followform(&tmp, fi, multipart);

    Str tmp2 = Strdup(fi->parent->action);
    if (!Strcmp_charp(tmp2, "!CURRENT_URL!")) {
        /* It means "current URL" */
        tmp2 = parsedURL2Str(&Currentbuf->currentURL);
        char* p;
        if ((p = strchr(tmp2->ptr, '?')) != NULL)
            Strshrink(tmp2, (tmp2->ptr + tmp2->length) - p);
    }

    if (fi->parent->method == FORM_METHOD_GET) {
        char* p;
        if ((p = strchr(tmp2->ptr, '?')) != NULL)
            Strshrink(tmp2, (tmp2->ptr + tmp2->length) - p);
        Strcat_charp(tmp2, "?");
        Strcat(tmp2, tmp);
        loadLink(tmp2->ptr, (char*)a->target, NULL, NULL, do_download);
    } else if (fi->parent->method == FORM_METHOD_POST) {
        Buffer* buf;
        if (multipart) {
            struct stat st;
            stat(fi->parent->body, &st);
            fi->parent->length = st.st_size;
        } else {
            fi->parent->body = tmp->ptr;
            fi->parent->length = tmp->length;
        }
        buf = loadLink(tmp2->ptr, (char*)a->target, NULL, fi->parent, do_download);
        if (multipart) {
            unlink(fi->parent->body);
        }
        // if (buf && !(buf->bufferprop & BP_REDIRECTED)) { /* buf must be Currentbuf */
        /* BP_REDIRECTED means that the buffer is obtained through
         * Location: header. In this case, buf->form_submit must not be set
         * because the page is not loaded by POST method but GET method.
         */
        buf->form_submit = save_submit_formlist(fi);
        // }
    } else if ((fi->parent->method == FORM_METHOD_INTERNAL && (!Strcmp_charp(fi->parent->action, "map") || !Strcmp_charp(fi->parent->action, "none"))) || Currentbuf->bufferprop & BP_INTERNAL) { /* internal */
        do_internal(tmp2->ptr, tmp->ptr);
    } else {
        message(getUI(), MSG_ERR, "Can't send form because of illegal method.");
    }
}

static void
_followForm(bool submit, bool do_download)
{
    if (Currentbuf->firstLine == NULL)
        return;

    Anchor* a = retrieveCurrentForm(Currentbuf);
    if (a == NULL)
        return;

    struct FormItem* fi = (struct FormItem*)a->url;
    switch (fi->type) {
    case FORM_INPUT_TEXT: {
        if (submit) {
            do_submit(a, fi, do_download);
            return;
        }
        if (fi->readonly)
            /* FIXME: gettextize? */
            message(getUI(), MSG_INFO, "Read only field!");
        /* FIXME: gettextize? */
        char* p = inputStrHist(getUI(), "TEXT:", fi->value ? fi->value->ptr : NULL, TextHist);
        if (p == NULL || fi->readonly)
            break;
        fi->value = Strnew_charp(p);
        formUpdateBuffer(a, Currentbuf, fi);
        if (fi->accept || fi->parent->nitems == 1) {
            do_submit(a, fi, do_download);
            return;
        }
        break;
    }
    case FORM_INPUT_FILE: {
        if (submit) {
            do_submit(a, fi, do_download);
            return;
        }
        if (fi->readonly)
            /* FIXME: gettextize? */
            message(getUI(), MSG_INFO, "Read only field!");
        /* FIXME: gettextize? */
        char* p = inputFilenameHist(getUI(), "Filename:", fi->value ? fi->value->ptr : NULL, NULL);
        if (p == NULL || fi->readonly)
            break;
        fi->value = Strnew_charp(p);
        formUpdateBuffer(a, Currentbuf, fi);
        if (fi->accept || fi->parent->nitems == 1) {
            do_submit(a, fi, do_download);
            return;
        }
        break;
    }
    case FORM_INPUT_PASSWORD: {
        if (submit) {
            do_submit(a, fi, do_download);
            return;
        }
        if (fi->readonly) {
            /* FIXME: gettextize? */
            message(getUI(), MSG_INFO, "Read only field!");
            break;
        }
        /* FIXME: gettextize? */
        char* p = inputLine(getUI(), "Password:", fi->value ? fi->value->ptr : NULL,
            IN_PASSWORD);
        if (p == NULL)
            break;
        fi->value = Strnew_charp(p);
        formUpdateBuffer(a, Currentbuf, fi);
        if (fi->accept) {
            do_submit(a, fi, do_download);
            return;
        }
        break;
    }
    case FORM_TEXTAREA: {
        if (submit) {
            do_submit(a, fi, do_download);
            return;
        }
        if (fi->readonly) {
            message(getUI(), MSG_INFO, "Read only field!");
        }
        input_textarea(fi);
        formUpdateBuffer(a, Currentbuf, fi);
        break;
    }
    case FORM_INPUT_RADIO: {
        if (submit) {
            do_submit(a, fi, do_download);
            return;
        }
        if (fi->readonly) {
            message(getUI(), MSG_INFO, "Read only field!");
            break;
        }
        formRecheckRadio(a, Currentbuf, fi);
        break;
    }
    case FORM_INPUT_CHECKBOX: {
        if (submit) {
            do_submit(a, fi, do_download);
            return;
        }
        if (fi->readonly) {
            /* FIXME: gettextize? */
            message(getUI(), MSG_INFO, "Read only field!");
            break;
        }
        fi->checked = !fi->checked;
        formUpdateBuffer(a, Currentbuf, fi);
        break;
    }
    case FORM_SELECT: {
        if (submit) {
            do_submit(a, fi, do_download);
            return;
        }
        if (!formChooseOptionByMenu(fi,
                Currentbuf->cursorX - Currentbuf->pos + a->start.pos,
                Currentbuf->cursorY))
            break;
        formUpdateBuffer(a, Currentbuf, fi);
        if (fi->parent->nitems == 1) {
            do_submit(a, fi, do_download);
            return;
        }
        break;
    }
    case FORM_INPUT_IMAGE:
    case FORM_INPUT_SUBMIT:
    case FORM_INPUT_BUTTON: {
        do_submit(a, fi, do_download);
        break;
    }
    case FORM_INPUT_RESET: {
        for (int i = 0; i < Currentbuf->formitem->nanchor; i++) {
            Anchor* a2 = &Currentbuf->formitem->anchors[i];
            struct FormItem* f2 = (struct FormItem*)a2->url;
            if (f2->parent == fi->parent && f2->name && f2->value && f2->type != FORM_INPUT_SUBMIT && f2->type != FORM_INPUT_HIDDEN && f2->type != FORM_INPUT_RESET) {
                f2->value = f2->init_value;
                f2->checked = f2->init_checked;
                f2->label = f2->init_label;
                f2->selected = f2->init_selected;
                formUpdateBuffer(a2, Currentbuf, f2);
            }
        }
        break;
    }
    case FORM_INPUT_HIDDEN:
    default:
        break;
    }
}

bool onFrame()
{
    struct TermEntry* t = getTermEntry();
    bool use_graphic = graph_ok(t);

    updateDownload();
    if (Currentbuf->submit) {
        Anchor* a = Currentbuf->submit;
        Currentbuf->submit = NULL;
        gotoLine(Currentbuf, a->start.line);
        Currentbuf->pos = a->start.pos;
        _followForm(true, false);
        return false;
    }
    /* event processing */
    if (CurrentEvent) {
        CurrentKey = -1;
        CurrentKeyData = NULL;
        CurrentCmdData = (char*)CurrentEvent->data;
        w3mFuncList[CurrentEvent->cmd].func();

        bufToScreen(getUI(), Currentbuf);
        renderFrame(getUI());

        CurrentCmdData = NULL;
        CurrentEvent = CurrentEvent->next;
        return false;
    }
    /* get keypress event */
    if (Currentbuf->event) {
        if (Currentbuf->event->status != AL_UNSET) {
            CurrentAlarm = Currentbuf->event;
            if (CurrentAlarm->sec == 0) { /* refresh (0sec) */
                Currentbuf->event = NULL;
                CurrentKey = -1;
                CurrentKeyData = NULL;
                CurrentCmdData = (char*)CurrentAlarm->data;
                w3mFuncList[CurrentAlarm->cmd].func();

                bufToScreen(getUI(), Currentbuf);
                renderFrame(getUI());

                CurrentCmdData = NULL;
                return false;
            }
        } else
            Currentbuf->event = NULL;
    }
    if (!Currentbuf->event)
        CurrentAlarm = &DefaultAlarm;
    if (CurrentAlarm->sec > 0) {
        mySignal(SIGALRM, SigAlarm);
        alarm(CurrentAlarm->sec);
    }

    mySignal(SIGWINCH, resize_hook);
    if (activeImage && displayImage && Currentbuf->img && !Currentbuf->image_loaded) {
        loadImage(Currentbuf, IMG_FLAG_NEXT, false);
        bufToScreen(getUI(), Currentbuf);
        renderFrame(getUI());
        // continue;
    }
    if (need_resize_screen) {
        resize_screen();
        bufToScreen(getUI(), Currentbuf);
        renderFrame(getUI());
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
        ui_printStatus("STATUS: key=[%02x > %02x > %02x > %02x > %02x > %02x > %02x > %02x]",
            g_keylog[(g_i - 0) % sizeof(g_keylog)],
            g_keylog[(g_i - 1) % sizeof(g_keylog)],
            g_keylog[(g_i - 2) % sizeof(g_keylog)],
            g_keylog[(g_i - 3) % sizeof(g_keylog)],
            g_keylog[(g_i - 4) % sizeof(g_keylog)],
            g_keylog[(g_i - 5) % sizeof(g_keylog)],
            g_keylog[(g_i - 6) % sizeof(g_keylog)],
            g_keylog[(g_i - 7) % sizeof(g_keylog)]);

        set_buffer_environ(Currentbuf);
        save_buffer_position(Currentbuf);
        {
            CurrentKey = c;
            unsigned char prev = g_keylog[(g_i - 1) % sizeof(g_keylog)];
            CommandFunc func = (prev == 0x1b) ? EscKeymap[c]
                                              : GlobalKeymap[c];
            func();
        }
        bufToScreen(getUI(), Currentbuf);
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

DEFUN(nulcmd, NOTHING NULL @ @ @, "Do nothing")
{ /* do nothing */
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
        w3mFuncList[(int)map[c]].func();
}

void tmpClearBuffer(Buffer* buf)
{
    if (writeBufferCache(buf) == 0) {
        buf->firstLine = NULL;
        buf->topLine = NULL;
        buf->currentLine = NULL;
        buf->lastLine = NULL;
    }
}

static Str currentURL(void);

void saveBufferInfo()
{
    FILE* fp;
    if ((fp = fopen(rcFile("bufinfo"), "w")) == NULL) {
        return;
    }
    fprintf(fp, "%s\n", currentURL()->ptr);
    fclose(fp);
}

void delBuffer(Buffer* buf)
{
    if (buf == NULL)
        return;
    if (Currentbuf == buf)
        Currentbuf = buf->nextBuffer;
    Firstbuf = deleteBuffer(Firstbuf, buf);
    if (!Currentbuf)
        Currentbuf = Firstbuf;
}

static void
repBuffer(Buffer* oldbuf, Buffer* buf)
{
    Firstbuf = replaceBuffer(Firstbuf, oldbuf, buf);
    Currentbuf = buf;
}

static sigjmp_buf IntReturn;
static MySignalHandler intTrap(int _dummy)
{ /* Interrupt catcher */
    siglongjmp(IntReturn, 0);
}

MySignalHandler
resize_hook(int _dummy)
{
    need_resize_screen = true;
    mySignal(SIGWINCH, resize_hook);
}

static void
resize_screen(void)
{
    need_resize_screen = false;
    setlinescols(get_tty_fd());
    vt_setupscreen(getScreen(), getLines(), getCols());
    vt_clear(getScreen());
}

/*
 * Command functions: These functions are called with a keystroke.
 */

static void
nscroll(int n)
{
    Buffer* buf = Currentbuf;
    Line *top = buf->topLine, *cur = buf->currentLine;
    int lnum, tlnum, llnum, diff_n;

    if (buf->firstLine == NULL)
        return;
    lnum = cur->linenumber;
    buf->topLine = lineSkip(buf, top, n, false);
    if (buf->topLine == top) {
        lnum += n;
        if (lnum < buf->topLine->linenumber)
            lnum = buf->topLine->linenumber;
        else if (lnum > buf->lastLine->linenumber)
            lnum = buf->lastLine->linenumber;
    } else {
        tlnum = buf->topLine->linenumber;
        llnum = buf->topLine->linenumber + getScreen()->ROWS - 1;
        if (nextpage_topline)
            diff_n = 0;
        else
            diff_n = n - (tlnum - top->linenumber);
        if (lnum < tlnum)
            lnum = tlnum + diff_n;
        if (lnum > llnum)
            lnum = llnum + diff_n;
    }
    gotoLine(buf, lnum);
    arrangeLine(buf);
    if (n > 0) {
        if (buf->currentLine->bpos && buf->currentLine->bwidth >= buf->currentColumn + buf->visualpos)
            cursorDown(buf, 1);
        else {
            while (buf->currentLine->next && buf->currentLine->next->bpos && buf->currentLine->bwidth + buf->currentLine->width < buf->currentColumn + buf->visualpos)
                cursorDown0(buf, 1);
        }
    } else {
        if (buf->currentLine->bwidth + buf->currentLine->width < buf->currentColumn + buf->visualpos)
            cursorUp(buf, 1);
        else {
            while (buf->currentLine->prev && buf->currentLine->bpos && buf->currentLine->bwidth >= buf->currentColumn + buf->visualpos)
                cursorUp0(buf, 1);
        }
    }
}

/* Move page forward */
DEFUN(pgFore, NEXT_PAGE, "Scroll down one page")
{
    nscroll(searchKeyNum() * (getScreen()->ROWS - 1));
}

/* Move page backward */
DEFUN(pgBack, PREV_PAGE, "Scroll up one page")
{
    nscroll(searchKeyNum() * (getScreen()->ROWS - 1));
}

/* Move half page forward */
DEFUN(hpgFore, NEXT_HALF_PAGE, "Scroll down half a page")
{
    nscroll(-searchKeyNum() * (getScreen()->ROWS / 2 - 1));
}

/* Move half page backward */
DEFUN(hpgBack, PREV_HALF_PAGE, "Scroll up half a page")
{
    nscroll(-searchKeyNum() * (getScreen()->ROWS / 2 - 1));
}

/* 1 line up */
DEFUN(lup1, UP, "Scroll the screen up one line")
{
    nscroll(searchKeyNum());
}

/* 1 line down */
DEFUN(ldown1, DOWN, "Scroll the screen down one line")
{
    nscroll(-searchKeyNum());
}

/* move cursor position to the center of screen */
DEFUN(ctrCsrV, CENTER_V, "Center on cursor line")
{
    int offsety;
    if (Currentbuf->firstLine == NULL)
        return;
    offsety = getScreen()->ROWS / 2 - Currentbuf->cursorY;
    if (offsety != 0) {
        Currentbuf->topLine = lineSkip(Currentbuf, Currentbuf->topLine, -offsety, false);
        arrangeLine(Currentbuf);
    }
}

DEFUN(ctrCsrH, CENTER_H, "Center on cursor column")
{
    int offsetx;
    if (Currentbuf->firstLine == NULL)
        return;
    offsetx = Currentbuf->cursorX - getScreen()->COLS / 2;
    if (offsetx != 0) {
        columnSkip(Currentbuf, offsetx);
        arrangeCursor(Currentbuf);
    }
}

/* Redraw screen */
DEFUN(rdrwSc, REDRAW, "Draw the screen anew")
{
    vt_clear(getScreen());
    arrangeCursor(Currentbuf);
}

/* Search regular expression forward */

DEFUN(srchfor, SEARCH SEARCH_FORE WHEREIS, "Search forward")
{
    srch(forwardSearch, "Forward: ");
}

DEFUN(isrchfor, ISEARCH, "Incremental search forward")
{
    isrch(forwardSearch, "I-search: ");
}

/* Search regular expression backward */

DEFUN(srchbak, SEARCH_BACK, "Search backward")
{
    srch(backwardSearch, "Backward: ");
}

DEFUN(isrchbak, ISEARCH_BACK, "Incremental search backward")
{
    isrch(backwardSearch, "I-search backward: ");
}

/* Search next matching */
DEFUN(srchnxt, SEARCH_NEXT, "Continue search forward")
{
    srch_nxtprv(0);
}

/* Search previous matching */
DEFUN(srchprv, SEARCH_PREV, "Continue search backward")
{
    srch_nxtprv(1);
}

static void
cmd_loadURL(const char* url, struct Url* current, const char* referer, struct Form* post)
{
    // refresh(ttyWriter());
    struct Content c = loadGeneralFile(url, current, post, referer);
    Buffer* buf = makeBuffer(&c, false);
    if (buf == NULL) {
        /* FIXME: gettextize? */
        char* emsg = Sprintf("Can't load %s", conv_from_system(url))->ptr;
        message(getUI(), MSG_ERR, emsg);
    } else if (buf != NO_BUFFER) {
        pushBuffer(buf);
    }
}

static void
shiftvisualpos(Buffer* buf, int shift)
{
    Line* l = buf->currentLine;
    buf->visualpos -= shift;
    if (buf->visualpos - l->bwidth >= getScreen()->COLS)
        buf->visualpos = l->bwidth + getScreen()->COLS - 1;
    else if (buf->visualpos - l->bwidth < 0)
        buf->visualpos = l->bwidth;
    arrangeLine(buf);
    if (buf->visualpos - l->bwidth == -shift && buf->cursorX == 0)
        buf->visualpos = l->bwidth;
}

/* Shift screen left */
DEFUN(shiftl, SHIFT_LEFT, "Shift screen left")
{
    int column;

    if (Currentbuf->firstLine == NULL)
        return;
    column = Currentbuf->currentColumn;
    columnSkip(Currentbuf, searchKeyNum() * (-getScreen()->COLS + 1) + 1);
    shiftvisualpos(Currentbuf, Currentbuf->currentColumn - column);
}

/* Shift screen right */
DEFUN(shiftr, SHIFT_RIGHT, "Shift screen right")
{
    int column;

    if (Currentbuf->firstLine == NULL)
        return;
    column = Currentbuf->currentColumn;
    columnSkip(Currentbuf, searchKeyNum() * (getScreen()->COLS - 1) - 1);
    shiftvisualpos(Currentbuf, Currentbuf->currentColumn - column);
}

DEFUN(col1R, RIGHT, "Shift screen one column right")
{
    Buffer* buf = Currentbuf;
    Line* l = buf->currentLine;
    int j, column, n = searchKeyNum();

    if (l == NULL)
        return;
    for (j = 0; j < n; j++) {
        column = buf->currentColumn;
        columnSkip(Currentbuf, 1);
        if (column == buf->currentColumn)
            break;
        shiftvisualpos(Currentbuf, 1);
    }
}

DEFUN(col1L, LEFT, "Shift screen one column left")
{
    Buffer* buf = Currentbuf;
    Line* l = buf->currentLine;
    int j, n = searchKeyNum();

    if (l == NULL)
        return;
    for (j = 0; j < n; j++) {
        if (buf->currentColumn == 0)
            break;
        columnSkip(Currentbuf, -1);
        shiftvisualpos(Currentbuf, -1);
    }
}

DEFUN(setEnv, SETENV, "Set environment variable")
{
    char* env;
    char *var, *value;

    CurrentKeyData = NULL; /* not allowed in w3m-control: */
    env = searchKeyData();
    if (env == NULL || *env == '\0' || strchr(env, '=') == NULL) {
        if (env != NULL && *env != '\0')
            env = Sprintf("%s=", env)->ptr;
        env = inputStrHist(getUI(), "Set environ: ", env, TextHist);
        if (env == NULL || *env == '\0') {

            return;
        }
    }
    if ((value = strchr(env, '=')) != NULL && value > env) {
        var = allocStr(env, value - env);
        value++;
        set_environ(var, value);
    }
}

/* Execute shell command */
DEFUN(execsh, EXEC_SHELL SHELL, "Execute shell command and display output")
{
    char* cmd;

    CurrentKeyData = NULL; /* not allowed in w3m-control: */
    cmd = searchKeyData();
    if (cmd == NULL || *cmd == '\0') {
        cmd = inputLineHist(getUI(), "(exec shell)!", "", IN_COMMAND, ShellHist);
    }
    if (cmd != NULL)
        cmd = conv_to_system(cmd);
    if (cmd != NULL && *cmd != '\0') {
        fmTerm();
        printf("\n");
        (void)!system(cmd); /* We do not care about the exit code here! */
        /* FIXME: gettextize? */
        printf("\n[Hit any key]");
        fflush(stdout);
        fmInit();
        // getch();
    }
}

/* Load file */
DEFUN(ldfile, LOAD, "Open local file in a new buffer")
{
    char* fn;

    fn = searchKeyData();
    if (fn == NULL || *fn == '\0') {
        /* FIXME: gettextize? */
        fn = inputFilenameHist(getUI(), "(Load)Filename? ", NULL, LoadHist);
    }
    if (fn != NULL)
        fn = conv_to_system(fn);
    if (fn == NULL || *fn == '\0') {

        return;
    }
    cmd_loadfile(fn);
}

/* Load help file */
DEFUN(ldhelp, HELP, "Show help panel")
{
    char* lang;
    int n;
    Str tmp;

    lang = AcceptLang;
    n = strcspn(lang, ";, \t");
    tmp = Sprintf("file:///$LIB/" HELP_CGI CGI_EXTENSION "?version=%s&lang=%s",
        Str_form_quote(Strnew_charp(w3m_version))->ptr,
        Str_form_quote(Strnew_charp_n(lang, n))->ptr);
    cmd_loadURL(tmp->ptr, NULL, NO_REFERER, NULL);
}

static void
cmd_loadfile(char* fn)
{
    struct Content c = loadGeneralFile(file_to_url(fn, CurrentDir), NULL, NULL, NO_REFERER);
    Buffer* buf = makeBuffer(&c, false);
    if (buf == NULL) {
        /* FIXME: gettextize? */
        char* emsg = Sprintf("%s not found", conv_from_system(fn))->ptr;
        message(getUI(), MSG_ERR, emsg);
    } else if (buf != NO_BUFFER) {
        pushBuffer(buf);
    }
}

/* Move cursor left */
static void
_movL(int n)
{
    int i, m = searchKeyNum();
    if (Currentbuf->firstLine == NULL)
        return;
    for (i = 0; i < m; i++)
        cursorLeft(Currentbuf, n);
}

DEFUN(movL, MOVE_LEFT, "Cursor left")
{
    _movL(getScreen()->COLS / 2);
}

DEFUN(movL1, MOVE_LEFT1, "Cursor left. With edge touched, slide")
{
    _movL(1);
}

/* Move cursor downward */
static void
_movD(int n)
{
    int i, m = searchKeyNum();
    if (Currentbuf->firstLine == NULL)
        return;
    for (i = 0; i < m; i++)
        cursorDown(Currentbuf, n);
}

DEFUN(movD, MOVE_DOWN, "Cursor down")
{
    _movD((getScreen()->ROWS + 1) / 2);
}

DEFUN(movD1, MOVE_DOWN1, "Cursor down. With edge touched, slide")
{
    _movD(1);
}

/* move cursor upward */
static void
_movU(int n)
{
    int i, m = searchKeyNum();
    if (Currentbuf->firstLine == NULL)
        return;
    for (i = 0; i < m; i++)
        cursorUp(Currentbuf, n);
}

DEFUN(movU, MOVE_UP, "Cursor up")
{
    _movU((getScreen()->ROWS + 1) / 2);
}

DEFUN(movU1, MOVE_UP1, "Cursor up. With edge touched, slide")
{
    _movU(1);
}

/* Move cursor right */
static void
_movR(int n)
{
    int i, m = searchKeyNum();
    if (Currentbuf->firstLine == NULL)
        return;
    for (i = 0; i < m; i++)
        cursorRight(Currentbuf, n);
}

DEFUN(movR, MOVE_RIGHT, "Cursor right")
{
    _movR(getScreen()->COLS / 2);
}

DEFUN(movR1, MOVE_RIGHT1, "Cursor right. With edge touched, slide")
{
    _movR(1);
}

/* movLW, movRW */
/*
 * From: Takashi Nishimoto <g96p0935@mse.waseda.ac.jp> Date: Mon, 14 Jun
 * 1999 09:29:56 +0900
 */

#define nextChar(s, l) \
    do {               \
        (s)++;         \
    } while ((s) < (l)->len && (l)->propBuf[s] & PC_WCHAR2)
#define prevChar(s, l) \
    do {               \
        (s)--;         \
    } while ((s) > 0 && (l)->propBuf[s] & PC_WCHAR2)

static wc_uint32
getChar(char* p)
{
    return wc_any_to_ucs(wtf_parse1((wc_uchar**)&p));
}

static int
is_wordchar(wc_uint32 c)
{
    return wc_is_ucs_alnum(c);
}

static int
prev_nonnull_line(Line* line)
{
    Line* l;

    for (l = line; l != NULL && l->len == 0; l = l->prev)
        ;
    if (l == NULL || l->len == 0)
        return -1;

    Currentbuf->currentLine = l;
    if (l != line)
        Currentbuf->pos = Currentbuf->currentLine->len;
    return 0;
}

DEFUN(movLW, PREV_WORD, "Move to the previous word")
{
    char* lb;
    Line *pline, *l;
    int ppos;
    int i, n = searchKeyNum();

    if (Currentbuf->firstLine == NULL)
        return;

    for (i = 0; i < n; i++) {
        pline = Currentbuf->currentLine;
        ppos = Currentbuf->pos;

        if (prev_nonnull_line(Currentbuf->currentLine) < 0)
            goto end;

        while (1) {
            l = Currentbuf->currentLine;
            lb = l->lineBuf;
            while (Currentbuf->pos > 0) {
                int tmp = Currentbuf->pos;
                prevChar(tmp, l);
                if (is_wordchar(getChar(&lb[tmp])))
                    break;
                Currentbuf->pos = tmp;
            }
            if (Currentbuf->pos > 0)
                break;
            if (prev_nonnull_line(Currentbuf->currentLine->prev) < 0) {
                Currentbuf->currentLine = pline;
                Currentbuf->pos = ppos;
                goto end;
            }
            Currentbuf->pos = Currentbuf->currentLine->len;
        }

        l = Currentbuf->currentLine;
        lb = l->lineBuf;
        while (Currentbuf->pos > 0) {
            int tmp = Currentbuf->pos;
            prevChar(tmp, l);
            if (!is_wordchar(getChar(&lb[tmp])))
                break;
            Currentbuf->pos = tmp;
        }
    }
end:
    arrangeCursor(Currentbuf);
}

static int
next_nonnull_line(Line* line)
{
    Line* l;

    for (l = line; l != NULL && l->len == 0; l = l->next)
        ;

    if (l == NULL || l->len == 0)
        return -1;

    Currentbuf->currentLine = l;
    if (l != line)
        Currentbuf->pos = 0;
    return 0;
}

DEFUN(movRW, NEXT_WORD, "Move to the next word")
{
    char* lb;
    Line *pline, *l;
    int ppos;
    int i, n = searchKeyNum();

    if (Currentbuf->firstLine == NULL)
        return;

    for (i = 0; i < n; i++) {
        pline = Currentbuf->currentLine;
        ppos = Currentbuf->pos;

        if (next_nonnull_line(Currentbuf->currentLine) < 0)
            goto end;

        l = Currentbuf->currentLine;
        lb = l->lineBuf;
        while (Currentbuf->pos < l->len && is_wordchar(getChar(&lb[Currentbuf->pos])))
            nextChar(Currentbuf->pos, l);

        while (1) {
            while (Currentbuf->pos < l->len && !is_wordchar(getChar(&lb[Currentbuf->pos])))
                nextChar(Currentbuf->pos, l);
            if (Currentbuf->pos < l->len)
                break;
            if (next_nonnull_line(Currentbuf->currentLine->next) < 0) {
                Currentbuf->currentLine = pline;
                Currentbuf->pos = ppos;
                goto end;
            }
            Currentbuf->pos = 0;
            l = Currentbuf->currentLine;
            lb = l->lineBuf;
        }
    }
end:
    arrangeCursor(Currentbuf);
}

static void
_quitfm(int confirm)
{
    char* ans = "y";

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

/* Quit */
DEFUN(quitfm, ABORT EXIT, "Quit without confirmation")
{
    _quitfm(false);
}

/* Question and Quit */
DEFUN(qquitfm, QUIT, "Quit with confirmation request")
{
    _quitfm(confirm_on_quit);
}

/* Select buffer */
DEFUN(selBuf, SELECT, "Display buffer-stack panel")
{
    Buffer* buf;
    int ok;
    char cmd;

    ok = false;
    do {
        buf = selectBuffer(Firstbuf, Currentbuf, &cmd);
        switch (cmd) {
        case 'B':
            ok = true;
            break;
        case '\n':
        case ' ':
            Currentbuf = buf;
            ok = true;
            break;
        case 'D':
            delBuffer(buf);
            if (Firstbuf == NULL) {
                /* No more buffer */
                Firstbuf = nullBuffer();
                Currentbuf = Firstbuf;
            }
            break;
        case 'q':
            qquitfm();
            break;
        case 'Q':
            quitfm();
            break;
        }
    } while (!ok);

    for (buf = Firstbuf; buf != NULL; buf = buf->nextBuffer) {
        if (buf == Currentbuf)
            continue;
        deleteImage(buf);
        if (clear_buffer)
            tmpClearBuffer(buf);
    }
}

/* Suspend (on BSD), or run interactive shell (on SysV) */
DEFUN(susp, INTERRUPT SUSPEND, "Suspend w3m to background")
{
    struct VirtualTerm* vt = getScreen();
#ifndef SIGSTOP
    char* shell;
#endif /* not SIGSTOP */
    vt_move(vt, getLines() - 1, 0);
    vt_clrtoeolx(vt);
    // refresh(ttyWriter());
    fmTerm();
#ifndef SIGSTOP
    shell = getenv("SHELL");
    if (shell == NULL)
        shell = "/bin/sh";
    system(shell);
#else /* SIGSTOP */
    signal(SIGTSTP, SIG_DFL); /* just in case */
    /*
     * Note: If susp() was called from SIGTSTP handler,
     * unblocking SIGTSTP would be required here.
     * Currently not.
     */
    kill(0, SIGTSTP); /* stop whole job, not a single process */
#endif /* SIGSTOP */
    fmInit();
}

/* Go to specified line */
void _goLine(const char* l)
{
    if (l == NULL || *l == '\0' || Currentbuf->currentLine == NULL) {

        return;
    }
    Currentbuf->pos = 0;
    if (*l == '^') {
        Currentbuf->topLine = Currentbuf->currentLine = Currentbuf->firstLine;
    } else if (*l == '$') {
        Currentbuf->topLine = lineSkip(Currentbuf, Currentbuf->lastLine,
            -(getScreen()->ROWS + 1) / 2, true);
        Currentbuf->currentLine = Currentbuf->lastLine;
    } else
        gotoRealLine(Currentbuf, atoi(l));
    arrangeCursor(Currentbuf);
}

DEFUN(goLine, GOTO_LINE, "Go to the specified line")
{

    char* str = searchKeyData();
    if (str)
        _goLine(str);
    else
        /* FIXME: gettextize? */
        _goLine(inputStr(getUI(), "Goto line: ", ""));
}

DEFUN(goLineL, END, "Go to the last line")
{
    _goLine("$");
}

/* Go to the bottom of the line */
DEFUN(linend, LINE_END, "Go to the end of the line")
{
    if (Currentbuf->firstLine == NULL)
        return;
    while (Currentbuf->currentLine->next
        && Currentbuf->currentLine->next->bpos)
        cursorDown0(Currentbuf, 1);
    Currentbuf->pos = Currentbuf->currentLine->len - 1;
    arrangeCursor(Currentbuf);
}

static int
cur_real_linenumber(Buffer* buf)
{
    Line *l, *cur = buf->currentLine;
    int n;

    if (!cur)
        return 1;
    n = cur->real_linenumber ? cur->real_linenumber : 1;
    for (l = buf->firstLine; l && l != cur && l->real_linenumber == 0; l = l->next) { /* header */
        if (l->bpos == 0)
            n++;
    }
    return n;
}

/* Run editor on the current buffer */
DEFUN(editBf, EDIT, "Edit local source")
{
    char* fn = Currentbuf->filename;
    Str cmd;

    if (fn == NULL || (Currentbuf->type == NULL && Currentbuf->edit == NULL) || /* Reading shell */
        Currentbuf->real_scheme != SCM_LOCAL || !strcmp(Currentbuf->currentURL.file, "-") /* file is std input  */
    ) {
        message(getUI(), MSG_ERR, "Can't edit other than local file");
        return;
    }
    if (Currentbuf->edit)
        cmd = unquote_mailcap(Currentbuf->edit, Currentbuf->real_type, fn,
            getHttpHeaderValue(Currentbuf->document_header, "Content-Type:"), NULL);
    else
        cmd = myEditor(Editor, shell_quote(fn),
            cur_real_linenumber(Currentbuf));
    exec_cmd(cmd->ptr);

    reload();
}

/* Run editor on the current screen */
DEFUN(editScr, EDIT_SCREEN, "Edit rendered copy of document")
{
    char* tmpf = tmpfname(TMPF_DFL, NULL)->ptr;
    FILE* f = fopen(tmpf, "w");
    if (f == NULL) {
        /* FIXME: gettextize? */
        message(getUI(), MSG_ERR, Sprintf("Can't open %s", tmpf)->ptr);
        return;
    }
    saveBuffer(Currentbuf, f, true);
    fclose(f);
    exec_cmd(myEditor(Editor, shell_quote(tmpf),
        cur_real_linenumber(Currentbuf))
            ->ptr);
    unlink(tmpf);
}

/* Set / unset mark */
DEFUN(_mark, MARK, "Set/unset mark")
{
    Line* l;
    if (!use_mark)
        return;
    if (Currentbuf->firstLine == NULL)
        return;
    l = Currentbuf->currentLine;
    l->propBuf[Currentbuf->pos] ^= PE_MARK;
}

/* Go to next mark */
DEFUN(nextMk, NEXT_MARK, "Go to the next mark")
{
    Line* l;
    int i;

    if (!use_mark)
        return;
    if (Currentbuf->firstLine == NULL)
        return;
    i = Currentbuf->pos + 1;
    l = Currentbuf->currentLine;
    if (i >= l->len) {
        i = 0;
        l = l->next;
    }
    while (l != NULL) {
        for (; i < l->len; i++) {
            if (l->propBuf[i] & PE_MARK) {
                Currentbuf->currentLine = l;
                Currentbuf->pos = i;
                arrangeCursor(Currentbuf);

                return;
            }
        }
        l = l->next;
        i = 0;
    }
    /* FIXME: gettextize? */
    message(getUI(), MSG_INFO, "No mark exist after here");
}

/* Go to previous mark */
DEFUN(prevMk, PREV_MARK, "Go to the previous mark")
{
    Line* l;
    int i;

    if (!use_mark)
        return;
    if (Currentbuf->firstLine == NULL)
        return;
    i = Currentbuf->pos - 1;
    l = Currentbuf->currentLine;
    if (i < 0) {
        l = l->prev;
        if (l != NULL)
            i = l->len - 1;
    }
    while (l != NULL) {
        for (; i >= 0; i--) {
            if (l->propBuf[i] & PE_MARK) {
                Currentbuf->currentLine = l;
                Currentbuf->pos = i;
                arrangeCursor(Currentbuf);

                return;
            }
        }
        l = l->prev;
        if (l != NULL)
            i = l->len - 1;
    }
    /* FIXME: gettextize? */
    message(getUI(), MSG_INFO, "No mark exist before here");
}

static char* MarkString = NULL;

/* Mark place to which the regular expression matches */
DEFUN(reMark, REG_MARK, "Mark all occurences of a pattern")
{
    Line* l;
    char* str;
    char *p, *p1, *p2;

    if (!use_mark)
        return;
    str = searchKeyData();
    if (str == NULL || *str == '\0') {
        str = inputStrHist(getUI(), "(Mark)Regexp: ", MarkString, TextHist);
        if (str == NULL || *str == '\0') {

            return;
        }
    }
    str = conv_search_string(str, DisplayCharset);
    if ((str = regexCompile(str, 1)) != NULL) {
        message(getUI(), MSG_INFO, str);
        return;
    }
    MarkString = str;
    for (l = Currentbuf->firstLine; l != NULL; l = l->next) {
        p = l->lineBuf;
        for (;;) {
            if (regexMatch(p, &l->lineBuf[l->len] - p, p == l->lineBuf) == 1) {
                matchedPosition(&p1, &p2);
                l->propBuf[p1 - l->lineBuf] |= PE_MARK;
                p = p2;
            } else
                break;
        }
    }
}

static void
gotoLabel(const char* label)
{
    Anchor* al = searchURLLabel(Currentbuf, label);
    if (al == NULL) {
        /* FIXME: gettextize? */
        message(getUI(), MSG_INFO, Sprintf("%s is not found", label)->ptr);
        return;
    }

    Buffer* buf = newBuffer();
    copyBuffer(buf, Currentbuf);
    int i;
    for (i = 0; i < MAX_LB; i++)
        buf->linkBuffer[i] = NULL;
    buf->currentURL.label = allocStr(label, -1);
    pushHashHist(URLHist, parsedURL2Str(&buf->currentURL)->ptr);
    (*buf->clone)++;
    pushBuffer(buf);
    gotoLine(Currentbuf, al->start.line);
    if (label_topline)
        Currentbuf->topLine = lineSkip(Currentbuf, Currentbuf->topLine,
            Currentbuf->currentLine->linenumber
                - Currentbuf->topLine->linenumber,
            false);
    Currentbuf->pos = al->start.pos;
    arrangeCursor(Currentbuf);

    return;
}

static void followAnchor(bool do_download)
{
    Anchor* a;
    struct Url u;
    int x = 0, y = 0, map = 0;
    char* url;

    if (Currentbuf->firstLine == NULL)
        return;

    a = retrieveCurrentImg(Currentbuf);
    if (a && a->image && a->image->map) {
        _followForm(false, do_download);
        return;
    }
    if (a && a->image && a->image->ismap) {
        getMapXY(Currentbuf, a, &x, &y);
        map = 1;
    }
    a = retrieveCurrentAnchor(Currentbuf);
    if (a == NULL) {
        _followForm(false, do_download);
        return;
    }
    if (*a->url == '#') { /* index within this buffer */
        gotoLabel((char*)a->url + 1);
        return;
    }
    u = parseUrl(a->url, baseURL(Currentbuf));
    if (Strcmp(parsedURL2Str(&u), parsedURL2Str(&Currentbuf->currentURL)) == 0) {
        /* index within this buffer */
        if (u.label) {
            gotoLabel(u.label);
            return;
        }
    }
    url = (char*)a->url;
    if (map)
        url = Sprintf("%s?%d,%d", a->url, x, y)->ptr;

    loadLink(url, (char*)a->target, a->referer, NULL, do_download);
}

/* follow HREF link */
DEFUN(followA, GOTO_LINK, "Follow current hyperlink in a new buffer")
{
    followAnchor(false);
}

/* follow HREF link in the buffer */
void bufferA(void)
{
    followAnchor(false);
}

static void followImage(bool do_download)
{
    if (Currentbuf->firstLine == NULL)
        return;

    Anchor* a;
    a = retrieveCurrentImg(Currentbuf);
    if (a == NULL)
        return;
    /* FIXME: gettextize? */
    message(getUI(), MSG_INFO, Sprintf("loading %s", a->url)->ptr);
    // refresh(ttyWriter());
    struct Content c = loadGeneralFile(a->url, baseURL(Currentbuf), NULL, NULL);
    Buffer* buf = makeBuffer(&c, do_download);
    if (buf == NULL) {
        /* FIXME: gettextize? */
        char* emsg = Sprintf("Can't load %s", a->url)->ptr;
        message(getUI(), MSG_ERR, emsg);
    } else if (buf != NO_BUFFER) {
        pushBuffer(buf);
    }
}

/* view inline image */
DEFUN(followI, VIEW_IMAGE, "Display image in viewer")
{
    followImage(false);
}

/* submit form */
DEFUN(submitForm, SUBMIT, "Submit form")
{
    _followForm(true, false);
}

/* process form */
void followForm(void)
{
    _followForm(false, false);
}

/* go to the top anchor */
DEFUN(topA, LINK_BEGIN, "Move to the first hyperlink")
{
    HmarkerList* hl = Currentbuf->hmarklist;
    BufferPoint* po;
    Anchor* an;
    int hseq = 0;

    if (Currentbuf->firstLine == NULL)
        return;
    if (!hl || hl->nmark == 0)
        return;

    do {
        if (hseq >= hl->nmark)
            return;
        po = hl->marks + hseq;
        an = retrieveAnchor(Currentbuf->href, po->line, po->pos);
        if (an == NULL)
            an = retrieveAnchor(Currentbuf->formitem, po->line, po->pos);
        hseq++;
    } while (an == NULL);

    gotoLine(Currentbuf, po->line);
    Currentbuf->pos = po->pos;
    arrangeCursor(Currentbuf);
}

/* go to the last anchor */
DEFUN(lastA, LINK_END, "Move to the last hyperlink")
{
    HmarkerList* hl = Currentbuf->hmarklist;
    BufferPoint* po;
    Anchor* an;
    int hseq;

    if (Currentbuf->firstLine == NULL)
        return;
    if (!hl || hl->nmark == 0)
        return;

    hseq = hl->nmark - 1;
    do {
        if (hseq < 0)
            return;
        po = hl->marks + hseq;
        an = retrieveAnchor(Currentbuf->href, po->line, po->pos);
        if (an == NULL)
            an = retrieveAnchor(Currentbuf->formitem, po->line, po->pos);
        hseq--;
    } while (an == NULL);

    gotoLine(Currentbuf, po->line);
    Currentbuf->pos = po->pos;
    arrangeCursor(Currentbuf);
}

/* go to the nth anchor */
DEFUN(nthA, LINK_N, "Go to the nth link")
{
    HmarkerList* hl = Currentbuf->hmarklist;
    BufferPoint* po;
    Anchor* an;

    int n = searchKeyNum();
    if (n < 0 || n > hl->nmark)
        return;

    if (Currentbuf->firstLine == NULL)
        return;
    if (!hl || hl->nmark == 0)
        return;

    po = hl->marks + n - 1;
    an = retrieveAnchor(Currentbuf->href, po->line, po->pos);
    if (an == NULL)
        an = retrieveAnchor(Currentbuf->formitem, po->line, po->pos);
    if (an == NULL)
        return;

    gotoLine(Currentbuf, po->line);
    Currentbuf->pos = po->pos;
    arrangeCursor(Currentbuf);
}

/* go to the next anchor */
DEFUN(nextA, NEXT_LINK, "Move to the next hyperlink")
{
    _nextA(false);
}

/* go to the previous anchor */
DEFUN(prevA, PREV_LINK, "Move to the previous hyperlink")
{
    _prevA(false);
}

/* go to the next visited anchor */
DEFUN(nextVA, NEXT_VISITED, "Move to the next visited hyperlink")
{
    _nextA(true);
}

/* go to the previous visited anchor */
DEFUN(prevVA, PREV_VISITED, "Move to the previous visited hyperlink")
{
    _prevA(true);
}

/* go to the next [visited] anchor */
static void
_nextA(int visited)
{
    HmarkerList* hl = Currentbuf->hmarklist;
    BufferPoint* po;
    Anchor *an, *pan;
    int i, x, y, n = searchKeyNum();
    struct Url url;

    if (Currentbuf->firstLine == NULL)
        return;
    if (!hl || hl->nmark == 0)
        return;

    an = retrieveCurrentAnchor(Currentbuf);
    if (visited != true && an == NULL)
        an = retrieveCurrentForm(Currentbuf);

    y = Currentbuf->currentLine->linenumber;
    x = Currentbuf->pos;

    if (visited == true) {
        n = hl->nmark;
    }

    for (i = 0; i < n; i++) {
        pan = an;
        if (an && an->hseq >= 0) {
            int hseq = an->hseq + 1;
            do {
                if (hseq >= hl->nmark) {
                    if (visited == true)
                        return;
                    an = pan;
                    goto _end;
                }
                po = &hl->marks[hseq];
                an = retrieveAnchor(Currentbuf->href, po->line, po->pos);
                if (visited != true && an == NULL)
                    an = retrieveAnchor(Currentbuf->formitem, po->line,
                        po->pos);
                hseq++;
                if (visited == true && an) {
                    url = parseUrl(an->url, baseURL(Currentbuf));
                    if (getHashHist(URLHist, parsedURL2Str(&url)->ptr)) {
                        goto _end;
                    }
                }
            } while (an == NULL || an == pan);
        } else {
            an = closest_next_anchor(Currentbuf->href, NULL, x, y);
            if (visited != true)
                an = closest_next_anchor(Currentbuf->formitem, an, x, y);
            if (an == NULL) {
                if (visited == true)
                    return;
                an = pan;
                break;
            }
            x = an->start.pos;
            y = an->start.line;
            if (visited == true) {
                url = parseUrl(an->url, baseURL(Currentbuf));
                if (getHashHist(URLHist, parsedURL2Str(&url)->ptr)) {
                    goto _end;
                }
            }
        }
    }
    if (visited == true)
        return;

_end:
    if (an == NULL || an->hseq < 0)
        return;
    po = &hl->marks[an->hseq];
    gotoLine(Currentbuf, po->line);
    Currentbuf->pos = po->pos;
    arrangeCursor(Currentbuf);
}

/* go to the previous anchor */
static void
_prevA(int visited)
{
    HmarkerList* hl = Currentbuf->hmarklist;
    BufferPoint* po;
    Anchor *an, *pan;
    int i, x, y, n = searchKeyNum();
    struct Url url;

    if (Currentbuf->firstLine == NULL)
        return;
    if (!hl || hl->nmark == 0)
        return;

    an = retrieveCurrentAnchor(Currentbuf);
    if (visited != true && an == NULL)
        an = retrieveCurrentForm(Currentbuf);

    y = Currentbuf->currentLine->linenumber;
    x = Currentbuf->pos;

    if (visited == true) {
        n = hl->nmark;
    }

    for (i = 0; i < n; i++) {
        pan = an;
        if (an && an->hseq >= 0) {
            int hseq = an->hseq - 1;
            do {
                if (hseq < 0) {
                    if (visited == true)
                        return;
                    an = pan;
                    goto _end;
                }
                po = hl->marks + hseq;
                an = retrieveAnchor(Currentbuf->href, po->line, po->pos);
                if (visited != true && an == NULL)
                    an = retrieveAnchor(Currentbuf->formitem, po->line,
                        po->pos);
                hseq--;
                if (visited == true && an) {
                    url = parseUrl(an->url, baseURL(Currentbuf));
                    if (getHashHist(URLHist, parsedURL2Str(&url)->ptr)) {
                        goto _end;
                    }
                }
            } while (an == NULL || an == pan);
        } else {
            an = closest_prev_anchor(Currentbuf->href, NULL, x, y);
            if (visited != true)
                an = closest_prev_anchor(Currentbuf->formitem, an, x, y);
            if (an == NULL) {
                if (visited == true)
                    return;
                an = pan;
                break;
            }
            x = an->start.pos;
            y = an->start.line;
            if (visited == true && an) {
                url = parseUrl(an->url, baseURL(Currentbuf));
                if (getHashHist(URLHist, parsedURL2Str(&url)->ptr)) {
                    goto _end;
                }
            }
        }
    }
    if (visited == true)
        return;

_end:
    if (an == NULL || an->hseq < 0)
        return;
    po = hl->marks + an->hseq;
    gotoLine(Currentbuf, po->line);
    Currentbuf->pos = po->pos;
    arrangeCursor(Currentbuf);
}

/* go to the next left/right anchor */
static void
nextX(int d, int dy)
{
    HmarkerList* hl = Currentbuf->hmarklist;
    Anchor *an, *pan;
    Line* l;
    int i, x, y, n = searchKeyNum();

    if (Currentbuf->firstLine == NULL)
        return;
    if (!hl || hl->nmark == 0)
        return;

    an = retrieveCurrentAnchor(Currentbuf);
    if (an == NULL)
        an = retrieveCurrentForm(Currentbuf);

    l = Currentbuf->currentLine;
    x = Currentbuf->pos;
    y = l->linenumber;
    pan = NULL;
    for (i = 0; i < n; i++) {
        if (an)
            x = (d > 0) ? an->end.pos : an->start.pos - 1;
        an = NULL;
        while (1) {
            for (; x >= 0 && x < l->len; x += d) {
                an = retrieveAnchor(Currentbuf->href, y, x);
                if (!an)
                    an = retrieveAnchor(Currentbuf->formitem, y, x);
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
            x = (d > 0) ? 0 : l->len - 1;
            y = l->linenumber;
        }
        if (!an)
            break;
    }

    if (pan == NULL)
        return;
    gotoLine(Currentbuf, y);
    Currentbuf->pos = pan->start.pos;
    arrangeCursor(Currentbuf);
}

/* go to the next downward/upward anchor */
static void
nextY(int d)
{
    HmarkerList* hl = Currentbuf->hmarklist;
    Anchor *an, *pan;
    int i, x, y, n = searchKeyNum();
    int hseq;

    if (Currentbuf->firstLine == NULL)
        return;
    if (!hl || hl->nmark == 0)
        return;

    an = retrieveCurrentAnchor(Currentbuf);
    if (an == NULL)
        an = retrieveCurrentForm(Currentbuf);

    x = Currentbuf->pos;
    y = Currentbuf->currentLine->linenumber + d;
    pan = NULL;
    hseq = -1;
    for (i = 0; i < n; i++) {
        if (an)
            hseq = abs(an->hseq);
        an = NULL;
        for (; y >= 0 && y <= Currentbuf->lastLine->linenumber; y += d) {
            an = retrieveAnchor(Currentbuf->href, y, x);
            if (!an)
                an = retrieveAnchor(Currentbuf->formitem, y, x);
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
    gotoLine(Currentbuf, pan->start.line);
    arrangeLine(Currentbuf);
}

/* go to the next left anchor */
DEFUN(nextL, NEXT_LEFT, "Move left to the next hyperlink")
{
    nextX(-1, 0);
}

/* go to the next left-up anchor */
DEFUN(nextLU, NEXT_LEFT_UP, "Move left or upward to the next hyperlink")
{
    nextX(-1, -1);
}

/* go to the next right anchor */
DEFUN(nextR, NEXT_RIGHT, "Move right to the next hyperlink")
{
    nextX(1, 0);
}

/* go to the next right-down anchor */
DEFUN(nextRD, NEXT_RIGHT_DOWN, "Move right or downward to the next hyperlink")
{
    nextX(1, 1);
}

/* go to the next downward anchor */
DEFUN(nextD, NEXT_DOWN, "Move downward to the next hyperlink")
{
    nextY(1);
}

/* go to the next upward anchor */
DEFUN(nextU, NEXT_UP, "Move upward to the next hyperlink")
{
    nextY(-1);
}

/* go to the next bufferr */
DEFUN(nextBf, NEXT, "Switch to the next buffer")
{
    Currentbuf = prevBuffer(Firstbuf, Currentbuf);
}

/* go to the previous bufferr */
DEFUN(prevBf, PREV, "Switch to the previous buffer")
{
    Buffer* buf = Currentbuf->nextBuffer;
    if (!buf) {
        return;
    }
    Currentbuf = buf;
}

static int
checkBackBuffer(Buffer* buf)
{
    if (buf->nextBuffer)
        return true;

    return false;
}

/* delete current buffer and back to the previous buffer */
DEFUN(backBf, BACK, "Close current buffer and return to the one below in stack")
{
    if (!checkBackBuffer(Currentbuf)) {
        /* FIXME: gettextize? */
        message(getUI(), MSG_INFO, "Can't go back...");
        return;
    }

    delBuffer(Currentbuf);
}

DEFUN(deletePrevBuf, DELETE_PREVBUF, "Delete previous buffer (mainly for local CGI-scripts)")
{
    Buffer* buf = Currentbuf->nextBuffer;
    if (buf)
        delBuffer(buf);
}

/* go to specified URL */
static void
goURL0(char* prompt, int relative)
{
    const char* url;
    const char* referer;
    struct Url p_url, *current;
    Buffer* cur_buf = Currentbuf;
    const int* no_referer_ptr;

    url = searchKeyData();
    if (url == NULL) {
        struct Hist* hist = copyHist(URLHist);
        Anchor* a;

        current = baseURL(Currentbuf);
        if (current) {
            char* c_url = parsedURL2Str(current)->ptr;
            if (DefaultURLString == DEFAULT_URL_CURRENT)
                url = url_decode2(c_url, NULL);
            else
                pushHist(hist, c_url);
        }
        a = retrieveCurrentAnchor(Currentbuf);
        if (a) {
            char* a_url;
            p_url = parseUrl(a->url, current);
            a_url = parsedURL2Str(&p_url)->ptr;
            if (DefaultURLString == DEFAULT_URL_LINK)
                url = url_decode2(a_url, Currentbuf);
            else
                pushHist(hist, a_url);
        }
        url = inputLineHist(getUI(), prompt, url, IN_URL, hist);
        if (url != NULL)
            SKIP_BLANKS(url);
    }
    if (relative) {
        current = baseURL(Currentbuf);
        if ((no_referer_ptr && *no_referer_ptr) || current == NULL || current->scheme == SCM_LOCAL || current->scheme == SCM_LOCAL_CGI || current->scheme == SCM_DATA)
            referer = NO_REFERER;
        else
            referer = parsedURL2RefererStr(&Currentbuf->currentURL)->ptr;
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
        gotoLabel(url + 1);
        return;
    }
    p_url = parseUrl(url, current);
    pushHashHist(URLHist, parsedURL2Str(&p_url)->ptr);
    cmd_loadURL(url, current, referer, NULL);
    if (Currentbuf != cur_buf) /* success */
        pushHashHist(URLHist, parsedURL2Str(&Currentbuf->currentURL)->ptr);
}

DEFUN(goURL, GOTO, "Open specified document in a new buffer")
{
    goURL0("Goto URL: ", false);
}

DEFUN(goHome, GOTO_HOME, "Open home page in a new buffer")
{
    const char* url;
    if ((url = getenv("HTTP_HOME")) != NULL || (url = getenv("WWW_HOME")) != NULL) {
        struct Url p_url;
        Buffer* cur_buf = Currentbuf;
        SKIP_BLANKS(url);
        url = url_quote(url);
        p_url = parseUrl(url, NULL);
        pushHashHist(URLHist, parsedURL2Str(&p_url)->ptr);
        cmd_loadURL(url, NULL, NULL, NULL);
        if (Currentbuf != cur_buf) /* success */
            pushHashHist(URLHist, parsedURL2Str(&Currentbuf->currentURL)->ptr);
    }
}

DEFUN(gorURL, GOTO_RELATIVE, "Go to relative address")
{
    goURL0("Goto relative URL: ", true);
}

static void
cmd_loadBuffer(Buffer* buf, int prop, int linkid)
{
    if (buf == NULL) {
        message(getUI(), MSG_ERR, "Can't load string");
    } else if (buf != NO_BUFFER) {
        buf->bufferprop |= (BP_INTERNAL | prop);
        if (!(buf->bufferprop & BP_NO_URL))
            buf->currentURL = copyParsedUrl(&Currentbuf->currentURL);
        if (linkid != LB_NOLINK) {
            buf->linkBuffer[REV_LB[linkid]] = Currentbuf;
            Currentbuf->linkBuffer[linkid] = buf;
        }
        pushBuffer(buf);
    }
}

/* load bookmark */
DEFUN(ldBmark, BOOKMARK VIEW_BOOKMARK, "View bookmarks")
{
    cmd_loadURL(BookmarkFile, NULL, NO_REFERER, NULL);
}

#define W3MBOOKMARK_CMDNAME "w3mbookmark"
// #define W3MBOOKMARK_CMDNAME "w3mbookmark.exe"

/* Add current to bookmark */
DEFUN(adBmark, ADD_BOOKMARK, "Add current page to bookmarks")
{
    Str tmp;
    struct Form* request;

    tmp = Sprintf("mode=panel&cookie=%s&bmark=%s&url=%s&title=%s"
                  "&charset=%s",
        (Str_form_quote(localCookie()))->ptr,
        (Str_form_quote(Strnew_charp(BookmarkFile)))->ptr,
        (Str_form_quote(parsedURL2Str(&Currentbuf->currentURL)))->ptr,
        (Str_form_quote(wc_conv_strict(Currentbuf->buffername,
             InnerCharset,
             BookmarkCharset)))
            ->ptr,
        wc_ces_to_charset(BookmarkCharset));
    request = newFormList(NULL, "post", NULL, NULL, NULL, NULL, NULL);
    request->body = tmp->ptr;
    request->length = tmp->length;
    cmd_loadURL("file:///$LIB/" W3MBOOKMARK_CMDNAME, NULL, NO_REFERER, request);
}

/* option setting */
DEFUN(ldOpt, OPTIONS, "Display options setting panel")
{
    cmd_loadBuffer(load_option_panel(), BP_NO_URL, LB_NOLINK);
}

/* set an option */
DEFUN(setOpt, SET_OPTION, "Set option")
{
    char* opt;

    CurrentKeyData = NULL; /* not allowed in w3m-control: */
    opt = searchKeyData();
    if (opt == NULL || *opt == '\0' || strchr(opt, '=') == NULL) {
        if (opt != NULL && *opt != '\0') {
            char* v = get_param_option(opt);
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
    cmd_loadBuffer(message_list_panel(), BP_NO_URL, LB_NOLINK);
}

/* page info */
DEFUN(pginfo, INFO, "Display information about the current document")
{
    Buffer* buf;

    if ((buf = Currentbuf->linkBuffer[LB_N_INFO]) != NULL) {
        Currentbuf = buf;

        return;
    }
    if ((buf = Currentbuf->linkBuffer[LB_INFO]) != NULL)
        delBuffer(buf);
    buf = page_info_panel(Currentbuf);
    cmd_loadBuffer(buf, BP_NORMAL, LB_INFO);
}

void follow_map(struct KeyValue* arg)
{
    const char* name = tag_get_value(arg, "link");
    Anchor* an;
    MapArea* a;
    int x, y;
    struct Url p_url;

    an = retrieveCurrentImg(Currentbuf);
    x = Currentbuf->cursorX;
    y = Currentbuf->cursorY;
    a = follow_map_menu(Currentbuf, (char*)name, an, x, y);
    if (a == NULL || a->url == NULL || *(a->url) == '\0') {
        return;
    }
    if (*(a->url) == '#') {
        gotoLabel(a->url + 1);
        return;
    }
    p_url = parseUrl(a->url, baseURL(Currentbuf));
    pushHashHist(URLHist, parsedURL2Str(&p_url)->ptr);
    cmd_loadURL(a->url, baseURL(Currentbuf),
        parsedURL2Str(&Currentbuf->currentURL)->ptr, NULL);
}

/* link menu */
DEFUN(linkMn, LINK_MENU, "Pop up link element menu")
{
    LinkList* l = link_menu(Currentbuf);
    struct Url p_url;

    if (!l || !l->url)
        return;
    if (*(l->url) == '#') {
        gotoLabel(l->url + 1);
        return;
    }
    p_url = parseUrl(l->url, baseURL(Currentbuf));
    pushHashHist(URLHist, parsedURL2Str(&p_url)->ptr);
    cmd_loadURL(l->url, baseURL(Currentbuf),
        parsedURL2Str(&Currentbuf->currentURL)->ptr, NULL);
}

static void
anchorMn(Anchor* (*menu_func)(Buffer*), int go)
{
    Anchor* a;
    BufferPoint* po;

    if (!Currentbuf->href || !Currentbuf->hmarklist)
        return;
    a = menu_func(Currentbuf);
    if (!a || a->hseq < 0)
        return;
    po = &Currentbuf->hmarklist->marks[a->hseq];
    gotoLine(Currentbuf, po->line);
    Currentbuf->pos = po->pos;
    arrangeCursor(Currentbuf);

    if (go)
        followAnchor(false);
}

/* accesskey */
DEFUN(accessKey, ACCESSKEY, "Pop up accesskey menu")
{
    anchorMn(accesskey_menu, true);
}

/* list menu */
DEFUN(listMn, LIST_MENU, "Pop up menu for hyperlinks to browse to")
{
    anchorMn(list_menu, true);
}

DEFUN(movlistMn, MOVE_LIST_MENU, "Pop up menu to navigate between hyperlinks")
{
    anchorMn(list_menu, false);
}

/* link,anchor,image list */
DEFUN(linkLst, LIST, "Show all URLs referenced")
{
    Buffer* buf = link_list_panel(Currentbuf);
    if (buf) {
        cmd_loadBuffer(buf, BP_NORMAL, LB_NOLINK);
    }
}

/* cookie list */
DEFUN(cooLst, COOKIE, "View cookie list")
{
    Buffer* buf;

    buf = cookie_list_panel();
    if (buf != NULL)
        cmd_loadBuffer(buf, BP_NO_URL, LB_NOLINK);
}

/* History page */
DEFUN(ldHist, HISTORY, "Show browsing history")
{
    cmd_loadBuffer(historyBuffer(URLHist), BP_NO_URL, LB_NOLINK);
}

/* download HREF link */
DEFUN(svA, SAVE_LINK, "Save hyperlink target")
{
    CurrentKeyData = NULL; /* not allowed in w3m-control: */
    followAnchor(true);
}

/* download IMG link */
DEFUN(svI, SAVE_IMAGE, "Save inline image")
{
    CurrentKeyData = NULL; /* not allowed in w3m-control: */
    followImage(true);
}

/* save buffer */
DEFUN(svBuf, PRINT SAVE_SCREEN, "Save rendered document")
{
    char* qfile = NULL;
    FILE* f;
    int is_pipe;

    CurrentKeyData = NULL; /* not allowed in w3m-control: */
    const char* file;
    file = searchKeyData();
    if (file == NULL || *file == '\0') {
        /* FIXME: gettextize? */
        qfile = inputLineHist(getUI(), "Save buffer to: ", NULL, IN_COMMAND, SaveHist);
        if (qfile == NULL || *qfile == '\0') {

            return;
        }
    }
    file = conv_to_system(qfile ? qfile : file);
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
    saveBuffer(Currentbuf, f, true);
    if (is_pipe)
        pclose(f);
    else
        fclose(f);
}

/* save source */
DEFUN(svSrc, DOWNLOAD SAVE, "Save document source")
{
    if (Currentbuf->sourcefile == NULL)
        return;
    CurrentKeyData = NULL; /* not allowed in w3m-control: */
    PermitSaveToPipe = true;
    const char* file;
    // if (Currentbuf->real_scheme == SCM_LOCAL)
    //     file = conv_from_system(guessSaveName(NULL, Currentbuf->currentURL.real_file));
    // else
    file = guessSaveName(Currentbuf->document_header, Currentbuf->currentURL.file);
    doFileCopy(Currentbuf->sourcefile, file);
    PermitSaveToPipe = false;
}

static void
_peekURL(int only_img)
{

    Anchor* a;
    struct Url pu;
    static Str s = NULL;
    static Lineprop* p = NULL;
    Lineprop* pp;
    static int offset = 0, n;

    if (Currentbuf->firstLine == NULL)
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
    a = (only_img ? NULL : retrieveCurrentAnchor(Currentbuf));
    if (a == NULL) {
        a = (only_img ? NULL : retrieveCurrentForm(Currentbuf));
        if (a == NULL) {
            a = retrieveCurrentImg(Currentbuf);
            if (a == NULL)
                return;
        } else
            s = Strnew_charp(form2str((struct FormItem*)a->url));
    }
    if (s == NULL) {
        pu = parseUrl(a->url, baseURL(Currentbuf));
        s = parsedURL2Str(&pu);
    }
    if (DecodeURL)
        s = Strnew_charp(url_decode2(s->ptr, Currentbuf));
    s = checkType(s, &pp, NULL);
    p = NewAtom_N(Lineprop, s->length);
    memcpy((void*)p, (void*)pp, s->length * sizeof(Lineprop));
disp:
    n = searchKeyNum();
    if (n > 1 && s->length > (n - 1) * (getCols() - 1))
        offset = (n - 1) * (getCols() - 1);
    while (offset < s->length && p[offset] & PC_WCHAR2)
        offset++;
    message(getUI(), MSG_INFO, &s->ptr[offset]);
}

/* peek URL */
DEFUN(peekURL, PEEK_LINK, "Show target address")
{
    _peekURL(0);
}

/* peek URL of image */
DEFUN(peekIMG, PEEK_IMG, "Show image address")
{
    _peekURL(1);
}

/* show current URL */
static Str
currentURL(void)
{
    if (Currentbuf->bufferprop & BP_INTERNAL)
        return Strnew_size(0);
    return parsedURL2Str(&Currentbuf->currentURL);
}

DEFUN(curURL, PEEK, "Show current address")
{
    static Str s = NULL;
    static Lineprop* p = NULL;
    Lineprop* pp;
    static int offset = 0, n;

    if (Currentbuf->bufferprop & BP_INTERNAL)
        return;
    if (CurrentKey == prev_key && s != NULL) {
        if (s->length - offset >= getCols())
            offset++;
        else if (s->length <= offset) /* bug ? */
            offset = 0;
    } else {
        offset = 0;
        s = currentURL();
        if (DecodeURL)
            s = Strnew_charp(url_decode2(s->ptr, NULL));
        s = checkType(s, &pp, NULL);
        p = NewAtom_N(Lineprop, s->length);
        memcpy(p, pp, s->length * sizeof(Lineprop));
    }
    n = searchKeyNum();
    if (n > 1 && s->length > (n - 1) * (getCols() - 1))
        offset = (n - 1) * (getCols() - 1);
    while (offset < s->length && p[offset] & PC_WCHAR2)
        offset++;
    message(getUI(), MSG_INFO, &s->ptr[offset]);
}
/* view HTML source */

DEFUN(vwSrc, SOURCE VIEW, "Toggle between HTML shown or processed")
{
    if (Currentbuf->type == NULL)
        return;

    Buffer* buf;
    if ((buf = Currentbuf->linkBuffer[LB_SOURCE]) != NULL || (buf = Currentbuf->linkBuffer[LB_N_SOURCE]) != NULL) {
        Currentbuf = buf;

        return;
    }
    if (Currentbuf->sourcefile == NULL) {
        return;
    }

    buf = newBuffer();

    if (is_html_type(Currentbuf->type)) {
        buf->type = "text/plain";
        if (Currentbuf->real_type && is_html_type(Currentbuf->real_type))
            buf->real_type = "text/plain";
        else
            buf->real_type = Currentbuf->real_type;
        buf->buffername = Sprintf("source of %s", Currentbuf->buffername)->ptr;
        buf->linkBuffer[LB_N_SOURCE] = Currentbuf;
        Currentbuf->linkBuffer[LB_SOURCE] = buf;
    } else if (!strcasecmp(Currentbuf->type, "text/plain")) {
        buf->type = "text/html";
        if (Currentbuf->real_type && !strcasecmp(Currentbuf->real_type, "text/plain"))
            buf->real_type = "text/html";
        else
            buf->real_type = Currentbuf->real_type;
        buf->buffername = Sprintf("HTML view of %s",
            Currentbuf->buffername)
                              ->ptr;
        buf->linkBuffer[LB_SOURCE] = Currentbuf;
        Currentbuf->linkBuffer[LB_N_SOURCE] = buf;
    } else {
        return;
    }
    buf->currentURL = Currentbuf->currentURL;
    buf->real_scheme = Currentbuf->real_scheme;
    buf->filename = Currentbuf->filename;
    buf->sourcefile = Currentbuf->sourcefile;
    buf->document_charset = Currentbuf->document_charset;
    buf->clone = Currentbuf->clone;
    (*buf->clone)++;

    pushBuffer(buf);
}

/* reload */
DEFUN(reload, RELOAD, "Load current document anew")
{
    Buffer *buf, *fbuf = NULL, sbuf;
    wc_ces old_charset;
    Str url;
    int multipart;

    if (Currentbuf->bufferprop & BP_INTERNAL) {
        if (!strcmp(Currentbuf->buffername, DOWNLOAD_LIST_TITLE)) {
            ldDL();
            return;
        }
        /* FIXME: gettextize? */
        message(getUI(), MSG_ERR, "Can't reload...");
        return;
    }
    if (Currentbuf->currentURL.scheme == SCM_LOCAL && !strcmp(Currentbuf->currentURL.file, "-")) {
        /* file is std input */
        /* FIXME: gettextize? */
        message(getUI(), MSG_ERR, "Can't reload stdin");
        return;
    }
    copyBuffer(&sbuf, Currentbuf);
    multipart = 0;

    struct Form* post;
    if (Currentbuf->form_submit) {
        post = Currentbuf->form_submit->parent;
        if (post->method == FORM_METHOD_POST
            && post->enctype == FORM_ENCTYPE_MULTIPART) {
            Str query;
            struct stat st;
            multipart = 1;
            query_from_followform(&query, Currentbuf->form_submit, multipart);
            stat(post->body, &st);
            post->length = st.st_size;
        }
    } else {
        post = NULL;
    }
    url = parsedURL2Str(&Currentbuf->currentURL);
    /* FIXME: gettextize? */
    message(getUI(), MSG_INFO, "Reloading...");
    // refresh(ttyWriter());
    old_charset = DocumentCharset;
    if (Currentbuf->document_charset != WC_CES_US_ASCII)
        DocumentCharset = Currentbuf->document_charset;
    // SearchHeader = Currentbuf->search_header;
    DefaultType = (char*)Currentbuf->real_type;
    struct Content c = loadGeneralFile(url->ptr, NULL, post, NO_REFERER /*, true*/);
    buf = makeBuffer(&c, false);
    DocumentCharset = old_charset;
    // SearchHeader = false;
    DefaultType = NULL;

    if (multipart)
        unlink(post->body);
    if (buf == NULL) {
        /* FIXME: gettextize? */
        message(getUI(), MSG_ERR, "Can't reload...");
        return;
    } else if (buf == NO_BUFFER) {

        return;
    }
    if (fbuf != NULL)
        Firstbuf = deleteBuffer(Firstbuf, fbuf);
    repBuffer(Currentbuf, buf);
    if ((buf->type != NULL) && (sbuf.type != NULL) && ((!strcasecmp(buf->type, "text/plain") && is_html_type(sbuf.type)) || (is_html_type(buf->type) && !strcasecmp(sbuf.type, "text/plain")))) {
        vwSrc();
        if (Currentbuf != buf)
            Firstbuf = deleteBuffer(Firstbuf, buf);
    }
    Currentbuf->form_submit = sbuf.form_submit;
    if (Currentbuf->firstLine) {
        // COPY_BUFROOT(Currentbuf, &sbuf);
        // restorePosition(Currentbuf, &sbuf);
    }
}

/* reshape */
DEFUN(reshape, RESHAPE, "Re-render document")
{
    Currentbuf->width = 0;
}

static void
_docCSet(wc_ces charset)
{
    if (Currentbuf->bufferprop & BP_INTERNAL)
        return;
    if (Currentbuf->sourcefile == NULL) {
        message(getUI(), MSG_INFO, "Can't reload...");
        return;
    }
    Currentbuf->document_charset = charset;
}

void change_charset(struct KeyValue* arg)
{
    Buffer* buf = Currentbuf->linkBuffer[LB_N_INFO];
    wc_ces charset;

    if (buf == NULL)
        return;
    delBuffer(Currentbuf);
    Currentbuf = buf;
    if (Currentbuf->bufferprop & BP_INTERNAL)
        return;
    charset = Currentbuf->document_charset;
    for (; arg; arg = arg->next) {
        if (!strcmp(arg->arg, "charset"))
            charset = atoi(arg->value);
    }
    _docCSet(charset);
}

DEFUN(docCSet, CHARSET, "Change the character encoding for the current document")
{
    char* cs;
    wc_ces charset;

    cs = searchKeyData();
    if (cs == NULL || *cs == '\0')
        /* FIXME: gettextize? */
        cs = inputStr(getUI(), "Document charset: ",
            wc_ces_to_charset(Currentbuf->document_charset));
    charset = wc_guess_charset_short(cs, 0);
    if (charset == 0) {

        return;
    }
    _docCSet(charset);
}

DEFUN(defCSet, DEFAULT_CHARSET, "Change the default character encoding")
{
    char* cs;
    wc_ces charset;

    cs = searchKeyData();
    if (cs == NULL || *cs == '\0')
        /* FIXME: gettextize? */
        cs = inputStr(getUI(), "Default document charset: ",
            wc_ces_to_charset(DocumentCharset));
    charset = wc_guess_charset_short(cs, 0);
    if (charset != 0)
        DocumentCharset = charset;
}

/* mark URL-like patterns as anchors */
void chkURLBuffer(Buffer* buf)
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
    int i;
    for (i = 0; url_like_pat[i]; i++) {
        reAnchor(buf, url_like_pat[i]);
    }
    buf->check_url |= CHK_URL;
}

DEFUN(chkURL, MARK_URL, "Turn URL-like strings into hyperlinks")
{
    chkURLBuffer(Currentbuf);
}

DEFUN(chkWORD, MARK_WORD, "Turn current word into hyperlink")
{
    char* p;
    int spos, epos;
    p = getCurWord(Currentbuf, &spos, &epos);
    if (p == NULL)
        return;
    reAnchorWord(Currentbuf, Currentbuf->currentLine, spos, epos);
}

/* show current line number and number of lines in the entire document */
DEFUN(curlno, LINE_INFO, "Display current position in document")
{
    Line* l = Currentbuf->currentLine;
    Str tmp;
    int cur = 0, all = 0, col = 0, len = 0;

    if (l != NULL) {
        cur = l->real_linenumber;
        col = l->bwidth + Currentbuf->currentColumn + Currentbuf->cursorX + 1;
        while (l->next && l->next->bpos)
            l = l->next;
        if (l->width < 0)
            l->width = COLPOS(l, l->len);
        len = l->bwidth + l->width;
    }
    if (Currentbuf->lastLine)
        all = Currentbuf->lastLine->real_linenumber;
    tmp = Sprintf("line %d/%d (%d%%) col %d/%d", cur, all,
        (int)((double)cur * 100.0 / (double)(all ? all : 1)
            + 0.5),
        col, len);
    Strcat_charp(tmp, "  ");
    Strcat_charp(tmp, wc_ces_to_charset_desc(Currentbuf->document_charset));

    message(getUI(), MSG_INFO, tmp->ptr);
}

DEFUN(dispI, DISPLAY_IMAGE, "Restart loading and drawing of images")
{
    if (!displayImage)
        initImage();
    if (!activeImage)
        return;
    displayImage = true;
    /*
     * if (!(Currentbuf->type && is_html_type(Currentbuf->type)))
     * return;
     */
    Currentbuf->image_flag = IMG_FLAG_AUTO;
}

DEFUN(stopI, STOP_IMAGE, "Stop loading and drawing of images")
{
    if (!activeImage)
        return;
    /*
     * if (!(Currentbuf->type && is_html_type(Currentbuf->type)))
     * return;
     */
    Currentbuf->image_flag = IMG_FLAG_SKIP;
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
getCurWord(Buffer* buf, int* spos, int* epos)
{
    char* p;
    Line* l = buf->currentLine;
    int b, e;

    *spos = 0;
    *epos = 0;
    if (l == NULL)
        return NULL;
    p = l->lineBuf;
    e = buf->pos;
    while (e > 0 && !is_wordchar(getChar(&p[e])))
        prevChar(e, l);
    if (!is_wordchar(getChar(&p[e])))
        return NULL;
    b = e;
    while (b > 0) {
        int tmp = b;
        prevChar(tmp, l);
        if (!is_wordchar(getChar(&p[tmp])))
            break;
        b = tmp;
    }
    while (e < l->len && is_wordchar(getChar(&p[e])))
        nextChar(e, l);
    *spos = b;
    *epos = e;
    return &p[b];
}

static char*
GetWord(Buffer* buf)
{
    int b, e;
    char* p;

    if ((p = getCurWord(buf, &b, &e)) != NULL) {
        return Strnew_charp_n(p, e - b)->ptr;
    }
    return NULL;
}

static void
execdict(char* word)
{
    if (!UseDictCommand || word == NULL || *word == '\0') {

        return;
    }

    char *w, *dictcmd;
    w = conv_to_system(word);
    if (*w == '\0') {

        return;
    }
    dictcmd = Sprintf("%s?%s", DictCommand,
        Str_form_quote(Strnew_charp(w))->ptr)
                  ->ptr;
    struct Content c = loadGeneralFile(dictcmd, NULL, NULL, NO_REFERER);
    Buffer* buf = makeBuffer(&c, false);
    if (buf == NULL) {
        message(getUI(), MSG_INFO, "Execution failed");
        return;
    } else if (buf != NO_BUFFER) {
        buf->filename = w;
        buf->buffername = Sprintf("%s %s", DICTBUFFERNAME, word)->ptr;
        if (buf->type == NULL)
            buf->type = "text/plain";
        pushBuffer(buf);
    }
}

DEFUN(dictword, DICT_WORD, "Execute dictionary command (see README.dict)")
{
    execdict(inputStr(getUI(), "(dictionary)!", ""));
}

DEFUN(dictwordat, DICT_WORD_AT,
    "Execute dictionary command for word at cursor")
{
    execdict(GetWord(Currentbuf));
}

void set_buffer_environ(Buffer* buf)
{
    static Buffer* prev_buf = NULL;
    static Line* prev_line = NULL;
    static int prev_pos = -1;
    Line* l;

    if (buf == NULL)
        return;
    if (buf != prev_buf) {
        set_environ("W3M_SOURCEFILE", buf->sourcefile);
        set_environ("W3M_FILENAME", buf->filename);
        set_environ("W3M_TITLE", buf->buffername);
        set_environ("W3M_URL", parsedURL2Str(&buf->currentURL)->ptr);
        set_environ("W3M_TYPE", buf->real_type ? buf->real_type : "unknown");
        set_environ("W3M_CHARSET", wc_ces_to_charset(buf->document_charset));
    }
    l = buf->currentLine;
    if (l && (buf != prev_buf || l != prev_line || buf->pos != prev_pos)) {
        Anchor* a;
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
        set_environ("W3M_CURRENT_LINE", Sprintf("%ld", l->real_linenumber)->ptr);
        set_environ("W3M_CURRENT_COLUMN", Sprintf("%d", buf->currentColumn + buf->cursorX + 1)->ptr);
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

char* searchKeyData(void)
{
    char* data = NULL;

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
    char* d;
    int n = 1;

    d = searchKeyData();
    if (d != NULL)
        n = atoi(d);
    return n;
}

void deleteFiles()
{
    while (Firstbuf && Firstbuf != NO_BUFFER) {
        Buffer* buf = Firstbuf->nextBuffer;
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
        func();
        CurrentCmdData = NULL;
    }
}

static MySignalHandler
SigAlarm(int _dummy)
{
    char* data;

    if (CurrentAlarm->sec > 0) {
        CurrentKey = -1;
        CurrentKeyData = NULL;
        CurrentCmdData = data = (char*)CurrentAlarm->data;
        w3mFuncList[CurrentAlarm->cmd].func();
        CurrentCmdData = NULL;
        if (CurrentAlarm->status == AL_IMPLICIT_ONCE) {
            CurrentAlarm->sec = 0;
            CurrentAlarm->status = AL_UNSET;
        }
        if (Currentbuf->event) {
            if (Currentbuf->event->status != AL_UNSET)
                CurrentAlarm = Currentbuf->event;
            else
                Currentbuf->event = NULL;
        }
        if (!Currentbuf->event)
            CurrentAlarm = &DefaultAlarm;
        if (CurrentAlarm->sec > 0) {
            mySignal(SIGALRM, SigAlarm);
            alarm(CurrentAlarm->sec);
        }
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
    char* resource = searchKeyData();

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
        initMimeTypes();
        return;
    }

    message(getUI(), MSG_ERR, Sprintf("Don't know how to reinitialize '%s'", resource)->ptr);
}

DEFUN(defKey, DEFINE_KEY, "Define a binding between a key stroke combination and a command")
{
    char* data;

    CurrentKeyData = NULL; /* not allowed in w3m-control: */
    data = searchKeyData();
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

static Buffer*
DownloadListBuffer(void)
{
    DownloadList* d;
    Str src = NULL;
    struct stat st;
    time_t cur_time;
    int duration, rate, eta;
    size_t size;

    if (!FirstDL)
        return NULL;
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
    return loadHTMLString(src);
}

void download_action(struct KeyValue* arg)
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
    ldDL();
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
    Buffer* buf;
    int replace = false, new_tab = false;
    int reload;

    if (Currentbuf->bufferprop & BP_INTERNAL && !strcmp(Currentbuf->buffername, DOWNLOAD_LIST_TITLE))
        replace = true;
    if (!FirstDL) {
        if (replace) {
            if (Currentbuf == Firstbuf && Currentbuf->nextBuffer == NULL) {
            } else
                delBuffer(Currentbuf);
        }
        return;
    }
    reload = checkDownloadList();
    buf = DownloadListBuffer();
    if (!buf) {

        return;
    }
    buf->bufferprop |= (BP_INTERNAL | BP_NO_URL);
    if (replace) {
        // COPY_BUFROOT(buf, Currentbuf);
        // restorePosition(buf, Currentbuf);
    }
    pushBuffer(buf);
    if (replace)
        deletePrevBuf();
    if (reload)
        Currentbuf->event = setAlarmEvent(Currentbuf->event, 1, AL_IMPLICIT,
            FUNCNAME_reload, NULL);
}

static void
save_buffer_position(Buffer* buf)
{
    BufferPos* b = buf->undo;

    if (!buf->firstLine)
        return;
    if (b && b->top_linenumber == TOP_LINENUMBER(buf) && b->cur_linenumber == CUR_LINENUMBER(buf) && b->currentColumn == buf->currentColumn && b->pos == buf->pos)
        return;
    b = New(BufferPos);
    b->top_linenumber = TOP_LINENUMBER(buf);
    b->cur_linenumber = CUR_LINENUMBER(buf);
    b->currentColumn = buf->currentColumn;
    b->pos = buf->pos;
    b->bpos = buf->currentLine ? buf->currentLine->bpos : 0;
    b->next = NULL;
    b->prev = buf->undo;
    if (buf->undo)
        buf->undo->next = b;
    buf->undo = b;
}

static void
resetPos(BufferPos* b)
{
    Buffer buf;
    Line top, cur;

    top.linenumber = b->top_linenumber;
    cur.linenumber = b->cur_linenumber;
    cur.bpos = b->bpos;
    buf.topLine = &top;
    buf.currentLine = &cur;
    buf.pos = b->pos;
    buf.currentColumn = b->currentColumn;
    restorePosition(Currentbuf, &buf);
    Currentbuf->undo = b;
}

DEFUN(undoPos, UNDO, "Cancel the last cursor movement")
{
    BufferPos* b = Currentbuf->undo;
    int i;

    if (!Currentbuf->firstLine)
        return;
    if (!b || !b->prev)
        return;

    resetPos(b);
}

DEFUN(redoPos, REDO, "Cancel the last undo")
{
    BufferPos* b = Currentbuf->undo;
    int i;

    if (!Currentbuf->firstLine)
        return;
    if (!b || !b->next)
        return;

    resetPos(b);
}

DEFUN(cursorTop, CURSOR_TOP, "Move cursor to the top of the screen")
{
    if (Currentbuf->firstLine == NULL)
        return;
    Currentbuf->currentLine = lineSkip(Currentbuf, Currentbuf->topLine,
        0, false);
    arrangeLine(Currentbuf);
}

DEFUN(cursorMiddle, CURSOR_MIDDLE, "Move cursor to the middle of the screen")
{
    if (Currentbuf->firstLine == NULL)
        return;
    int offsety = (getScreen()->ROWS - 1) / 2;
    Currentbuf->currentLine = currentLineSkip(Currentbuf, Currentbuf->topLine,
        offsety, false);
    arrangeLine(Currentbuf);
}

DEFUN(cursorBottom, CURSOR_BOTTOM, "Move cursor to the bottom of the screen")
{
    if (Currentbuf->firstLine == NULL)
        return;
    int offsety = getScreen()->ROWS - 1;
    Currentbuf->currentLine = currentLineSkip(Currentbuf, Currentbuf->topLine,
        offsety, false);
    arrangeLine(Currentbuf);
}

static char*
w3m_dir(const char* name, char* dft)
{
#ifdef USE_PATH_ENVVAR
    char* value = getenv(name);
    return value ? value : dft;
#else
    return dft;
#endif
}

char* w3m_auxbin_dir(void)
{
    return w3m_dir("W3M_AUXBIN_DIR", AUXBIN_DIR);
}

char* w3m_lib_dir(void)
{
    /* FIXME: use W3M_CGIBIN_DIR? */
    return w3m_dir("W3M_LIB_DIR", CGIBIN_DIR);
}

char* w3m_etc_dir(void)
{
    return w3m_dir("W3M_ETC_DIR", ETC_DIR);
}

char* w3m_conf_dir(void)
{
    return w3m_dir("W3M_CONF_DIR", CONF_DIR);
}

char* w3m_help_dir(void)
{
    return w3m_dir("W3M_HELP_DIR", HELP_DIR);
}

char* file_to_url(const char* file, const char* currentDir)
{
    Str tmp;
#ifdef SUPPORT_NETBIOS_SHARE
    char* host = NULL;
#endif

    if (!(file = expandPath(file)))
        return NULL;
#ifdef SUPPORT_NETBIOS_SHARE
    if (file[0] == '/' && file[1] == '/') {
        char* p;
        file += 2;
        if (*file) {
            p = strchr(file, '/');
            if (p != NULL && p != file) {
                host = allocStr(file, (p - file));
                file = p;
            }
        }
    }
#endif
    if (file[0] != '/') {
        tmp = Strnew_charp(currentDir);
        if (Strlastchar(tmp) != '/')
            Strcat_char(tmp, '/');
        Strcat_charp(tmp, file);
        file = tmp->ptr;
    }
    tmp = Strnew_charp("file://");
#ifdef SUPPORT_NETBIOS_SHARE
    if (host)
        Strcat_charp(tmp, host);
#endif
    Strcat_charp(tmp, file_quote(cleanupName(file)));
    return tmp->ptr;
}

Str myEditor(const char* cmd, const char* file, int line)
{
    Str tmp = NULL;
    char* p;
    int set_file = false, set_line = false;

    for (p = cmd; *p; p++) {
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
