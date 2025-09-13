#include "w3m.h"
#include "LinkList.h"
#include "Anchor.h"
#include "AnchorList.h"
#include "term_renderer.h"
#include "http_message.h"
#include "ui.h"
#include "HttpRequest.h"
#include "alloc.h"
#include "runtime.h"
#include "defun_macro.h"
#include "ContentType.h"
#include "Content.h"
#include "buffer_loader.h"
#include "progress.h"
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
#include "proxy.h"
#include "maparea.h"
#include "ssl_util.h"
#include "mailcap.h"
#include "local_cgi.h"
#include "cookie.h"
#include "ui.h"
#include "search.h"
#include "str_util.h"
#include "history.h"
#include "image.h"
#include "term_renderer.h"
#include "term_size.h"
#include "graphicchar.h"
#include "tty.h"
#include "TermEntry.h"
#include "screen.h"
#include <stdio.h>
#include <signal.h>
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

char* mkd_tmp_dir = (NULL);
char ArgvIsURL = true;

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

int clear_buffer = (true);
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

static void cmd_loadBuffer(struct UI ui, struct Buffer* buf, int prop, int linkid);

static char* getCurWord(struct Buffer* buf, int* spos, int* epos);

static int display_ok = false;
int prev_key = -1;

void set_buffer_environ(struct Buffer*);
static void save_buffer_position(struct Buffer* buf);

static void _nextA(struct UI ui, int);
static void _prevA(struct UI ui, int);
static int check_target = true;
static int searchKeyNum(void);

/*
 * List of error messages
 */
static struct Buffer*
message_list_panel(struct UI ui)
{
    Str tmp = Strnew_size(getScreen()->ROWS * getScreen()->COLS);
    ListItem* p;

    /* FIXME: gettextize? */
    Strcat_charp(tmp,
        "<html><head><title>List of error messages</title></head><body>"
        "<h1>List of error messages</h1><table cellpadding=0>\n");

    concatMessageList(tmp);

    Strcat_charp(tmp, "</table></body></html>");
    return loadHTMLString(ui, tmp, WC_CES_UTF_8);
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

static Str
conv_form_encoding(Str val, struct FormItem* fi, struct Buffer* buf)
{
    wc_ces charset = SystemCharset;

    if (fi->parent->charset)
        charset = fi->parent->charset;
    else if (buf->document.charset && buf->document.charset != WC_CES_US_ASCII)
        charset = buf->document.charset;
    return wc_Str_conv_strict(val, InnerCharset, charset);
}

static void
query_from_followform(struct UI ui, Str* query, struct FormItem* fi, int multipart)
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
                getMapXY(ui.current_buffer, retrieveCurrentImg(ui.current_buffer), &x, &y);
                *query = Strdup(conv_form_encoding(f2->name, fi, ui.current_buffer));
                Strcat_charp(*query, ".x");
                form_write_data(body, fi->parent->boundary, (*query)->ptr,
                    Sprintf("%d", x)->ptr);
                *query = Strdup(conv_form_encoding(f2->name, fi, ui.current_buffer));
                Strcat_charp(*query, ".y");
                form_write_data(body, fi->parent->boundary, (*query)->ptr,
                    Sprintf("%d", y)->ptr);
            } else if (f2->name && f2->name->length > 0 && f2->value != NULL) {
                /* not IMAGE */
                *query = conv_form_encoding(f2->value, fi, ui.current_buffer);
                if (f2->type == FORM_INPUT_FILE)
                    form_write_from_file(body, fi->parent->boundary,
                        conv_form_encoding(f2->name, fi,
                            ui.current_buffer)
                            ->ptr,
                        (*query)->ptr,
                        Str_conv_to_system(f2->value)->ptr);
                else
                    form_write_data(body, fi->parent->boundary,
                        conv_form_encoding(f2->name, fi,
                            ui.current_buffer)
                            ->ptr,
                        (*query)->ptr);
            }
        } else {
            /* not multipart */
            if (f2->type == FORM_INPUT_IMAGE) {
                int x = 0, y = 0;
                getMapXY(ui.current_buffer, retrieveCurrentImg(ui.current_buffer), &x, &y);
                Strcat(*query,
                    Str_form_quote(conv_form_encoding(f2->name, fi, ui.current_buffer)));
                Strcat(*query, Sprintf(".x=%d&", x));
                Strcat(*query,
                    Str_form_quote(conv_form_encoding(f2->name, fi, ui.current_buffer)));
                Strcat(*query, Sprintf(".y=%d", y));
            } else {
                /* not IMAGE */
                if (f2->name && f2->name->length > 0) {
                    Strcat(*query,
                        Str_form_quote(conv_form_encoding(f2->name, fi, ui.current_buffer)));
                    Strcat_char(*query, '=');
                }
                if (f2->value != NULL) {
                    if (fi->parent->method == FORM_METHOD_INTERNAL)
                        Strcat(*query, Str_form_quote(f2->value));
                    else {
                        Strcat(*query,
                            Str_form_quote(conv_form_encoding(f2->value, fi, ui.current_buffer)));
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

static void pushBuffer(struct UI ui, struct Buffer* buf)
{
    deleteImage(ui.current_buffer);
    if (clear_buffer)
        tmpClearBuffer(ui.current_buffer);

    struct Buffer* b;
    if (Firstbuf == ui.current_buffer) {
        buf->nextBuffer = Firstbuf;
        Firstbuf = ui.current_buffer = buf;
    } else if ((b = prevBuffer(Firstbuf, ui.current_buffer)) != NULL) {
        b->nextBuffer = buf;
        buf->nextBuffer = ui.current_buffer;
        ui.current_buffer = buf;
    }
    saveBufferInfo(ui);
}

static struct Buffer* loadNormalBuf(struct UI ui, struct Buffer* buf)
{
    pushBuffer(ui, buf);
    return buf;
}

static struct Buffer*
loadLink(struct UI ui, const char* url, const char* target, const char* referer, struct Form* post, bool do_download)
{

    message(ui, MSG_INFO, Sprintf("loading %s", url)->ptr);
    // refresh(ttyWriter());

    struct Url* base = baseURL(ui.current_buffer);
    // const int* no_referer_ptr;
    // if ((no_referer_ptr && *no_referer_ptr)
    //     || base == NULL
    //     || base->scheme == SCM_LOCAL
    //     || base->scheme == SCM_LOCAL_CGI
    //     || base->scheme == SCM_DATA)
    //     referer = NO_REFERER;
    if (referer == NULL)
        referer = parsedURL2RefererStr(&ui.current_buffer->currentURL)->ptr;

    struct Content c = loadGeneralFile(url, baseURL(ui.current_buffer), post, referer, UI_TTY);
    if (do_download) {
        if (!c.page)
            return NULL;
        const char* file = guessFileName(c.url.file);
        // doFileMove(tmp->ptr, file);
        abort();
        return NULL;
    }

    struct Buffer* buf = makeBuffer(ui, &c);
    if (buf == NULL) {
        char* emsg = Sprintf("Can't load %s", url)->ptr;
        message(ui, MSG_ERR, emsg);
        return NULL;
    }

    struct Url pu = parseUrl(url, base);
    pushHashHist(URLHist, parsedURL2Str(&pu)->ptr);

    if (buf == NULL) {
        return NULL;
    }

    if (do_download) /* download (thus no need to render frames) */
        return loadNormalBuf(ui, buf);

    if (target == NULL || /* no target specified (that means this page is not a frame page) */
        !strcmp(target, "_top") /* this link is specified to be opened as an indivisual * page */
    ) {
        return loadNormalBuf(ui, buf);
    }

    return loadNormalBuf(ui, buf);
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

static void do_submit(struct UI ui, struct Anchor* a, struct FormItem* fi, bool do_download)
{
    Str tmp = Strnew();
    int multipart = (fi->parent->method == FORM_METHOD_POST && fi->parent->enctype == FORM_ENCTYPE_MULTIPART);
    query_from_followform(ui, &tmp, fi, multipart);

    Str tmp2 = Strdup(fi->parent->action);
    if (!Strcmp_charp(tmp2, "!CURRENT_URL!")) {
        /* It means "current URL" */
        tmp2 = parsedURL2Str(&ui.current_buffer->currentURL);
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
        loadLink(ui, tmp2->ptr, (char*)a->target, NULL, NULL, do_download);
    } else if (fi->parent->method == FORM_METHOD_POST) {
        struct Buffer* buf;
        if (multipart) {
            struct stat st;
            stat(fi->parent->body, &st);
            fi->parent->length = st.st_size;
        } else {
            fi->parent->body = tmp->ptr;
            fi->parent->length = tmp->length;
        }
        buf = loadLink(ui, tmp2->ptr, (char*)a->target, NULL, fi->parent, do_download);
        if (multipart) {
            unlink(fi->parent->body);
        }
        // if (buf && !(buf->bufferprop & BP_REDIRECTED)) { /* buf must be ui.current_buffer */
        /* BP_REDIRECTED means that the buffer is obtained through
         * Location: header. In this case, buf->form_submit must not be set
         * because the page is not loaded by POST method but GET method.
         */
        buf->form_submit = save_submit_formlist(fi);
        // }
    } else if ((fi->parent->method == FORM_METHOD_INTERNAL && (!Strcmp_charp(fi->parent->action, "map") || !Strcmp_charp(fi->parent->action, "none"))) || ui.current_buffer->bufferprop & BP_INTERNAL) { /* internal */
        do_internal(ui, tmp2->ptr, tmp->ptr);
    } else {
        message(ui, MSG_ERR, "Can't send form because of illegal method.");
    }
}

static void
_followForm(struct UI ui, bool submit, bool do_download)
{
    if (ui.current_buffer->document.firstLine == NULL)
        return;

    struct Anchor* a = retrieveCurrentForm(ui.current_buffer);
    if (a == NULL)
        return;

    struct FormItem* fi = (struct FormItem*)a->url;
    switch (fi->type) {
    case FORM_INPUT_TEXT: {
        if (submit) {
            do_submit(ui, a, fi, do_download);
            return;
        }
        if (fi->readonly)
            message(ui, MSG_INFO, "Read only field!");
        const char* p = inputStrHist(ui, "TEXT:", fi->value ? fi->value->ptr : NULL, TextHist);
        if (p == NULL || fi->readonly)
            break;
        fi->value = Strnew_charp(p);
        formUpdateBuffer(ui, a, ui.current_buffer, fi);
        if (fi->accept || fi->parent->nitems == 1) {
            do_submit(ui, a, fi, do_download);
            return;
        }
        break;
    }
    case FORM_INPUT_FILE: {
        if (submit) {
            do_submit(ui, a, fi, do_download);
            return;
        }
        if (fi->readonly)
            message(ui, MSG_INFO, "Read only field!");
        const char* p = inputFilenameHist(ui, "Filename:", fi->value ? fi->value->ptr : NULL, NULL);
        if (p == NULL || fi->readonly)
            break;
        fi->value = Strnew_charp(p);
        formUpdateBuffer(ui, a, ui.current_buffer, fi);
        if (fi->accept || fi->parent->nitems == 1) {
            do_submit(ui, a, fi, do_download);
            return;
        }
        break;
    }
    case FORM_INPUT_PASSWORD: {
        if (submit) {
            do_submit(ui, a, fi, do_download);
            return;
        }
        if (fi->readonly) {
            message(ui, MSG_INFO, "Read only field!");
            break;
        }
        const char* p = inputLine(ui, "Password:", fi->value ? fi->value->ptr : NULL,
            IN_PASSWORD);
        if (p == NULL)
            break;
        fi->value = Strnew_charp(p);
        formUpdateBuffer(ui, a, ui.current_buffer, fi);
        if (fi->accept) {
            do_submit(ui, a, fi, do_download);
            return;
        }
        break;
    }
    case FORM_TEXTAREA: {
        if (submit) {
            do_submit(ui, a, fi, do_download);
            return;
        }
        if (fi->readonly) {
            message(ui, MSG_INFO, "Read only field!");
        }
        input_textarea(fi);
        formUpdateBuffer(ui, a, ui.current_buffer, fi);
        break;
    }
    case FORM_INPUT_RADIO: {
        if (submit) {
            do_submit(ui, a, fi, do_download);
            return;
        }
        if (fi->readonly) {
            message(ui, MSG_INFO, "Read only field!");
            break;
        }
        formRecheckRadio(ui, a, ui.current_buffer, fi);
        break;
    }
    case FORM_INPUT_CHECKBOX: {
        if (submit) {
            do_submit(ui, a, fi, do_download);
            return;
        }
        if (fi->readonly) {
            /* FIXME: gettextize? */
            message(ui, MSG_INFO, "Read only field!");
            break;
        }
        fi->checked = !fi->checked;
        formUpdateBuffer(ui, a, ui.current_buffer, fi);
        break;
    }
    case FORM_SELECT: {
        if (submit) {
            do_submit(ui, a, fi, do_download);
            return;
        }
        if (!formChooseOptionByMenu(fi,
                ui.viewport_cursor.x - ui.current_buffer->pos + a->start.pos,
                ui.viewport_cursor.y))
            break;
        formUpdateBuffer(ui, a, ui.current_buffer, fi);
        if (fi->parent->nitems == 1) {
            do_submit(ui, a, fi, do_download);
            return;
        }
        break;
    }
    case FORM_INPUT_IMAGE:
    case FORM_INPUT_SUBMIT:
    case FORM_INPUT_BUTTON: {
        do_submit(ui, a, fi, do_download);
        break;
    }
    case FORM_INPUT_RESET: {
        for (int i = 0; i < ui.current_buffer->document.formitem->nanchor; i++) {
            struct Anchor* a2 = &ui.current_buffer->document.formitem->anchors[i];
            struct FormItem* f2 = (struct FormItem*)a2->url;
            if (f2->parent == fi->parent && f2->name && f2->value && f2->type != FORM_INPUT_SUBMIT && f2->type != FORM_INPUT_HIDDEN && f2->type != FORM_INPUT_RESET) {
                f2->value = f2->init_value;
                f2->checked = f2->init_checked;
                f2->label = f2->init_label;
                f2->selected = f2->init_selected;
                formUpdateBuffer(ui, a2, ui.current_buffer, f2);
            }
        }
        break;
    }
    case FORM_INPUT_HIDDEN:
    default:
        break;
    }
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
        gotoLine(ui.current_buffer, a->start.line);
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

        bufToScreen(ui, ui.current_buffer);
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

                bufToScreen(ui, ui.current_buffer);
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
        bufToScreen(ui, ui.current_buffer);
        renderFrame(ui);
        // continue;
    }
    if (need_resize_screen) {
        resize_screen();
        bufToScreen(ui, ui.current_buffer);
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
    struct UI ui = getUI();
    if (IS_ASCII(c)) { /* Ascii */

        set_buffer_environ(ui.current_buffer);
        save_buffer_position(ui.current_buffer);
        {
            CurrentKey = c;
            unsigned char prev = g_keylog[(g_i - 1) % sizeof(g_keylog)];
            CommandFunc func = (prev == 0x1b) ? EscKeymap[c]
                                              : GlobalKeymap[c];
            func(ui);
        }
        if (applyCursor(ui.current_buffer)) {
            termClear(ttyWriter());
        }
        bufToScreen(ui, ui.current_buffer);
        renderFrame(ui);
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
        w3mFuncList[(int)map[c]].func(getUI());
}

void tmpClearBuffer(struct Buffer* buf)
{
    if (writeBufferCache(buf) == 0) {
        buf->document.firstLine = NULL;
        buf->document.topLineIndex = 0;
        buf->document.currentLineIndex = 0;
    }
}

static Str currentURL(struct UI ui);

void saveBufferInfo(struct UI ui)
{
    FILE* fp;
    if ((fp = fopen(rcFile("bufinfo"), "w")) == NULL) {
        return;
    }
    fprintf(fp, "%s\n", currentURL(ui)->ptr);
    fclose(fp);
}

void delBuffer(struct UI ui, struct Buffer* buf)
{
    if (buf == NULL)
        return;
    if (ui.current_buffer == buf)
        ui.current_buffer = buf->nextBuffer;
    Firstbuf = deleteBuffer(Firstbuf, buf);
    if (!ui.current_buffer)
        ui.current_buffer = Firstbuf;
}

static void
repBuffer(struct UI ui, struct Buffer* oldbuf, struct Buffer* buf)
{
    Firstbuf = replaceBuffer(Firstbuf, oldbuf, buf);
    ui.current_buffer = buf;
}

/* Move page forward */
DEFUN(pgFore, NEXT_PAGE, "Scroll down one page")
{
    ui.current_buffer->document.topLineIndex += getScreen()->ROWS;
}

/* Move page backward */
DEFUN(pgBack, PREV_PAGE, "Scroll up one page")
{
    // nscroll(searchKeyNum() * (getScreen()->ROWS - 1));
}

/* Move half page forward */
DEFUN(hpgFore, NEXT_HALF_PAGE, "Scroll down half a page")
{
    // nscroll(-searchKeyNum() * (getScreen()->ROWS / 2 - 1));
}

/* Move half page backward */
DEFUN(hpgBack, PREV_HALF_PAGE, "Scroll up half a page")
{
    // nscroll(-searchKeyNum() * (getScreen()->ROWS / 2 - 1));
}

/* 1 line up */
DEFUN(lup1, UP, "Scroll the screen up one line")
{
    ui.current_buffer->document.topLineIndex++;
}

/* 1 line down */
DEFUN(ldown1, DOWN, "Scroll the screen down one line")
{
    ui.current_buffer->document.topLineIndex--;
}

/* move cursor position to the center of screen */
DEFUN(ctrCsrV, CENTER_V, "Center on cursor line")
{
    if (ui.current_buffer->document.firstLine == NULL)
        return;
    int offsety = getScreen()->ROWS / 2 - ui.viewport_cursor.y;
    if (offsety != 0) {
        ui.current_buffer->document.topLineIndex = lineSkip(ui.current_buffer, topLine(&ui.current_buffer->document), -offsety, false)->linenumber;
        arrangeLine(ui.current_buffer);
    }
}

DEFUN(ctrCsrH, CENTER_H, "Center on cursor column")
{
    if (ui.current_buffer->document.firstLine == NULL)
        return;
    int offsetx = ui.viewport_cursor.x - getScreen()->COLS / 2;
    if (offsetx != 0) {
        columnSkip(ui.current_buffer, offsetx);
        arrangeCursor(ui.current_buffer);
    }
}

/* Redraw screen */
DEFUN(rdrwSc, REDRAW, "Draw the screen anew")
{
    vt_clear(getScreen());
    arrangeCursor(ui.current_buffer);
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
cmd_loadURL(struct UI ui, const char* url, struct Url* current, const char* referer, struct Form* post)
{
    // refresh(ttyWriter());
    struct Content c = loadGeneralFile(url, current, post, referer, UI_TTY);
    struct Buffer* buf = makeBuffer(ui, &c);
    if (buf == NULL) {
        /* FIXME: gettextize? */
        char* emsg = Sprintf("Can't load %s", conv_from_system(url))->ptr;
        message(getUI(), MSG_ERR, emsg);
    } else if (buf) {
        pushBuffer(ui, buf);
    }
}

static void
shiftvisualpos(struct Buffer* buf, int shift)
{
    struct LineList* l = currentLine(&buf->document);
    buf->visualpos -= shift;
    if (buf->visualpos - l->bwidth >= getScreen()->COLS)
        buf->visualpos = l->bwidth + getScreen()->COLS - 1;
    else if (buf->visualpos - l->bwidth < 0)
        buf->visualpos = l->bwidth;
    arrangeLine(buf);
    if (buf->visualpos - l->bwidth == -shift && getUI().viewport_cursor.x == 0)
        buf->visualpos = l->bwidth;
}

/* Shift screen left */
DEFUN(shiftl, SHIFT_LEFT, "Shift screen left")
{
    int column;

    if (ui.current_buffer->document.firstLine == NULL)
        return;
    column = ui.current_buffer->currentColumn;
    columnSkip(ui.current_buffer, searchKeyNum() * (-getScreen()->COLS + 1) + 1);
    shiftvisualpos(ui.current_buffer, ui.current_buffer->currentColumn - column);
}

/* Shift screen right */
DEFUN(shiftr, SHIFT_RIGHT, "Shift screen right")
{
    int column;

    if (ui.current_buffer->document.firstLine == NULL)
        return;
    column = ui.current_buffer->currentColumn;
    columnSkip(ui.current_buffer, searchKeyNum() * (getScreen()->COLS - 1) - 1);
    shiftvisualpos(ui.current_buffer, ui.current_buffer->currentColumn - column);
}

DEFUN(col1R, RIGHT, "Shift screen one column right")
{
    struct Buffer* buf = ui.current_buffer;
    struct LineList* l = currentLine(&buf->document);
    int j, column, n = searchKeyNum();

    if (l == NULL)
        return;
    for (j = 0; j < n; j++) {
        column = buf->currentColumn;
        columnSkip(ui.current_buffer, 1);
        if (column == buf->currentColumn)
            break;
        shiftvisualpos(ui.current_buffer, 1);
    }
}

DEFUN(col1L, LEFT, "Shift screen one column left")
{
    struct Buffer* buf = ui.current_buffer;
    struct LineList* l = currentLine(&buf->document);
    int j, n = searchKeyNum();

    if (l == NULL)
        return;
    for (j = 0; j < n; j++) {
        if (buf->currentColumn == 0)
            break;
        columnSkip(ui.current_buffer, -1);
        shiftvisualpos(ui.current_buffer, -1);
    }
}

DEFUN(setEnv, SETENV, "Set environment variable")
{
    CurrentKeyData = NULL; /* not allowed in w3m-control: */
    const char* env = searchKeyData();
    if (env == NULL || *env == '\0' || strchr(env, '=') == NULL) {
        if (env != NULL && *env != '\0')
            env = Sprintf("%s=", env)->ptr;
        env = inputStrHist(ui, "Set environ: ", env, TextHist);
        if (env == NULL || *env == '\0') {

            return;
        }
    }

    char* value;
    if ((value = strchr(env, '=')) != NULL && value > env) {
        char* var = allocStr(env, value - env);
        value++;
        set_environ(var, value);
    }
}

/* Execute shell command */
DEFUN(execsh, EXEC_SHELL SHELL, "Execute shell command and display output")
{
    CurrentKeyData = NULL; /* not allowed in w3m-control: */
    const char* cmd = searchKeyData();
    if (cmd == NULL || *cmd == '\0') {
        cmd = inputLineHist(ui, "(exec shell)!", "", IN_COMMAND, ShellHist);
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

static void cmd_loadfile(struct UI ui, const char* fn)
{
    struct Content c = loadGeneralFile(file_to_url(fn, CurrentDir), NULL, NULL, NO_REFERER, UI_TTY);
    struct Buffer* buf = makeBuffer(ui, &c);
    if (buf == NULL) {
        /* FIXME: gettextize? */
        char* emsg = Sprintf("%s not found", conv_from_system(fn))->ptr;
        message(getUI(), MSG_ERR, emsg);
    } else if (buf) {
        pushBuffer(ui, buf);
    }
}

/* Load file */
DEFUN(ldfile, LOAD, "Open local file in a new buffer")
{
    const char* fn = searchKeyData();
    if (fn == NULL || *fn == '\0') {
        /* FIXME: gettextize? */
        fn = inputFilenameHist(ui, "(Load)Filename? ", NULL, LoadHist);
    }
    if (fn != NULL)
        fn = conv_to_system(fn);
    if (fn == NULL || *fn == '\0') {
        return;
    }
    cmd_loadfile(ui, fn);
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
    cmd_loadURL(ui, tmp->ptr, NULL, NO_REFERER, NULL);
}

DEFUN(movL, MOVE_LEFT, "Cursor left")
{
    cursorLeft(1);
}

DEFUN(movL1, MOVE_LEFT1, "Cursor left. With edge touched, slide")
{
    cursorLeft(1);
}

DEFUN(movD, MOVE_DOWN, "Cursor down")
{
    cursorDown(1);
}

DEFUN(movD1, MOVE_DOWN1, "Cursor down. With edge touched, slide")
{
    cursorDown(1);
}

DEFUN(movU, MOVE_UP, "Cursor up")
{
    cursorUp(1);
}

DEFUN(movU1, MOVE_UP1, "Cursor up. With edge touched, slide")
{
    cursorUp(1);
}

DEFUN(movR, MOVE_RIGHT, "Cursor right")
{
    cursorRight(1);
}

DEFUN(movR1, MOVE_RIGHT1, "Cursor right. With edge touched, slide")
{
    cursorRight(1);
}

/* movLW, movRW */
/*
 * From: Takashi Nishimoto <g96p0935@mse.waseda.ac.jp> Date: Mon, 14 Jun
 * 1999 09:29:56 +0900
 */

static int nextChar(int s, struct Line* l)
{
    do {
        (s)++;
    } while ((s) < (l)->len && (l)->propBuf[s] & PC_WCHAR2);
    return s;
}

static int prevChar(int s, struct Line* l)
{
    do {
        (s)--;
    } while ((s) > 0 && (l)->propBuf[s] & PC_WCHAR2);
    return s;
}

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
prev_nonnull_line(struct UI ui, struct LineList* line)
{
    struct LineList* l;
    for (l = line; l != NULL && l->l.len == 0; l = l->prev)
        ;
    if (l == NULL || l->l.len == 0)
        return -1;

    ui.current_buffer->document.currentLineIndex = l->linenumber;
    if (l != line)
        ui.current_buffer->pos = currentLine(&ui.current_buffer->document)->l.len;
    return 0;
}

DEFUN(movLW, PREV_WORD, "Move to the previous word")
{
    char* lb;
    int ppos;
    int i, n = searchKeyNum();

    if (ui.current_buffer->document.firstLine == NULL)
        return;

    struct LineList *pline, *l;
    for (i = 0; i < n; i++) {
        pline = currentLine(&ui.current_buffer->document);
        ppos = ui.current_buffer->pos;

        if (prev_nonnull_line(ui, currentLine(&ui.current_buffer->document)) < 0)
            goto end;

        while (1) {
            l = currentLine(&ui.current_buffer->document);
            lb = l->l.lineBuf;
            while (ui.current_buffer->pos > 0) {
                int tmp = prevChar(ui.current_buffer->pos, &l->l);
                if (is_wordchar(getChar(&lb[tmp])))
                    break;
                ui.current_buffer->pos = tmp;
            }
            if (ui.current_buffer->pos > 0)
                break;
            if (prev_nonnull_line(ui, currentLine(&ui.current_buffer->document)->prev) < 0) {
                ui.current_buffer->document.currentLineIndex = pline->linenumber;
                ui.current_buffer->pos = ppos;
                goto end;
            }
            ui.current_buffer->pos = currentLine(&ui.current_buffer->document)->l.len;
        }

        l = currentLine(&ui.current_buffer->document);
        lb = l->l.lineBuf;
        while (ui.current_buffer->pos > 0) {
            int tmp = prevChar(ui.current_buffer->pos, &l->l);
            if (!is_wordchar(getChar(&lb[tmp])))
                break;
            ui.current_buffer->pos = tmp;
        }
    }
end:
    arrangeCursor(ui.current_buffer);
}

static int
next_nonnull_line(struct UI ui, struct LineList* line)
{
    struct LineList* l;
    for (l = line; l != NULL && l->l.len == 0; l = l->next)
        ;

    if (l == NULL || l->l.len == 0)
        return -1;

    ui.current_buffer->document.currentLineIndex = l->linenumber;
    if (l != line)
        ui.current_buffer->pos = 0;
    return 0;
}

DEFUN(movRW, NEXT_WORD, "Move to the next word")
{
    int i, n = searchKeyNum();

    if (ui.current_buffer->document.firstLine == NULL)
        return;

    char* lb;
    for (i = 0; i < n; i++) {
        struct LineList* pline = currentLine(&ui.current_buffer->document);
        int ppos = ui.current_buffer->pos;

        if (next_nonnull_line(ui, currentLine(&ui.current_buffer->document)) < 0)
            goto end;

        struct LineList* l = currentLine(&ui.current_buffer->document);
        lb = l->l.lineBuf;
        while (ui.current_buffer->pos < l->l.len && is_wordchar(getChar(&lb[ui.current_buffer->pos])))
            ui.current_buffer->pos = nextChar(ui.current_buffer->pos, &l->l);

        while (1) {
            while (ui.current_buffer->pos < l->l.len && !is_wordchar(getChar(&lb[ui.current_buffer->pos])))
                ui.current_buffer->pos = nextChar(ui.current_buffer->pos, &l->l);
            if (ui.current_buffer->pos < l->l.len)
                break;
            if (next_nonnull_line(ui, currentLine(&ui.current_buffer->document)->next) < 0) {
                ui.current_buffer->document.currentLineIndex = pline->linenumber;
                ui.current_buffer->pos = ppos;
                goto end;
            }
            ui.current_buffer->pos = 0;
            l = currentLine(&ui.current_buffer->document);
            lb = l->l.lineBuf;
        }
    }
end:
    arrangeCursor(ui.current_buffer);
}

static void
_quitfm(int confirm)
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
    struct Buffer* buf;
    int ok;
    char cmd;

    ok = false;
    do {
        buf = selectBuffer(Firstbuf, ui.current_buffer, &cmd);
        switch (cmd) {
        case 'B':
            ok = true;
            break;
        case '\n':
        case ' ':
            ui.current_buffer = buf;
            ok = true;
            break;
        case 'D':
            delBuffer(ui, buf);
            if (Firstbuf == NULL) {
                /* No more buffer */
                Firstbuf = nullBuffer();
                ui.current_buffer = Firstbuf;
            }
            break;
        case 'q':
            qquitfm(getUI());
            break;
        case 'Q':
            quitfm(getUI());
            break;
        }
    } while (!ok);

    for (buf = Firstbuf; buf != NULL; buf = buf->nextBuffer) {
        if (buf == ui.current_buffer)
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
void _goLine(struct UI ui, const char* l)
{
    if (l == NULL || *l == '\0' || currentLine(&ui.current_buffer->document) == NULL) {

        return;
    }
    ui.current_buffer->pos = 0;
    if (*l == '^') {
        ui.current_buffer->document.topLineIndex = ui.current_buffer->document.currentLineIndex = ui.current_buffer->document.firstLine->linenumber;
    } else if (*l == '$') {
        ui.current_buffer->document.topLineIndex = lineSkip(ui.current_buffer, lastLine(&ui.current_buffer->document),
            -(getScreen()->ROWS + 1) / 2, true)
                                                       ->linenumber;
        ui.current_buffer->document.currentLineIndex = lastLine(&ui.current_buffer->document)->linenumber;
    }
    // else
    //     gotoRealLine(ui.current_buffer, atoi(l));
    arrangeCursor(ui.current_buffer);
}

DEFUN(goLine, GOTO_LINE, "Go to the specified line")
{

    const char* str = searchKeyData();
    if (str)
        _goLine(ui, str);
    else
        /* FIXME: gettextize? */
        _goLine(ui, inputStr(getUI(), "Goto line: ", ""));
}

DEFUN(goLineL, END, "Go to the last line")
{
    _goLine(ui, "$");
}

/* Go to the bottom of the line */
DEFUN(linend, LINE_END, "Go to the end of the line")
{
    if (ui.current_buffer->document.firstLine == NULL)
        return;
    while (currentLine(&ui.current_buffer->document)->next
        && currentLine(&ui.current_buffer->document)->next->bpos)
        cursorDown(1);
    ui.current_buffer->pos = currentLine(&ui.current_buffer->document)->l.len - 1;
    arrangeCursor(ui.current_buffer);
}

// static int
// cur_real_linenumber(struct Buffer* buf)
// {
//     struct Line *l, *cur = currentLine(&buf->document);
//     int n;
//
//     if (!cur)
//         return 1;
//     n = cur->real_linenumber ? cur->real_linenumber : 1;
//     for (l = buf->firstLine; l && l != cur && l->real_linenumber == 0; l = l->next) { /* header */
//         if (l->bpos == 0)
//             n++;
//     }
//     return n;
// }

/* Run editor on the current buffer */
DEFUN(editBf, EDIT, "Edit local source")
{
    const char* fn = ui.current_buffer->filename;
    if (fn == NULL
        || (ui.current_buffer->content_type == CONTENTTYPE_UNKNOWN && ui.current_buffer->edit == NULL)
        || /* Reading shell */ ui.current_buffer->real_scheme != SCM_LOCAL
        || !strcmp(ui.current_buffer->currentURL.file, "-") /* file is std input  */
    ) {
        message(getUI(), MSG_ERR, "Can't edit other than local file");
        return;
    }

    Str cmd;
    if (ui.current_buffer->edit)
        cmd = unquote_mailcap(ui.current_buffer->edit, contentTypeStr(ui.current_buffer->content_type), fn,
            getHttpHeaderValue(ui.current_buffer->document_header, "Content-Type:"), NULL);
    else
        cmd = myEditor(Editor, shell_quote(fn), 1);
    // cur_real_linenumber(ui.current_buffer));
    exec_cmd(cmd->ptr);

    reload(ui);
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
    saveBuffer(ui.current_buffer, f, true);
    fclose(f);
    exec_cmd(myEditor(Editor, shell_quote(tmpf),
        1
        // cur_real_linenumber(ui.current_buffer)
        )
            ->ptr);
    unlink(tmpf);
}

/* Set / unset mark */
DEFUN(_mark, MARK, "Set/unset mark")
{
    if (!use_mark)
        return;
    if (ui.current_buffer->document.firstLine == NULL)
        return;
    struct LineList* l = currentLine(&ui.current_buffer->document);
    l->l.propBuf[ui.current_buffer->pos] ^= PE_MARK;
}

/* Go to next mark */
DEFUN(nextMk, NEXT_MARK, "Go to the next mark")
{
    if (!use_mark)
        return;
    if (ui.current_buffer->document.firstLine == NULL)
        return;
    int i = ui.current_buffer->pos + 1;
    struct LineList* l = currentLine(&ui.current_buffer->document);
    if (i >= l->l.len) {
        i = 0;
        l = l->next;
    }
    while (l != NULL) {
        for (; i < l->l.len; i++) {
            if (l->l.propBuf[i] & PE_MARK) {
                ui.current_buffer->document.currentLineIndex = l->linenumber;
                ui.current_buffer->pos = i;
                arrangeCursor(ui.current_buffer);

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
    if (!use_mark)
        return;
    if (ui.current_buffer->document.firstLine == NULL)
        return;
    int i = ui.current_buffer->pos - 1;
    struct LineList* l = currentLine(&ui.current_buffer->document);
    if (i < 0) {
        l = l->prev;
        if (l != NULL)
            i = l->l.len - 1;
    }
    while (l != NULL) {
        for (; i >= 0; i--) {
            if (l->l.propBuf[i] & PE_MARK) {
                ui.current_buffer->document.currentLineIndex = l->linenumber;
                ui.current_buffer->pos = i;
                arrangeCursor(ui.current_buffer);

                return;
            }
        }
        l = l->prev;
        if (l != NULL)
            i = l->l.len - 1;
    }
    /* FIXME: gettextize? */
    message(getUI(), MSG_INFO, "No mark exist before here");
}

static const char* MarkString = NULL;

/* Mark place to which the regular expression matches */
DEFUN(reMark, REG_MARK, "Mark all occurences of a pattern")
{
    if (!use_mark)
        return;

    const char* str = searchKeyData();
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

    struct LineList* l;
    char *p, *p1, *p2;
    MarkString = str;
    for (l = ui.current_buffer->document.firstLine; l != NULL; l = l->next) {
        p = l->l.lineBuf;
        for (;;) {
            if (regexMatch(p, &l->l.lineBuf[l->l.len] - p, p == l->l.lineBuf) == 1) {
                matchedPosition(&p1, &p2);
                l->l.propBuf[p1 - l->l.lineBuf] |= PE_MARK;
                p = p2;
            } else
                break;
        }
    }
}

static void
gotoLabel(struct UI ui, const char* label)
{
    struct Anchor* al = searchURLLabel(ui.current_buffer, label);
    if (al == NULL) {
        /* FIXME: gettextize? */
        message(getUI(), MSG_INFO, Sprintf("%s is not found", label)->ptr);
        return;
    }

    struct Buffer* buf = newBuffer();
    copyBuffer(buf, ui.current_buffer);
    int i;
    for (i = 0; i < MAX_LB; i++)
        buf->linkBuffer[i] = NULL;
    buf->currentURL.label = allocStr(label, -1);
    pushHashHist(URLHist, parsedURL2Str(&buf->currentURL)->ptr);
    (*buf->clone)++;
    pushBuffer(ui, buf);
    gotoLine(ui.current_buffer, al->start.line);
    if (label_topline)
        ui.current_buffer->document.topLineIndex = lineSkip(ui.current_buffer, topLine(&ui.current_buffer->document),
            currentLine(&ui.current_buffer->document)->linenumber
                - topLine(&ui.current_buffer->document)->linenumber,
            false)
                                                       ->linenumber;
    ui.current_buffer->pos = al->start.pos;
    arrangeCursor(ui.current_buffer);

    return;
}

static void followAnchor(struct UI ui, bool do_download)
{
    struct Anchor* a;
    struct Url u;
    int x = 0, y = 0, map = 0;
    char* url;

    if (ui.current_buffer->document.firstLine == NULL)
        return;

    a = retrieveCurrentImg(ui.current_buffer);
    if (a && a->image && a->image->map) {
        _followForm(getUI(), false, do_download);
        return;
    }
    if (a && a->image && a->image->ismap) {
        getMapXY(ui.current_buffer, a, &x, &y);
        map = 1;
    }
    a = retrieveCurrentAnchor(ui.current_buffer);
    if (a == NULL) {
        _followForm(getUI(), false, do_download);
        return;
    }
    if (*a->url == '#') { /* index within this buffer */
        gotoLabel(ui, (char*)a->url + 1);
        return;
    }
    u = parseUrl(a->url, baseURL(ui.current_buffer));
    if (Strcmp(parsedURL2Str(&u), parsedURL2Str(&ui.current_buffer->currentURL)) == 0) {
        /* index within this buffer */
        if (u.label) {
            gotoLabel(ui, u.label);
            return;
        }
    }
    url = (char*)a->url;
    if (map)
        url = Sprintf("%s?%d,%d", a->url, x, y)->ptr;

    loadLink(ui, url, (char*)a->target, a->referer, NULL, do_download);
}

/* follow HREF link */
DEFUN(followA, GOTO_LINK, "Follow current hyperlink in a new buffer")
{
    followAnchor(ui, false);
}

/* follow HREF link in the buffer */
static void bufferA(struct UI ui)
{
    followAnchor(ui, false);
}

static void followImage(struct UI ui, bool do_download)
{
    if (ui.current_buffer->document.firstLine == NULL)
        return;

    struct Anchor* a;
    a = retrieveCurrentImg(ui.current_buffer);
    if (a == NULL)
        return;
    /* FIXME: gettextize? */
    message(getUI(), MSG_INFO, Sprintf("loading %s", a->url)->ptr);
    // refresh(ttyWriter());
    struct Content c = loadGeneralFile(a->url, baseURL(ui.current_buffer), NULL, NULL, UI_TTY);
    if (do_download) {
        // TODO
        abort();
    }
    struct Buffer* buf = makeBuffer(ui, &c);
    if (buf == NULL) {
        /* FIXME: gettextize? */
        char* emsg = Sprintf("Can't load %s", a->url)->ptr;
        message(ui, MSG_ERR, emsg);
    } else if (buf) {
        pushBuffer(ui, buf);
    }
}

/* view inline image */
DEFUN(followI, VIEW_IMAGE, "Display image in viewer")
{
    followImage(ui, false);
}

/* submit form */
DEFUN(submitForm, SUBMIT, "Submit form")
{
    _followForm(ui, true, false);
}

/* process form */
void followForm(struct UI ui)
{
    _followForm(ui, false, false);
}

/* go to the top anchor */
DEFUN(topA, LINK_BEGIN, "Move to the first hyperlink")
{
    struct HmarkerList* hl = ui.current_buffer->document.hmarklist;
    struct BufferPoint* po;
    struct Anchor* an;
    int hseq = 0;

    if (ui.current_buffer->document.firstLine == NULL)
        return;
    if (!hl || hl->nmark == 0)
        return;

    do {
        if (hseq >= hl->nmark)
            return;
        po = hl->marks + hseq;
        an = retrieveAnchor(ui.current_buffer->document.href, *po);
        if (an == NULL)
            an = retrieveAnchor(ui.current_buffer->document.formitem, *po);
        hseq++;
    } while (an == NULL);

    gotoLine(ui.current_buffer, po->line);
    ui.current_buffer->pos = po->pos;
    arrangeCursor(ui.current_buffer);
}

/* go to the last anchor */
DEFUN(lastA, LINK_END, "Move to the last hyperlink")
{
    struct HmarkerList* hl = ui.current_buffer->document.hmarklist;
    struct BufferPoint* po;
    struct Anchor* an;
    int hseq;

    if (ui.current_buffer->document.firstLine == NULL)
        return;
    if (!hl || hl->nmark == 0)
        return;

    hseq = hl->nmark - 1;
    do {
        if (hseq < 0)
            return;
        po = hl->marks + hseq;
        an = retrieveAnchor(ui.current_buffer->document.href, *po);
        if (an == NULL)
            an = retrieveAnchor(ui.current_buffer->document.formitem, *po);
        hseq--;
    } while (an == NULL);

    gotoLine(ui.current_buffer, po->line);
    ui.current_buffer->pos = po->pos;
    arrangeCursor(ui.current_buffer);
}

/* go to the nth anchor */
DEFUN(nthA, LINK_N, "Go to the nth link")
{
    struct HmarkerList* hl = ui.current_buffer->document.hmarklist;

    int n = searchKeyNum();
    if (n < 0 || n > hl->nmark)
        return;

    if (ui.current_buffer->document.firstLine == NULL)
        return;
    if (!hl || hl->nmark == 0)
        return;

    struct BufferPoint* po = hl->marks + n - 1;
    struct Anchor* an = retrieveAnchor(ui.current_buffer->document.href, *po);
    if (an == NULL)
        an = retrieveAnchor(ui.current_buffer->document.formitem, *po);
    if (an == NULL)
        return;

    gotoLine(ui.current_buffer, po->line);
    ui.current_buffer->pos = po->pos;
    arrangeCursor(ui.current_buffer);
}

/* go to the next anchor */
DEFUN(nextA, NEXT_LINK, "Move to the next hyperlink")
{
    _nextA(ui, false);
}

/* go to the previous anchor */
DEFUN(prevA, PREV_LINK, "Move to the previous hyperlink")
{
    _prevA(ui, false);
}

/* go to the next visited anchor */
DEFUN(nextVA, NEXT_VISITED, "Move to the next visited hyperlink")
{
    _nextA(ui, true);
}

/* go to the previous visited anchor */
DEFUN(prevVA, PREV_VISITED, "Move to the previous visited hyperlink")
{
    _prevA(ui, true);
}

/* go to the next [visited] anchor */
static void
_nextA(struct UI ui, int visited)
{
    struct HmarkerList* hl = ui.current_buffer->document.hmarklist;
    struct BufferPoint* po;
    struct Anchor *an, *pan;
    int i, x, y, n = searchKeyNum();
    struct Url url;

    if (ui.current_buffer->document.firstLine == NULL)
        return;
    if (!hl || hl->nmark == 0)
        return;

    an = retrieveCurrentAnchor(ui.current_buffer);
    if (visited != true && an == NULL)
        an = retrieveCurrentForm(ui.current_buffer);

    y = currentLine(&ui.current_buffer->document)->linenumber;
    x = ui.current_buffer->pos;

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
                an = retrieveAnchor(ui.current_buffer->document.href, *po);
                if (visited != true && an == NULL)
                    an = retrieveAnchor(ui.current_buffer->document.formitem, *po);
                hseq++;
                if (visited == true && an) {
                    url = parseUrl(an->url, baseURL(ui.current_buffer));
                    if (getHashHist(URLHist, parsedURL2Str(&url)->ptr)) {
                        goto _end;
                    }
                }
            } while (an == NULL || an == pan);
        } else {
            an = closest_next_anchor(ui.current_buffer->document.href, NULL, x, y);
            if (visited != true)
                an = closest_next_anchor(ui.current_buffer->document.formitem, an, x, y);
            if (an == NULL) {
                if (visited == true)
                    return;
                an = pan;
                break;
            }
            x = an->start.pos;
            y = an->start.line;
            if (visited == true) {
                url = parseUrl(an->url, baseURL(ui.current_buffer));
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
    gotoLine(ui.current_buffer, po->line);
    ui.current_buffer->pos = po->pos;
    arrangeCursor(ui.current_buffer);
}

/* go to the previous anchor */
static void
_prevA(struct UI ui, int visited)
{
    struct HmarkerList* hl = ui.current_buffer->document.hmarklist;
    struct BufferPoint* po;
    struct Anchor *an, *pan;
    int i, x, y, n = searchKeyNum();
    struct Url url;

    if (ui.current_buffer->document.firstLine == NULL)
        return;
    if (!hl || hl->nmark == 0)
        return;

    an = retrieveCurrentAnchor(ui.current_buffer);
    if (visited != true && an == NULL)
        an = retrieveCurrentForm(ui.current_buffer);

    y = currentLine(&ui.current_buffer->document)->linenumber;
    x = ui.current_buffer->pos;

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
                an = retrieveAnchor(ui.current_buffer->document.href, *po);
                if (visited != true && an == NULL)
                    an = retrieveAnchor(ui.current_buffer->document.formitem, *po);
                hseq--;
                if (visited == true && an) {
                    url = parseUrl(an->url, baseURL(ui.current_buffer));
                    if (getHashHist(URLHist, parsedURL2Str(&url)->ptr)) {
                        goto _end;
                    }
                }
            } while (an == NULL || an == pan);
        } else {
            an = closest_prev_anchor(ui.current_buffer->document.href, NULL, x, y);
            if (visited != true)
                an = closest_prev_anchor(ui.current_buffer->document.formitem, an, x, y);
            if (an == NULL) {
                if (visited == true)
                    return;
                an = pan;
                break;
            }
            x = an->start.pos;
            y = an->start.line;
            if (visited == true && an) {
                url = parseUrl(an->url, baseURL(ui.current_buffer));
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
    gotoLine(ui.current_buffer, po->line);
    ui.current_buffer->pos = po->pos;
    arrangeCursor(ui.current_buffer);
}

/* go to the next left/right anchor */
static void
nextX(struct UI ui, int d, int dy)
{
    struct HmarkerList* hl = ui.current_buffer->document.hmarklist;
    struct Anchor *an, *pan;
    int i, x, y, n = searchKeyNum();

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
    gotoLine(ui.current_buffer, y);
    ui.current_buffer->pos = pan->start.pos;
    arrangeCursor(ui.current_buffer);
}

/* go to the next downward/upward anchor */
static void
nextY(struct UI ui, int d)
{
    struct HmarkerList* hl = ui.current_buffer->document.hmarklist;
    struct Anchor *an, *pan;
    int i, x, y, n = searchKeyNum();
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
    gotoLine(ui.current_buffer, pan->start.line);
    arrangeLine(ui.current_buffer);
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
    ui.current_buffer = prevBuffer(Firstbuf, ui.current_buffer);
}

/* go to the previous bufferr */
DEFUN(prevBf, PREV, "Switch to the previous buffer")
{
    struct Buffer* buf = ui.current_buffer->nextBuffer;
    if (!buf) {
        return;
    }
    ui.current_buffer = buf;
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
            referer = parsedURL2RefererStr(&ui.current_buffer->currentURL)->ptr;
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
    cmd_loadURL(ui, url, current, referer, NULL);
    if (ui.current_buffer != cur_buf) /* success */
        pushHashHist(URLHist, parsedURL2Str(&ui.current_buffer->currentURL)->ptr);
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
        cmd_loadURL(ui, url, NULL, NULL, NULL);
        if (ui.current_buffer != cur_buf) /* success */
            pushHashHist(URLHist, parsedURL2Str(&ui.current_buffer->currentURL)->ptr);
    }
}

DEFUN(gorURL, GOTO_RELATIVE, "Go to relative address")
{
    goURL0(ui, "Goto relative URL: ", true);
}

static void
cmd_loadBuffer(struct UI ui, struct Buffer* buf, int prop, int linkid)
{
    if (buf == NULL) {
        message(getUI(), MSG_ERR, "Can't load string");
    } else if (buf) {
        buf->bufferprop |= (BP_INTERNAL | prop);
        if (!(buf->bufferprop & BP_NO_URL))
            buf->currentURL = copyParsedUrl(&ui.current_buffer->currentURL);
        if (linkid != LB_NOLINK) {
            buf->linkBuffer[REV_LB[linkid]] = ui.current_buffer;
            ui.current_buffer->linkBuffer[linkid] = buf;
        }
        pushBuffer(ui, buf);
    }
}

/* load bookmark */
DEFUN(ldBmark, BOOKMARK VIEW_BOOKMARK, "View bookmarks")
{
    cmd_loadURL(ui, BookmarkFile, NULL, NO_REFERER, NULL);
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
        (Str_form_quote(parsedURL2Str(&ui.current_buffer->currentURL)))->ptr,
        (Str_form_quote(wc_conv_strict(ui.current_buffer->buffername,
             InnerCharset,
             BookmarkCharset)))
            ->ptr,
        wc_ces_to_charset(BookmarkCharset));
    request = newFormList(NULL, "post", NULL, NULL, NULL, NULL, NULL);
    request->body = tmp->ptr;
    request->length = tmp->length;
    cmd_loadURL(ui, "file:///$LIB/" W3MBOOKMARK_CMDNAME, NULL, NO_REFERER, request);
}

/* option setting */
DEFUN(ldOpt, OPTIONS, "Display options setting panel")
{
    cmd_loadBuffer(ui, load_option_panel(ui), BP_NO_URL, LB_NOLINK);
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
    cmd_loadBuffer(ui, message_list_panel(ui), BP_NO_URL, LB_NOLINK);
}

/* page info */
DEFUN(pginfo, INFO, "Display information about the current document")
{
    struct Buffer* buf;

    if ((buf = ui.current_buffer->linkBuffer[LB_N_INFO]) != NULL) {
        ui.current_buffer = buf;

        return;
    }
    if ((buf = ui.current_buffer->linkBuffer[LB_INFO]) != NULL)
        delBuffer(ui, buf);
    buf = page_info_panel(ui, ui.current_buffer);
    cmd_loadBuffer(ui, buf, BP_NORMAL, LB_INFO);
}

void follow_map(struct UI ui, struct KeyValue* arg)
{
    const char* name = tag_get_value(arg, "link");
    struct Anchor* an;
    MapArea* a;
    int x, y;
    struct Url p_url;

    an = retrieveCurrentImg(ui.current_buffer);
    // x = ui.current_buffer->cursorX;
    // y = ui.current_buffer->cursorY;
    a = follow_map_menu(ui.current_buffer, (char*)name, an, x, y);
    if (a == NULL || a->url == NULL || *(a->url) == '\0') {
        return;
    }
    if (*(a->url) == '#') {
        gotoLabel(ui, a->url + 1);
        return;
    }
    p_url = parseUrl(a->url, baseURL(ui.current_buffer));
    pushHashHist(URLHist, parsedURL2Str(&p_url)->ptr);
    cmd_loadURL(ui, a->url, baseURL(ui.current_buffer),
        parsedURL2Str(&ui.current_buffer->currentURL)->ptr, NULL);
}

/* link menu */
DEFUN(linkMn, LINK_MENU, "Pop up link element menu")
{
    struct LinkList* l = link_menu(ui.current_buffer);
    struct Url p_url;

    if (!l || !l->url)
        return;
    if (*(l->url) == '#') {
        gotoLabel(ui, l->url + 1);
        return;
    }
    p_url = parseUrl(l->url, baseURL(ui.current_buffer));
    pushHashHist(URLHist, parsedURL2Str(&p_url)->ptr);
    cmd_loadURL(ui, l->url, baseURL(ui.current_buffer),
        parsedURL2Str(&ui.current_buffer->currentURL)->ptr, NULL);
}

static void
anchorMn(struct UI ui, struct Anchor* (*menu_func)(struct Buffer*), int go)
{
    if (!ui.current_buffer->document.href || !ui.current_buffer->document.hmarklist)
        return;

    struct Anchor* a = menu_func(ui.current_buffer);
    if (!a || a->hseq < 0)
        return;

    struct BufferPoint* po = &ui.current_buffer->document.hmarklist->marks[a->hseq];
    gotoLine(ui.current_buffer, po->line);
    ui.current_buffer->pos = po->pos;
    arrangeCursor(ui.current_buffer);

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
    struct Buffer* buf = link_list_panel(ui, ui.current_buffer);
    if (buf) {
        cmd_loadBuffer(ui, buf, BP_NORMAL, LB_NOLINK);
    }
}

/* cookie list */
DEFUN(cooLst, COOKIE, "View cookie list")
{
    struct Buffer* buf;

    buf = cookie_list_panel(ui);
    if (buf != NULL)
        cmd_loadBuffer(ui, buf, BP_NO_URL, LB_NOLINK);
}

/* History page */
DEFUN(ldHist, HISTORY, "Show browsing history")
{
    cmd_loadBuffer(ui, historyBuffer(ui, URLHist), BP_NO_URL, LB_NOLINK);
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

/* save source */
DEFUN(svSrc, DOWNLOAD SAVE, "Save document source")
{
    if (ui.current_buffer->sourcefile == NULL)
        return;
    CurrentKeyData = NULL; /* not allowed in w3m-control: */
    PermitSaveToPipe = true;
    const char* file;
    // if (ui.current_buffer->real_scheme == SCM_LOCAL)
    //     file = conv_from_system(guessSaveName(NULL, ui.current_buffer->currentURL.real_file));
    // else
    file = guessSaveName(ui.current_buffer->document_header, ui.current_buffer->currentURL.file);
    doFileCopy(ui.current_buffer->sourcefile, file);
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
    if (!ui.current_buffer || ui.current_buffer->bufferprop & BP_INTERNAL)
        return Strnew_size(0);
    return parsedURL2Str(&ui.current_buffer->currentURL);
}

DEFUN(curURL, PEEK, "Show current address")
{
    static Str s = NULL;
    static Lineprop* p = NULL;
    Lineprop* pp;
    static int offset = 0, n;

    if (ui.current_buffer->bufferprop & BP_INTERNAL)
        return;
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
    if (ui.current_buffer->content_type == CONTENTTYPE_UNKNOWN)
        return;

    struct Buffer* buf;
    if ((buf = ui.current_buffer->linkBuffer[LB_SOURCE]) != NULL || (buf = ui.current_buffer->linkBuffer[LB_N_SOURCE]) != NULL) {
        ui.current_buffer = buf;

        return;
    }
    if (ui.current_buffer->sourcefile == NULL) {
        return;
    }

    buf = newBuffer();

    if (ui.current_buffer->content_type == CONTENTTYPE_TEXT_HTML) {
        buf->content_type = CONTENTTYPE_TEXT_PLAIN;
        buf->buffername = Sprintf("source of %s", ui.current_buffer->buffername)->ptr;
        buf->linkBuffer[LB_N_SOURCE] = ui.current_buffer;
        ui.current_buffer->linkBuffer[LB_SOURCE] = buf;
    } else if (ui.current_buffer->content_type == CONTENTTYPE_TEXT_PLAIN) {
        buf->content_type = CONTENTTYPE_TEXT_HTML;
        buf->buffername = Sprintf("HTML view of %s", ui.current_buffer->buffername)->ptr;
        buf->linkBuffer[LB_SOURCE] = ui.current_buffer;
        ui.current_buffer->linkBuffer[LB_N_SOURCE] = buf;
    } else {
        return;
    }
    buf->currentURL = ui.current_buffer->currentURL;
    buf->real_scheme = ui.current_buffer->real_scheme;
    buf->filename = ui.current_buffer->filename;
    buf->sourcefile = ui.current_buffer->sourcefile;
    buf->document.charset = ui.current_buffer->document.charset;
    buf->clone = ui.current_buffer->clone;
    (*buf->clone)++;

    pushBuffer(ui, buf);
}

/* reload */
DEFUN(reload, RELOAD, "Load current document anew")
{
    struct Buffer *buf, *fbuf = NULL, sbuf;
    wc_ces old_charset;
    Str url;
    int multipart;

    if (ui.current_buffer->bufferprop & BP_INTERNAL) {
        if (!strcmp(ui.current_buffer->buffername, DOWNLOAD_LIST_TITLE)) {
            ldDL(ui);
            return;
        }
        /* FIXME: gettextize? */
        message(getUI(), MSG_ERR, "Can't reload...");
        return;
    }
    if (ui.current_buffer->currentURL.scheme == SCM_LOCAL && !strcmp(ui.current_buffer->currentURL.file, "-")) {
        /* file is std input */
        /* FIXME: gettextize? */
        message(getUI(), MSG_ERR, "Can't reload stdin");
        return;
    }
    copyBuffer(&sbuf, ui.current_buffer);
    multipart = 0;

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
    url = parsedURL2Str(&ui.current_buffer->currentURL);
    /* FIXME: gettextize? */
    message(getUI(), MSG_INFO, "Reloading...");
    // refresh(ttyWriter());
    old_charset = DocumentCharset;
    if (ui.current_buffer->document.charset != WC_CES_US_ASCII)
        DocumentCharset = ui.current_buffer->document.charset;
    // SearchHeader = ui.current_buffer->search_header;
    DefaultType = contentTypeStr(ui.current_buffer->content_type);
    struct Content c = loadGeneralFile(url->ptr, NULL, post, NO_REFERER, UI_TTY /*, true*/);
    buf = makeBuffer(ui, &c);
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
    if (fbuf != NULL)
        Firstbuf = deleteBuffer(Firstbuf, fbuf);
    repBuffer(ui, ui.current_buffer, buf);
    if ((buf->content_type == CONTENTTYPE_TEXT_PLAIN && sbuf.content_type == CONTENTTYPE_TEXT_HTML)
        || (buf->content_type == CONTENTTYPE_TEXT_HTML && sbuf.content_type == CONTENTTYPE_TEXT_PLAIN)) {
        vwSrc(ui);
        if (ui.current_buffer != buf)
            Firstbuf = deleteBuffer(Firstbuf, buf);
    }
    ui.current_buffer->form_submit = sbuf.form_submit;
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
    if (ui.current_buffer->bufferprop & BP_INTERNAL)
        return;
    if (ui.current_buffer->sourcefile == NULL) {
        message(getUI(), MSG_INFO, "Can't reload...");
        return;
    }
    ui.current_buffer->document.charset = charset;
}

void change_charset(struct UI ui, struct KeyValue* arg)
{
    struct Buffer* buf = ui.current_buffer->linkBuffer[LB_N_INFO];
    wc_ces charset;

    if (buf == NULL)
        return;
    delBuffer(ui, ui.current_buffer);
    ui.current_buffer = buf;
    if (ui.current_buffer->bufferprop & BP_INTERNAL)
        return;
    charset = ui.current_buffer->document.charset;
    for (; arg; arg = arg->next) {
        if (!strcmp(arg->arg, "charset"))
            charset = atoi(arg->value);
    }
    _docCSet(ui, charset);
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
    while (e > 0 && !is_wordchar(getChar(&p[e])))
        e = prevChar(e, &l->l);
    if (!is_wordchar(getChar(&p[e])))
        return NULL;
    b = e;
    while (b > 0) {
        int tmp = b;
        tmp = prevChar(tmp, &l->l);
        if (!is_wordchar(getChar(&p[tmp])))
            break;
        b = tmp;
    }
    while (e < l->l.len && is_wordchar(getChar(&p[e])))
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

    char *w, *dictcmd;
    w = conv_to_system(word);
    if (*w == '\0') {

        return;
    }
    dictcmd = Sprintf("%s?%s", DictCommand,
        Str_form_quote(Strnew_charp(w))->ptr)
                  ->ptr;
    struct Content c = loadGeneralFile(dictcmd, NULL, NULL, NO_REFERER, UI_TTY);
    struct Buffer* buf = makeBuffer(ui, &c);
    if (buf == NULL) {
        message(getUI(), MSG_INFO, "Execution failed");
        return;
    } else if (buf) {
        buf->filename = w;
        buf->buffername = Sprintf("%s %s", DICTBUFFERNAME, word)->ptr;
        if (buf->content_type == CONTENTTYPE_UNKNOWN)
            buf->content_type = CONTENTTYPE_TEXT_PLAIN;
        pushBuffer(ui, buf);
    }
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
        set_environ("W3M_SOURCEFILE", buf->sourcefile);
        set_environ("W3M_FILENAME", buf->filename);
        set_environ("W3M_TITLE", buf->buffername);
        set_environ("W3M_URL", parsedURL2Str(&buf->currentURL)->ptr);
        set_environ("W3M_TYPE", contentTypeStr(buf->content_type));
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

static struct Buffer*
DownloadListBuffer(struct UI ui)
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
    return loadHTMLString(ui, src, WC_CES_UTF_8);
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
    struct Buffer* buf;
    int replace = false, new_tab = false;
    int reload;

    if (ui.current_buffer->bufferprop & BP_INTERNAL && !strcmp(ui.current_buffer->buffername, DOWNLOAD_LIST_TITLE))
        replace = true;
    if (!FirstDL) {
        if (replace) {
            if (ui.current_buffer == Firstbuf && ui.current_buffer->nextBuffer == NULL) {
            } else
                delBuffer(ui, ui.current_buffer);
        }
        return;
    }
    reload = checkDownloadList();
    buf = DownloadListBuffer(ui);
    if (!buf) {

        return;
    }
    buf->bufferprop |= (BP_INTERNAL | BP_NO_URL);
    if (replace) {
        // COPY_BUFROOT(buf, ui.current_buffer);
        // restorePosition(buf, ui.current_buffer);
    }
    pushBuffer(ui, buf);
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
    ui.current_buffer->document.currentLineIndex = lineSkip(ui.current_buffer, topLine(&ui.current_buffer->document), 0, false)->linenumber;
    arrangeLine(ui.current_buffer);
}

DEFUN(cursorMiddle, CURSOR_MIDDLE, "Move cursor to the middle of the screen")
{
    if (ui.current_buffer->document.firstLine == NULL)
        return;
    int offsety = (getScreen()->ROWS - 1) / 2;
    ui.current_buffer->document.currentLineIndex = currentLineSkip(ui.current_buffer, topLine(&ui.current_buffer->document), offsety, false)->linenumber;
    arrangeLine(ui.current_buffer);
}

DEFUN(cursorBottom, CURSOR_BOTTOM, "Move cursor to the bottom of the screen")
{
    if (ui.current_buffer->document.firstLine == NULL)
        return;
    int offsety = getScreen()->ROWS - 1;
    ui.current_buffer->document.currentLineIndex = currentLineSkip(ui.current_buffer, topLine(&ui.current_buffer->document), offsety, false)->linenumber;
    arrangeLine(ui.current_buffer);
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
