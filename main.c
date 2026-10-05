#define MAINPROGRAM
#include "defun.h"
#include "textlist.h"
#include "w3m.h"
#include "w3m_tty.h"
#include "w3m_screen.h"
#include "alloc.h"
#include "backend.h"
#include "gettext_helper.h"
#include "tab.h"
#include "alarm.h"
#include "news.h"
#include "ftp.h"
#include "StrWriter.h"
#include "indep.h"
#include "input_stream.h"
#include "entity.h"
#include "str_gc.h"
#include "str_const.h"
#include "w3m.h"
#include "buffer.h"
#include "charset.h"
#include "config.h"
#include "cookie.h"
#include "display.h"
#include "download.h"
#include "func.h"
#include "proto.h"
#include "frame.h"
#include "form.h"
#include "funcname1.h"
#include "linein.h"
#include "keybind.h"
#include "menu.h"
#include "myctype.h"
#include "pathdefs.h"
#include "rc.h"
#include "regex.h"
#include "search.h"
#include "tab.h"
#include "util.h"
#include "http_request.h"
#include "version.h"
#include "libwc/status.h"
#include "libwc/charset.h"

#include <libintl.h>
#include <locale.h>
#include <errno.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <time.h>

#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 255
#endif

#define DICTBUFFERNAME "*dictionary*"

#define COPY_BUFROOT(dstbuf, srcbuf)       \
    {                                      \
        (dstbuf)->rootX = (srcbuf)->rootX; \
        (dstbuf)->rootY = (srcbuf)->rootY; \
        (dstbuf)->cols = (srcbuf)->cols;   \
        (dstbuf)->lines = (srcbuf)->lines; \
    }

#define COPY_BUFPOSITION(dstbuf, srcbuf)                   \
    {                                                      \
        (dstbuf)->topLine = (srcbuf)->topLine;             \
        (dstbuf)->currentLine = (srcbuf)->currentLine;     \
        (dstbuf)->pos = (srcbuf)->pos;                     \
        (dstbuf)->cursorX = (srcbuf)->cursorX;             \
        (dstbuf)->cursorY = (srcbuf)->cursorY;             \
        (dstbuf)->visualpos = (srcbuf)->visualpos;         \
        (dstbuf)->currentColumn = (srcbuf)->currentColumn; \
    }
#define SAVE_BUFPOSITION(sbufp) COPY_BUFPOSITION(sbufp, Currentbuf)
#define RESTORE_BUFPOSITION(sbufp) COPY_BUFPOSITION(Currentbuf, sbufp)

JMP_BUF IntReturn;

static void sig_chld(int signo);

static void SigPipe(SIGNAL_ARG);

static int need_resize_screen = false;
static void resize_hook(SIGNAL_ARG);
static void resize_screen(void);

static void SigAlarm(SIGNAL_ARG);

static const char* MarkString = NULL;

typedef struct _Event {
    int cmd;
    void* data;
    struct _Event* next;
} Event;

#define GC_WARN_KEEP_MAX (20)
#define PREC_NUM (prec_num ? prec_num : 1)
#define PREC_LIMIT 10000

int do_download = false;
DownloadList* FirstDL = NULL;
DownloadList* LastDL = NULL;

int CurrentKey;
const char* CurrentCmdData;

int nTab;
int TabCols = 10;
TabBuffer* CurrentTab;
TabBuffer* FirstTab;
TabBuffer* LastTab;

Hist* DictHist;
Hist* LoadHist;
Hist* SaveHist;
Hist* ShellHist;
Hist* TextHist;
Hist* URLHist;
int (*searchRoutine)(Buffer*, const char*);
int fold_pre;

static Event* CurrentEvent = NULL;
static Event* LastEvent = NULL;
static pStr err_msg;
static GC_warn_proc orig_GC_warn_proc = NULL;
static char* session_bak;
static char* session_file;
static int add_download_list = false;
static int check_target = true;
static int deprecated;
static int display_ok = false;
static int on_target = 1;
static int prec_num = 0;
static int prev_key = -1;

static pStr currentURL(void);
static pStr make_optional_header_string(char* s);
static TabBuffer* newTab(void);
static char* _nxtarg(char* argv);
static char* getarg(char** argv, int* i);
static int _strSession(char* sf);
static int checkDownloadList(void);
static int searchKeyNum(void);
static void* die_oom(size_t bytes);
static void _followForm(int);
static void _goLine(const char*);
static void _newT(void);
static void _nextA(int);
static void _prevA(int);
static void cmd_loadURL(const char* url, struct Url* current, const char* referer, FormList* request);
static void cmd_loadfile(const char* path);
static void delBuffer(Buffer* buf);
static void do_dump(Buffer*);
static void escdmap(char c);
static void followTab(TabBuffer* tab);
static void fversion(FILE* f);
static void help(void);
static void init_from_env(void);
static void intTrap(SIGNAL_ARG);
static void keyPressEventProc(int c);
static void moveTab(TabBuffer* t, TabBuffer* t2, int right);
static void proc_buf(Buffer* newbuf, char search_header, int open_new_tab);
static void save_buffer_position(Buffer* buf);
static void set_buffer_environ(Buffer*);
static void usage(void);
static void wrap_GC_warn_proc(char* msg, GC_word arg);

#define NXTARG() _nxtarg(argv[++i])
#define ISOPT(opt) !strcmp(opt, argv[i])
#define CHKOPT(opt) !strncmp(opt, argv[i], strlen(opt))

/* Helpers for command-line argument parsing */
/* Assert an argument is passed and exit with error if not */
char* _nxtarg(char* argv)
{
    if (!argv)
        usage();
    return argv;
}

/* Get an concatenated argument or assert a seperated argument is passed */
char* getarg(char** argv, int* i)
{
    if (argv[*i][2])
        return &argv[*i][2];
    return _nxtarg(argv[++(*i)]);
}

void init_from_env(void)
{
    /* XXX: This variables should only be declared and set when they are
     * actually used. */
    char* p;

    if (!non_null(HTTP_proxy) && ((p = getenv("HTTP_PROXY")) || (p = getenv("http_proxy")) || (p = getenv("HTTP_proxy"))))
        HTTP_proxy = p;
    if (!non_null(HTTPS_proxy) && ((p = getenv("HTTPS_PROXY")) || (p = getenv("https_proxy")) || (p = getenv("HTTPS_proxy"))))
        HTTPS_proxy = p;
    if (HTTPS_proxy == NULL && non_null(HTTP_proxy))
        HTTPS_proxy = HTTP_proxy;
    if (!non_null(GOPHER_proxy) && ((p = getenv("GOPHER_PROXY")) || (p = getenv("gopher_proxy")) || (p = getenv("GOPHER_proxy"))))
        GOPHER_proxy = p;
    if (!non_null(FTP_proxy) && ((p = getenv("FTP_PROXY")) || (p = getenv("ftp_proxy")) || (p = getenv("FTP_proxy"))))
        FTP_proxy = p;
    if (!non_null(NO_proxy) && ((p = getenv("NO_PROXY")) || (p = getenv("no_proxy")) || (p = getenv("NO_proxy"))))
        NO_proxy = p;
    if (!non_null(NNTP_server) && (p = getenv("NNTPSERVER")) != NULL)
        NNTP_server = p;
    if (!non_null(NNTP_mode) && (p = getenv("NNTPMODE")) != NULL)
        NNTP_mode = p;

    if (!non_null(Editor) && (p = getenv("EDITOR")) != NULL)
        Editor = p;
    if (!non_null(Mailer) && (p = getenv("MAILER")) != NULL)
        Mailer = p;
}

void proc_buf(Buffer* newbuf, char search_header, int open_new_tab)
{
    if (newbuf == NO_BUFFER)
        return;

    if (newbuf->pagerSource || (newbuf->real_scheme == SCM_LOCAL && newbuf->header_source && newbuf->currentURL.file && strcmp(newbuf->currentURL.file, "-")))
        newbuf->search_header = search_header;
    if (!CurrentTab) {
        FirstTab = LastTab = CurrentTab = newTab();
        if (!FirstTab) {
            fprintf(stderr, "%s\n", "Can't allocated memory");
            exit(3);
        }
        nTab = 1;
        Firstbuf = Currentbuf = newbuf;
    } else if (open_new_tab) {
        _newT();
        Currentbuf->nextBuffer = newbuf;
        delBuffer(Currentbuf);
    } else {
        Currentbuf->nextBuffer = newbuf;
        Currentbuf = newbuf;
    }
    if ((!w3m_dump || w3m_dump == DUMP_BUFFER)
        && Currentbuf->frameset && RenderFrame)
        rFrame();
    if (w3m_dump)
        do_dump(Currentbuf);
    else
        Currentbuf = newbuf;
}

static void
fversion(FILE* f)
{
    fprintf(f, "w3m version %s, options %s\n", W3M_VERSION,
#if LANG == JA
        "lang=ja"
#else
        "lang=en"
#endif
        ",m17n"
        ",image"
        ",color"
        ",ansi-color"
        ",menu"
        ",cookie"
        ",ssl"
        ",ssl-verify"
        ",nntp"
        ",gopher"
        ",ipv6"
        ",alarm"
        ",mark"
        ",history"
        ",dict");
}

static void
usage(void)
{
    fputs("usage: w3m [OPTION]... [URL | file]...\n", stderr);
    fputs("Try 'w3m -h' for more information.\n", stderr);
    w3m_exit(2);
}

#define PUT(a, b) printf("    %-16s %s\n", a, b)
static void
help(void)
{
    /* FIXME: gettextize? */
    puts("usage: w3m [OPTION]... [URL | file]...");
    puts("options:");
    PUT("+<num>", "goto <num> line");
    PUT("-B", "load bookmark");
    PUT("-I charset", "document charset");
    PUT("-N", "open URL of command line on each new tab");
    PUT("-O charset", "display/output charset");
    PUT("-R", "restore from session file");
    PUT("-T type", "specify content-type");
    PUT("-X", "don't use termcap init/deinit");
    PUT("-bookmark file", "specify bookmark file");
    PUT("-cols width", "specify column width (used with -dump)");
    PUT("-config file", "specify config file");
    PUT("-dump", "dump formatted page into stdout");
    PUT("-dump_both", "dump HEAD and source into stdout");
    PUT("-dump_extra", "dump HEAD, source, and extra information into stdout");
    PUT("-dump_head", "dump response of HEAD request into stdout");
    PUT("-dump_source", "dump page source into stdout");
    PUT("-num", "show line number");
    PUT("-o opt=value", "assign value to config option");
    PUT("-o", "print all config options");
    PUT("-r", "ignore backspace effect");
    PUT("-s", "squeeze multiple blank lines");
    PUT("-session file", "specify session file");
    puts("");
    PUT("-help", "print this message");
    PUT("-version", "print w3m version");
    puts("");
    puts("For more details see w3m(1).");
    w3m_exit(0);
}
#undef PUT

static void
wrap_GC_warn_proc(char* msg, GC_word arg)
{
    if (fmInitialized) {
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
                tty_sleep_till_anykey(1, 1);
            }

            lock = 0;
        }
    } else if (orig_GC_warn_proc)
        orig_GC_warn_proc(msg, arg);
    else
        fprintf(stderr, msg, (unsigned long)arg);
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
    return;
}

static pStr
make_optional_header_string(char* s)
{
    char* p;
    pStr hs;

    if (strchr(s, '\n') || strchr(s, '\r'))
        return NULL;
    for (p = s; *p && *p != ':'; p++)
        ;
    if (*p != ':' || p == s)
        return NULL;
    hs = Strnew_size(strlen(s) + 3);
    Strcopy_charp_n(hs, s, p - s);
    if (!Strcasecmp_charp(hs, "content-type"))
        override_content_type = true;
    if (!Strcasecmp_charp(hs, "user-agent"))
        override_user_agent = true;
    Strcat_charp(hs, ": ");
    if (*(++p)) { /* not null header */
        SKIP_BLANKS(p); /* skip white spaces */
        Strcat_charp(hs, p);
    }
    Strcat_charp(hs, "\r\n");
    return hs;
}

static void*
die_oom(size_t bytes)
{
    fprintf(stderr, "Out of memory: %zu bytes unavailable!\n", bytes);
    exit(3);
    /*
     * Suppress compiler warning: function might return no value
     * This code is never reached.
     */
    return NULL;
}

static void
keyPressEventProc(int c)
{
    CurrentKey = c;
    w3mFuncList[(int)GlobalKeymap[c]].func();
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

static void
dump_source(Buffer* buf)
{
    FILE* f;
    int c;
    if (buf->sourcefile == NULL)
        return;
    f = fopen(buf->sourcefile, "r");
    if (f == NULL)
        return;
    while ((c = fgetc(f)) != EOF) {
        putchar(c);
    }
    fclose(f);
}

static void
dump_head(Buffer* buf)
{
    struct TextListItem* ti;

    if (buf->document_header == NULL) {
        if (w3m_dump & DUMP_EXTRA)
            printf("\n");
        return;
    }
    for (ti = buf->document_header->first; ti; ti = ti->next) {
        pStr os = Strnew();
        struct Writer w = makeWriter(os);
        wc_Str_conv_strict(&WcOption, &w,
            (const uint8_t*)ti->ptr, (const uint8_t*)ti->ptr + strlen(ti->ptr), InnerCharset,
            buf->document_charset);
        printf("%s", os->ptr);
    }
    puts("");
}

static void
dump_extra(Buffer* buf)
{
    printf("W3m-current-url: %s\n", parsedURL2Str(&buf->currentURL)->ptr);
    if (buf->baseURL)
        printf("W3m-base-url: %s\n", parsedURL2Str(buf->baseURL)->ptr);
    printf("W3m-document-charset: %s\n",
        wc_ces_to_charset(buf->document_charset));
    if (buf->ssl_certificate) {
        pStr tmp = Strnew();
        const char* p;
        for (p = buf->ssl_certificate; *p; p++) {
            Strcat_char(tmp, *p);
            if (*p == '\n') {
                for (; *(p + 1) == '\n'; p++)
                    ;
                if (*(p + 1))
                    Strcat_char(tmp, '\t');
            }
        }
        if (Strlastchar(tmp) != '\n')
            Strcat_char(tmp, '\n');
        printf("W3m-ssl-certificate: %s", tmp->ptr);
    }
}

static int
cmp_anchor_hseq(const void* a, const void* b)
{
    return (*((const Anchor* const*)a))->hseq - (*((const Anchor* const*)b))->hseq;
}

static void
do_dump(Buffer* buf)
{
    volatile SigActionFunc prevtrap = NULL;

    prevtrap = mySignal(SIGINT, intTrap);
    if (SETJMP(IntReturn) != 0) {
        mySignal(SIGINT, prevtrap);
        return;
    }
    if (w3m_dump & DUMP_EXTRA)
        dump_extra(buf);
    if (w3m_dump & DUMP_HEAD)
        dump_head(buf);
    if (w3m_dump & DUMP_SOURCE)
        dump_source(buf);
    if (w3m_dump == DUMP_BUFFER) {
        int i;
        saveBuffer(buf, stdout, false);
        if (displayLinkNumber && buf->hrefList) {
            int nanchor = buf->hrefList->nanchor;
            printf("\nReferences:\n\n");
            Anchor** in_order = New_N(Anchor*, buf->hrefList->nanchor);
            for (i = 0; i < nanchor; i++)
                in_order[i] = buf->hrefList->anchors + i;
            qsort(in_order, nanchor, sizeof(Anchor*), cmp_anchor_hseq);
            for (i = 0; i < nanchor; i++) {
                struct Url pu;
                char* url;
                if (in_order[i]->slave)
                    continue;
                pu = parseURL2(in_order[i]->url, baseURL(buf));
                url = url_decode2(parsedURL2Str(&pu)->ptr, Currentbuf)->ptr;
                printf("[%d] %s\n", in_order[i]->hseq + 1 - !!zeroBasedLinkNo, url);
            }
        }
    }
    mySignal(SIGINT, prevtrap);
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

DEFUN(escmap, ESCMAP, "ESC map")
{
    char c;
    c = tty_getch();
    if (IS_ASCII(c))
        escKeyProc(c, K_ESC, EscKeymap);
}

DEFUN(escbmap, ESCBMAP, "ESC [ map")
{
    char c;
    c = tty_getch();
    if (IS_DIGIT(c)) {
        escdmap(c);
        return;
    }
    if (IS_ASCII(c))
        escKeyProc(c, K_ESCB, EscBKeymap);
}

static void
escdmap(char c)
{
    int d;
    d = (int)c - (int)'0';
    c = tty_getch();
    if (IS_DIGIT(c)) {
        d = d * 10 + (int)c - (int)'0';
        c = tty_getch();
    }
    if (c == '~')
        escKeyProc(d, K_ESCD, EscDKeymap);
}

DEFUN(multimap, MULTIMAP, "multimap")
{
    char c;
    c = tty_getch();
    if (IS_ASCII(c)) {
        CurrentKey = K_MULTI | (CurrentKey << 16) | c;
        escKeyProc(c, 0, NULL);
    }
}

static void
delBuffer(Buffer* buf)
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

static void
intTrap(SIGNAL_ARG)
{ /* Interrupt catcher */
    LONGJMP(IntReturn, 0);
}

static void
resize_hook(SIGNAL_ARG)
{
    need_resize_screen = true;
    mySignal(SIGWINCH, resize_hook);
}

static void
resize_screen(void)
{
    need_resize_screen = false;
    tty_update_size();
    setupscreen();
    if (CurrentTab)
        displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

static void
SigPipe(SIGNAL_ARG)
{
    mySignal(SIGPIPE, SigPipe);
}

/*
 * Command functions: These functions are called with a keystroke.
 */

static void
nscroll(int n, int mode)
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
        llnum = buf->topLine->linenumber + buf->lines - 1;
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
    displayBuffer(buf, mode);
}

/* Move page forward */
DEFUN(pgFore, NEXT_PAGE, "Scroll down one page")
{
    if (vi_prec_num)
        nscroll(searchKeyNum() * (Currentbuf->lines - 1), B_NORMAL);
    else
        nscroll(prec_num ? searchKeyNum() : searchKeyNum() * (Currentbuf->lines - 1), prec_num ? B_SCROLL : B_NORMAL);
}

/* Move page backward */
DEFUN(pgBack, PREV_PAGE, "Scroll up one page")
{
    if (vi_prec_num)
        nscroll(-searchKeyNum() * (Currentbuf->lines - 1), B_NORMAL);
    else
        nscroll(-(prec_num ? searchKeyNum() : searchKeyNum() * (Currentbuf->lines - 1)), prec_num ? B_SCROLL : B_NORMAL);
}

/* Move half page forward */
DEFUN(hpgFore, NEXT_HALF_PAGE, "Scroll down half a page")
{
    nscroll(searchKeyNum() * (Currentbuf->lines / 2 - 1), B_NORMAL);
}

/* Move half page backward */
DEFUN(hpgBack, PREV_HALF_PAGE, "Scroll up half a page")
{
    nscroll(-searchKeyNum() * (Currentbuf->lines / 2 - 1), B_NORMAL);
}

/* 1 line up */
DEFUN(lup1, UP, "Scroll the screen up one line")
{
    nscroll(searchKeyNum(), B_SCROLL);
}

/* 1 line down */
DEFUN(ldown1, DOWN, "Scroll the screen down one line")
{
    nscroll(-searchKeyNum(), B_SCROLL);
}

/* move cursor position to the center of screen */
DEFUN(ctrCsrV, CENTER_V, "Center on cursor line")
{
    int offsety;
    if (Currentbuf->firstLine == NULL)
        return;
    offsety = /*Currentbuf->lines / 2*/ -Currentbuf->cursorY;
    if (offsety != 0) {
        Currentbuf->topLine = lineSkip(Currentbuf, Currentbuf->topLine, -offsety, false);
        arrangeLine(Currentbuf);
        displayBuffer(Currentbuf, B_NORMAL);
    }
}

DEFUN(ctrCsrH, CENTER_H, "Center on cursor column")
{
    int offsetx;
    if (Currentbuf->firstLine == NULL)
        return;
    offsetx = Currentbuf->cursorX - Currentbuf->cols / 2;
    if (offsetx != 0) {
        columnSkip(Currentbuf, offsetx);
        arrangeCursor(Currentbuf);
        displayBuffer(Currentbuf, B_NORMAL);
    }
}

/* Redraw screen */
DEFUN(rdrwSc, REDRAW, "Draw the screen anew")
{
    clear();
    arrangeCursor(Currentbuf);
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

static void
clear_mark(Line* l)
{
    int pos;
    if (!l)
        return;
    for (pos = 0; pos < l->size; pos++)
        l->propBuf[pos] &= ~PE_MARK;
}

/* search by regular expression */
static int
srchcore(const char* str, int (*func)(Buffer*, const char*))
{
    volatile int i, result = SR_NOTFOUND;

    if (str != NULL && str != SearchString)
        SearchString = str;
    if (SearchString == NULL || *SearchString == '\0')
        return SR_NOTFOUND;

    str = conv_search_string(SearchString, DisplayCharset);
    SigActionFunc prevtrap = mySignal(SIGINT, intTrap);
    tty_crmode();
    if (SETJMP(IntReturn) == 0) {
        for (i = 0; i < PREC_NUM; i++) {
            result = func(Currentbuf, str);
            if (i < PREC_NUM - 1 && result & SR_FOUND)
                clear_mark(Currentbuf->currentLine);
        }
    }
    mySignal(SIGINT, prevtrap);
    tty_raw();
    return result;
}

static void
disp_srchresult(int result, const char* prompt, const char* str)
{
    if (str == NULL)
        str = "";
    if (result & SR_NOTFOUND)
        disp_message(Sprintf("Not found: %s", str)->ptr, true);
    else if (result & SR_WRAPPED)
        disp_message(Sprintf("Search wrapped: %s", str)->ptr, true);
    else if (show_srch_str)
        disp_message(Sprintf("%s%s", prompt, str)->ptr, true);
}

static int
dispincsrch(int ch, pStr buf, Lineprop* prop)
{
    static Buffer sbuf;
    char* str;
    int do_next_search = false;

    if (ch == 0 && buf == NULL) {
        SAVE_BUFPOSITION(&sbuf); /* search starting point */
        return -1;
    }

    str = buf->ptr;
    switch (ch) {
    case 022: /* C-r */
        searchRoutine = backwardSearch;
        do_next_search = true;
        break;
    case 023: /* C-s */
        searchRoutine = forwardSearch;
        do_next_search = true;
        break;

    default:
        if (ch >= 0)
            return ch; /* use InputKeymap */
    }

    if (do_next_search) {
        if (*str) {
            if (searchRoutine == forwardSearch)
                Currentbuf->pos += 1;
            SAVE_BUFPOSITION(&sbuf);
            if (srchcore(str, searchRoutine) == SR_NOTFOUND
                && searchRoutine == forwardSearch) {
                Currentbuf->pos -= 1;
                SAVE_BUFPOSITION(&sbuf);
            }
            arrangeCursor(Currentbuf);
            displayBuffer(Currentbuf, B_FORCE_REDRAW);
            clear_mark(Currentbuf->currentLine);
            return -1;
        } else
            return 020; /* _prev completion for C-s C-s */
    } else if (*str) {
        RESTORE_BUFPOSITION(&sbuf);
        arrangeCursor(Currentbuf);
        srchcore(str, searchRoutine);
        arrangeCursor(Currentbuf);
    }
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
    clear_mark(Currentbuf->currentLine);
    return -1;
}

static void
isrch(int (*func)(Buffer*, const char*), const char* prompt)
{
    Buffer sbuf;
    SAVE_BUFPOSITION(&sbuf);
    dispincsrch(0, NULL, NULL); /* initialize incremental search state */

    searchRoutine = func;
    char* str = inputLineHistSearch(prompt, NULL, IN_STRING, TextHist, dispincsrch).ptr;
    if (str == NULL) {
        RESTORE_BUFPOSITION(&sbuf);
    }
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

typedef int (*BufferSearchFunc)(Buffer*, const char*);

static void
srch(BufferSearchFunc func, const char* prompt, const char* str, bool disp)
{
    int pos = Currentbuf->pos;
    if (func == forwardSearch)
        Currentbuf->pos += 1;
    int result = srchcore(str, func);
    if (result & SR_FOUND)
        clear_mark(Currentbuf->currentLine);
    else
        Currentbuf->pos = pos;
    displayBuffer(Currentbuf, B_NORMAL);
    if (disp)
        disp_srchresult(result, prompt, str);
    searchRoutine = func;
}

DEFUN(srchfor_at, SEARCH_WORD_AT, "Search forward for word at cursor")
{
    const char* prompt = "ForwardAt: ";
    const char* str = GetWord(Currentbuf).ptr;
    if (str == NULL || *str == '\0') {
        displayBuffer(Currentbuf, B_NORMAL);
        return;
    }

    srch(forwardSearch, prompt, str, true);
}

DEFUN(srchfor, SEARCH SEARCH_FORE WHEREIS, "Search forward")
{
    const char* prompt = "Forward: ";
    bool disp = false;
    const char* str = searchKeyData();
    if (str == NULL || *str == '\0') {
        str = inputStrHist(prompt, NULL, TextHist).ptr;
        if (str != NULL && *str == '\0')
            str = SearchString;
        if (str == NULL) {
            displayBuffer(Currentbuf, B_NORMAL);
            return;
        }
        disp = true;
    }

    srch(forwardSearch, prompt, str, disp);
}

DEFUN(srchbak, SEARCH_BACK, "Search backward")
{
    const char* prompt = "Backward: ";
    bool disp = false;
    const char* str = searchKeyData();
    if (str == NULL || *str == '\0') {
        str = inputStrHist(prompt, NULL, TextHist).ptr;
        if (str != NULL && *str == '\0')
            str = SearchString;
        if (str == NULL) {
            displayBuffer(Currentbuf, B_NORMAL);
            return;
        }
        disp = true;
    }

    srch(backwardSearch, prompt, str, disp);
}

DEFUN(isrchfor, ISEARCH, "Incremental search forward")
{
    isrch(forwardSearch, "I-search: ");
}

DEFUN(isrchbak, ISEARCH_BACK, "Incremental search backward")
{
    isrch(backwardSearch, "I-search backward: ");
}

static void
srch_nxtprv(int reverse)
{
    int result;
    /* *INDENT-OFF* */
    static int (*routine[2])(Buffer*, const char*) = {
        forwardSearch, backwardSearch
    };
    /* *INDENT-ON* */

    if (searchRoutine == NULL) {
        disp_message(_("No previous regular expression"), true);
        return;
    }
    if (reverse != 0)
        reverse = 1;
    if (searchRoutine == backwardSearch)
        reverse ^= 1;
    if (reverse == 0)
        Currentbuf->pos += 1;
    result = srchcore(SearchString, routine[reverse]);
    if (result & SR_FOUND)
        clear_mark(Currentbuf->currentLine);
    else {
        if (reverse == 0)
            Currentbuf->pos -= 1;
    }
    displayBuffer(Currentbuf, B_NORMAL);
    disp_srchresult(result, (reverse ? "Backward: " : "Forward: "),
        SearchString);
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
shiftvisualpos(Buffer* buf, int shift)
{
    Line* l = buf->currentLine;
    buf->visualpos -= shift;
    if (buf->visualpos - l->bwidth >= buf->cols)
        buf->visualpos = l->bwidth + buf->cols - 1;
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
    columnSkip(Currentbuf, searchKeyNum() * (-Currentbuf->cols + 1) + 1);
    shiftvisualpos(Currentbuf, Currentbuf->currentColumn - column);
    displayBuffer(Currentbuf, B_NORMAL);
}

/* Shift screen right */
DEFUN(shiftr, SHIFT_RIGHT, "Shift screen right")
{
    int column;

    if (Currentbuf->firstLine == NULL)
        return;
    column = Currentbuf->currentColumn;
    columnSkip(Currentbuf, searchKeyNum() * (Currentbuf->cols - 1) - 1);
    shiftvisualpos(Currentbuf, Currentbuf->currentColumn - column);
    displayBuffer(Currentbuf, B_NORMAL);
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
    displayBuffer(Currentbuf, B_NORMAL);
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
    displayBuffer(Currentbuf, B_NORMAL);
}

DEFUN(cd, CD, "Change working directory")
{
    char* dir = inputFilename(_("cd to? "), NULL).ptr;
    if (chdir(dir) == -1)
        disp_err_message(strerror(errno), false);
    CurrentDir = currentdir();
}

DEFUN(setEnv, SETENV, "Set environment variable")
{
    const char* env = searchKeyData();
    if (env == NULL || *env == '\0' || strchr(env, '=') == NULL) {
        if (env != NULL && *env != '\0')
            env = Sprintf("%s=", env)->ptr;
        env = inputStrHist("Set environ: ", env, TextHist).ptr;
        if (env == NULL || *env == '\0') {
            displayBuffer(Currentbuf, B_NORMAL);
            return;
        }
    }
    const char *var, *value;
    if ((value = strchr(env, '=')) != NULL && value > env) {
        var = allocStr_n(env, value - env).ptr;
        value++;
        set_environ(var, value);
    }
    displayBuffer(Currentbuf, B_NORMAL);
}

DEFUN(pipeBuf, PIPE_BUF, "Pipe current buffer through a shell command and display output")
{
    const char* cmd = searchKeyData();
    if (cmd == NULL || *cmd == '\0') {
        cmd = inputLineHist(_("Pipe buffer to: "), "", IN_COMMAND, ShellHist).ptr;
    }
    if (cmd != NULL) {
        pStr os = Strnew();
        struct Writer w = makeWriter(os);
        conv_to_system(&WcOption, &w, cmd);
        cmd = os->ptr;
    }
    if (cmd == NULL || *cmd == '\0') {
        displayBuffer(Currentbuf, B_NORMAL);
        return;
    }
    char* tmpf = tmpfname(CurrentPid, TMPF_DFL, NULL)->ptr;
    FILE* f = fopen(tmpf, "w");
    if (f == NULL) {
        disp_message(Sprintf(_("Can't save buffer to %s"), cmd)->ptr, true);
        return;
    }
    saveBuffer(Currentbuf, f, true);
    fclose(f);
    Buffer* buf = getpipe(myExtCommand(cmd, shell_quote(tmpf)->ptr, true)->ptr);
    if (buf == NULL) {
        disp_message("Execution failed", true);
        return;
    } else {
        buf->filename = cmd;
        pStr os = Strnew();
        struct Writer w = makeWriter(os);
        conv_from_system(&WcOption, &w, cmd);
        buf->buffername = Sprintf("%s %s", PIPEBUFFERNAME, os->ptr)->ptr;
        buf->bufferprop |= (BP_INTERNAL | BP_NO_URL);
        if (buf->type == NULL)
            buf->type = "text/plain";
        buf->currentURL.file = "-";
        pushBuffer(buf);
    }
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

/* Execute shell command and read output ac pipe. */
DEFUN(pipesh, PIPE_SHELL, "Execute shell command and display output")
{
    const char* cmd = searchKeyData();
    if (cmd == NULL || *cmd == '\0') {
        cmd = inputLineHist("(read shell[pipe])!", "", IN_COMMAND, ShellHist).ptr;
    }
    if (cmd != NULL) {
        pStr os = Strnew();
        struct Writer w = makeWriter(os);
        conv_to_system(&WcOption, &w, cmd);
        cmd = os->ptr;
    }
    if (cmd == NULL || *cmd == '\0') {
        displayBuffer(Currentbuf, B_NORMAL);
        return;
    }
    Buffer* buf = getpipe(cmd);
    if (buf == NULL) {
        disp_message("Execution failed", true);
        return;
    } else {
        buf->bufferprop |= (BP_INTERNAL | BP_NO_URL);
        if (buf->type == NULL)
            buf->type = "text/plain";
        pushBuffer(buf);
    }
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

/* Execute shell command and load entire output to buffer */
DEFUN(readsh, READ_SHELL, "Execute shell command and display output")
{
    const char* cmd = searchKeyData();
    if (cmd == NULL || *cmd == '\0') {
        cmd = inputLineHist("(read shell)!", "", IN_COMMAND, ShellHist).ptr;
    }
    if (cmd != NULL) {
        pStr os = Strnew();
        struct Writer w = makeWriter(os);
        conv_to_system(&WcOption, &w, cmd);
        cmd = os->ptr;
    }
    if (cmd == NULL || *cmd == '\0') {
        displayBuffer(Currentbuf, B_NORMAL);
        return;
    }
    SigActionFunc prevtrap = mySignal(SIGINT, intTrap);
    tty_crmode();
    Buffer* buf = getshell(cmd);
    mySignal(SIGINT, prevtrap);
    tty_raw();
    if (buf == NULL) {
        disp_message(_("Execution failed"), true);
        return;
    } else {
        buf->bufferprop |= (BP_INTERNAL | BP_NO_URL);
        if (buf->type == NULL)
            buf->type = "text/plain";
        pushBuffer(buf);
    }
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

/* Execute shell command */
DEFUN(execsh, EXEC_SHELL SHELL, "Execute shell command and display output")
{
    const char* cmd = searchKeyData();
    if (cmd == NULL || *cmd == '\0') {
        cmd = inputLineHist("(exec shell)!", "", IN_COMMAND, ShellHist).ptr;
    }
    if (cmd != NULL) {
        pStr os = Strnew();
        struct Writer w = makeWriter(os);
        conv_to_system(&WcOption, &w, cmd);
        cmd = os->ptr;
    }
    if (cmd != NULL && *cmd != '\0') {
        fmTerm();
        printf("\n");
        (void)!system(cmd); /* We do not care about the exit code here! */
        printf(_("\n[Hit any key]"));
        fflush(stdout);
        fmInit();
        tty_getch();
    }
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

/* Load file */
DEFUN(ldfile, LOAD, "Open local file in a new buffer")
{
    const char* fn = searchKeyData();
    if (fn == NULL || *fn == '\0') {
        fn = inputFilenameHist(_("(Load)Filename? "), NULL, LoadHist).ptr;
    }
    if (fn != NULL) {
        pStr os = Strnew();
        struct Writer w = makeWriter(os);
        conv_to_system(&WcOption, &w, fn);
        fn = os->ptr;
    }
    if (fn == NULL || *fn == '\0') {
        displayBuffer(Currentbuf, B_NORMAL);
        return;
    }
    cmd_loadfile(fn);
}

/* Load help file */
DEFUN(ldhelp, HELP, "Show help panel")
{
    char* lang;
    int n;
    pStr tmp;

    lang = AcceptLang;
    n = strcspn(lang, ";, \t");
    tmp = Sprintf("file:///$LIB/" HELP_CGI CGI_EXTENSION "?version=%s&lang=%s",
        Str_form_quote(Strnew_charp(W3M_VERSION))->ptr,
        Str_form_quote(Strnew_charp_n(lang, n))->ptr);
    cmd_loadURL(tmp->ptr, NULL, NO_REFERER, NULL);
}

static void
cmd_loadfile(const char* fn)
{
    Buffer* buf = loadGeneralFile(file_to_url(fn, CurrentDir)->ptr, NULL, NO_REFERER, 0, NULL);
    if (buf == NULL) {
        pStr os = Strnew();
        struct Writer w = makeWriter(os);
        conv_from_system(&WcOption, &w, fn);
        char* emsg = Sprintf(_("%s not found"), os->ptr)->ptr;
        disp_err_message(emsg, false);
    } else if (buf != NO_BUFFER) {
        pushBuffer(buf);
        if (RenderFrame && Currentbuf->frameset != NULL)
            rFrame();
    }
    displayBuffer(Currentbuf, B_NORMAL);
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
    displayBuffer(Currentbuf, B_NORMAL);
}

DEFUN(movL, MOVE_LEFT, "Cursor left")
{
    _movL(Currentbuf->cols / 2);
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
    displayBuffer(Currentbuf, B_NORMAL);
}

DEFUN(movD, MOVE_DOWN, "Cursor down")
{
    _movD((Currentbuf->lines + 1) / 2);
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
    displayBuffer(Currentbuf, B_NORMAL);
}

DEFUN(movU, MOVE_UP, "Cursor up")
{
    _movU((Currentbuf->lines + 1) / 2);
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
    displayBuffer(Currentbuf, B_NORMAL);
}

DEFUN(movR, MOVE_RIGHT, "Cursor right")
{
    _movR(Currentbuf->cols / 2);
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
                if (is_wordchar(getChar(&WcOption, &lb[tmp])))
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
            if (!is_wordchar(getChar(&WcOption, &lb[tmp])))
                break;
            Currentbuf->pos = tmp;
        }
    }
end:
    arrangeCursor(Currentbuf);
    displayBuffer(Currentbuf, B_NORMAL);
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
        while (Currentbuf->pos < l->len && is_wordchar(getChar(&WcOption, &lb[Currentbuf->pos])))
            nextChar(Currentbuf->pos, l);

        while (1) {
            while (Currentbuf->pos < l->len && !is_wordchar(getChar(&WcOption, &lb[Currentbuf->pos])))
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
    displayBuffer(Currentbuf, B_NORMAL);
}

static void
_quitfm(int ask)
{
    if (checkDownloadList()
        && !confirm(_("Download process retains."
                      "Do you want to exit w3m?")))
        goto nope;
    else if (ask && !confirm(_("Do you want to exit w3m?")))
        goto nope;

    tty_title(""); /* XXX */
    if (activeImage)
        termImage();
    fmTerm();
    save_cookies();
    if (UseHistory && SaveURLHist)
        saveUrlHistory();
    if (deprecated)
        fputs("DEPRECATION WARNING\n", stderr);
    if (deprecated & 1) {
        fputs("-H is deprecated and will be removed in the future.\n", stderr);
        fputs("Use -o highIntensityColors=true instead.\n", stderr);
        fputs("-backend is deprecated and will be removed in the future.\n", stderr);
    }
    if (deprecated & 2) {
        fputs("OS2/ and MinGW support is deprecated and will be removed in the future.\n", stderr);
        fputs("Speak up at ~rkta/w3m@lists.sr.ht\n", stderr);
    }
    w3m_exit(0);

nope:
    displayBuffer(Currentbuf, B_NORMAL);
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
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

/* Suspend (on BSD), or run interactive shell (on SysV) */
DEFUN(susp, INTERRUPT SUSPEND, "Suspend w3m to background")
{
#ifndef SIGSTOP
    char* shell;
#endif /* not SIGSTOP */
    move(LASTLINE, 0);
    clrtoeolx();
    refresh(tty_file());
    fmTerm();
#ifndef SIGSTOP
    shell = getenv("SHELL");
    if (shell == NULL)
        shell = "/bin/sh";
    system(shell);
#else /* SIGSTOP */
#ifdef SIGTSTP
    signal(SIGTSTP, SIG_DFL); /* just in case */
    /*
     * Note: If susp() was called from SIGTSTP handler,
     * unblocking SIGTSTP would be required here.
     * Currently not.
     */
    kill(0, SIGTSTP); /* stop whole job, not a single process */
#else
    kill((pid_t)0, SIGSTOP);
#endif
#endif /* SIGSTOP */
    fmInit();
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

/* Go to specified line */
static void
_goLine(const char* l)
{
    if (l == NULL || *l == '\0' || Currentbuf->currentLine == NULL) {
        displayBuffer(Currentbuf, B_FORCE_REDRAW);
        return;
    }
    Currentbuf->pos = 0;
    if (((*l == '^') || (*l == '$')) && prec_num) {
        gotoRealLine(Currentbuf, prec_num);
    } else if (*l == '^') {
        Currentbuf->topLine = Currentbuf->currentLine = Currentbuf->firstLine;
    } else if (*l == '$') {
        Currentbuf->topLine = lineSkip(Currentbuf, Currentbuf->lastLine,
            -(Currentbuf->lines + 1) / 2, true);
        Currentbuf->currentLine = Currentbuf->lastLine;
    } else
        gotoRealLine(Currentbuf, atoi(l));
    arrangeCursor(Currentbuf);
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

DEFUN(goLine, GOTO_LINE, "Go to the specified line")
{

    const char* str = searchKeyData();
    if (prec_num)
        _goLine("^");
    else if (str)
        _goLine(str);
    else
        _goLine(inputStr(_("Goto line: "), "").ptr);
}

DEFUN(goLineF, BEGIN, "Go to the first line")
{
    _goLine("^");
}

DEFUN(goLineL, END, "Go to the last line")
{
    _goLine("$");
}

/* Go to the beginning of the line */
DEFUN(linbeg, LINE_BEGIN, "Go to the beginning of the line")
{
    if (Currentbuf->firstLine == NULL)
        return;
    while (Currentbuf->currentLine->prev && Currentbuf->currentLine->bpos)
        cursorUp0(Currentbuf, 1);
    Currentbuf->pos = 0;
    arrangeCursor(Currentbuf);
    displayBuffer(Currentbuf, B_NORMAL);
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
    displayBuffer(Currentbuf, B_NORMAL);
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
    const char* fn = Currentbuf->filename;
    pStr cmd;

    if (fn == NULL || Currentbuf->pagerSource != NULL || /* Behaving as a pager */
        (Currentbuf->type == NULL && Currentbuf->edit == NULL) || /* Reading shell */
        Currentbuf->real_scheme != SCM_LOCAL || !strcmp(Currentbuf->currentURL.file, "-") || /* file is std input  */
        Currentbuf->bufferprop & BP_FRAME) { /* Frame */
        disp_err_message("Can't edit other than local file", true);
        return;
    }
    if (Currentbuf->edit)
        cmd = unquote_mailcap(Currentbuf->edit, Currentbuf->real_type, fn,
            checkHeader(Currentbuf, "Content-Type:"), NULL);
    else
        cmd = editor_cmd(shell_quote(fn)->ptr,
            cur_real_linenumber(Currentbuf));
    exec_cmd(cmd->ptr);

    displayBuffer(Currentbuf, B_FORCE_REDRAW);
    reload();
}

/* Run editor on the current screen */
DEFUN(editScr, EDIT_SCREEN, "Edit rendered copy of document")
{
    char* tmpf;
    FILE* f;

    tmpf = tmpfname(CurrentPid, TMPF_DFL, NULL)->ptr;
    f = fopen(tmpf, "w");
    if (f == NULL) {
        disp_err_message(Sprintf(_("Can't open %s"), tmpf)->ptr, true);
        return;
    }
    saveBuffer(Currentbuf, f, true);
    fclose(f);
    exec_cmd(editor_cmd(shell_quote(tmpf)->ptr,
        cur_real_linenumber(Currentbuf))
            ->ptr);
    unlink(tmpf);
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
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
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
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
                displayBuffer(Currentbuf, B_NORMAL);
                return;
            }
        }
        l = l->next;
        i = 0;
    }
    disp_message(_("No mark exist after here"), true);
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
                displayBuffer(Currentbuf, B_NORMAL);
                return;
            }
        }
        l = l->prev;
        if (l != NULL)
            i = l->len - 1;
    }
    disp_message(_("No mark exist before here"), true);
}

/* Mark place to which the regular expression matches */
DEFUN(reMark, REG_MARK, "Mark all occurences of a pattern")
{
    Line* l;
    const char* str;
    const char *p, *p1, *p2;

    if (!use_mark)
        return;
    str = searchKeyData();
    if (str == NULL || *str == '\0') {
        str = inputStrHist("(Mark)Regexp: ", MarkString, TextHist).ptr;
        if (str == NULL || *str == '\0') {
            displayBuffer(Currentbuf, B_NORMAL);
            return;
        }
    }
    str = conv_search_string(str, DisplayCharset);
    if ((str = regexCompile(str, 1)) != NULL) {
        disp_message(str, true);
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

    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

static Buffer*
loadNormalBuf(Buffer* buf, int renderframe)
{
    pushBuffer(buf);
    if (renderframe && RenderFrame && Currentbuf->frameset != NULL)
        rFrame();
    return buf;
}

static Buffer*
loadLink(const char* url, const char* target, const char* referer, FormList* request)
{
    Buffer *buf, *nfbuf;
    union frameset_element* f_element = NULL;
    int flag = 0;
    struct Url *base, pu;
    const int* no_referer_ptr;

    message(Sprintf("loading %s", url)->ptr, 0, 0);
    refresh(tty_file());

    no_referer_ptr = query_SCONF_NO_REFERER_FROM(&Currentbuf->currentURL);
    base = baseURL(Currentbuf);
    if ((no_referer_ptr && *no_referer_ptr) || base == NULL || base->scheme == SCM_LOCAL || base->scheme == SCM_LOCAL_CGI || base->scheme == SCM_DATA)
        referer = NO_REFERER;
    if (referer == NULL)
        referer = parsedURL2RefererStr(&Currentbuf->currentURL)->ptr;
    buf = loadGeneralFile(url, baseURL(Currentbuf), referer, flag, request);
    if (buf == NULL) {
        char* emsg = Sprintf("Can't load %s", url)->ptr;
        disp_err_message(emsg, false);
        return NULL;
    }

    pu = parseURL2(url, base);
    pushHashHist(URLHist, parsedURL2Str(&pu)->ptr);

    if (buf == NO_BUFFER) {
        return NULL;
    }
    if (!on_target) /* open link as an indivisual page */
        return loadNormalBuf(buf, true);

    if (do_download) /* download (thus no need to render frames) */
        return loadNormalBuf(buf, false);

    if (target == NULL || /* no target specified (that means this page is not a frame page) */
        !strcmp(target, "_top") || /* this link is specified to be opened as an indivisual * page */
        !(Currentbuf->bufferprop & BP_FRAME) /* This page is not a frame page */
    ) {
        return loadNormalBuf(buf, true);
    }
    nfbuf = Currentbuf->linkBuffer[LB_N_FRAME];
    if (nfbuf == NULL) {
        /* original page (that contains <frameset> tag) doesn't exist */
        return loadNormalBuf(buf, true);
    }

    f_element = search_frame(nfbuf->frameset, target);
    if (f_element == NULL) {
        /* specified target doesn't exist in this frameset */
        return loadNormalBuf(buf, true);
    }

    /* frame page */

    /* stack current frameset */
    pushFrameTree(&(nfbuf->frameQ), copyFrameSet(nfbuf->frameset), Currentbuf);
    /* delete frame view buffer */
    delBuffer(Currentbuf);
    Currentbuf = nfbuf;
    /* nfbuf->frameset = copyFrameSet(nfbuf->frameset); */
    resetFrameElement(f_element, buf, referer, request);
    discardBuffer(buf);
    rFrame();
    {
        Anchor* al = NULL;
        const char* label = pu.label;

        if (label && f_element->element->attr == F_BODY) {
            al = searchAnchor(f_element->body->nameList, label);
        }
        if (!al) {
            label = Strnew_m_charp("_", target, NULL)->ptr;
            al = searchURLLabel(Currentbuf, label);
        }
        if (al) {
            gotoLine(Currentbuf, al->start.line);
            if (label_topline)
                Currentbuf->topLine = lineSkip(Currentbuf, Currentbuf->topLine,
                    Currentbuf->currentLine->linenumber - Currentbuf->topLine->linenumber,
                    false);
            Currentbuf->pos = al->start.pos;
            arrangeCursor(Currentbuf);
        }
    }
    displayBuffer(Currentbuf, B_NORMAL);
    return buf;
}

static void
gotoLabel(const char* label)
{
    Buffer* buf;
    Anchor* al;
    int i;

    al = searchURLLabel(Currentbuf, label);
    if (al == NULL) {
        disp_message(Sprintf(_("%s is not found"), label)->ptr, true);
        return;
    }
    buf = newBuffer(Currentbuf->width);
    copyBuffer(buf, Currentbuf);
    for (i = 0; i < MAX_LB; i++)
        buf->linkBuffer[i] = NULL;
    buf->currentURL.label = allocStr(label).ptr;
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
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
    return;
}

static int
handleMailto(const char* url)
{
    pStr to;
    char* pos;

    if (strncasecmp(url, "mailto:", 7))
        return 0;
    if (!non_null(Mailer)) {
        disp_err_message(_("no mailer is specified"), true);
        return 1;
    }

    /* invoke external mailer */
    if (MailtoOptions == MAILTO_OPTIONS_USE_MAILTO_URL) {
        to = Strnew_charp(html_unquote(url));
    } else {
        to = Strnew_charp(url + 7);
        if ((pos = strchr(to->ptr, '?')) != NULL)
            Strtruncate(to, pos - to->ptr);
    }
    exec_cmd(myExtCommand(Mailer, shell_quote(file_unquote(to->ptr)->ptr)->ptr,
        false)
            ->ptr);
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
    pushHashHist(URLHist, url);
    return 1;
}

/* follow HREF link */
DEFUN(followA, GOTO_LINK, "Follow current hyperlink in a new buffer")
{
    if (Currentbuf->firstLine == NULL)
        return;

    Anchor* a = retrieveCurrentImg(Currentbuf);
    if (a && a->image && a->image->map) {
        _followForm(false);
        return;
    }
    bool map = false;
    int x = 0, y = 0;
    if (a && a->image && a->image->ismap) {
        getMapXY(Currentbuf, a, &x, &y);
        map = true;
    }
    a = retrieveCurrentAnchor(Currentbuf);
    if (a == NULL) {
        _followForm(false);
        return;
    }
    if (*a->url == '#') { /* index within this buffer */
        gotoLabel(a->url + 1);
        return;
    }

    struct Url u = parseURL2(a->url, baseURL(Currentbuf));
    if (Strcmp(parsedURL2Str(&u), parsedURL2Str(&Currentbuf->currentURL)) == 0) {
        /* index within this buffer */
        if (u.label) {
            gotoLabel(u.label);
            return;
        }
    }
    if (handleMailto(a->url))
        return;
    const char* url = a->url;

    if (map)
        url = Sprintf("%s?%d,%d", a->url, x, y)->ptr;

    if (check_target
        && open_tab_blank
        && a->target
        && (!strcasecmp(a->target, "_new") || !strcasecmp(a->target, "_blank"))) {
        Buffer* buf;

        _newT();
        buf = Currentbuf;
        loadLink(url, a->target, a->referer, NULL);
        if (buf != Currentbuf)
            delBuffer(buf);
        else
            deleteTab(CurrentTab);
        displayBuffer(Currentbuf, B_FORCE_REDRAW);
        return;
    }
    loadLink(url, a->target, a->referer, NULL);
    displayBuffer(Currentbuf, B_NORMAL);
}

/* view inline image */
DEFUN(followI, VIEW_IMAGE, "Display image in viewer")
{
    Anchor* a;
    Buffer* buf;

    if (Currentbuf->firstLine == NULL)
        return;

    a = retrieveCurrentImg(Currentbuf);
    if (a == NULL)
        return;
    message(Sprintf(_("loading %s"), a->url)->ptr, 0, 0);
    refresh(tty_file());
    buf = loadGeneralFile(a->url, baseURL(Currentbuf), NULL, 0, NULL);
    if (buf == NULL) {
        char* emsg = Sprintf(_("Can't load %s"), a->url)->ptr;
        disp_err_message(emsg, false);
    } else if (buf != NO_BUFFER) {
        pushBuffer(buf);
    }
    displayBuffer(Currentbuf, B_NORMAL);
}

static FormItemList*
save_submit_formlist(FormItemList* src)
{
    FormList* list;
    FormList* srclist;
    FormItemList* srcitem;
    FormItemList* item;
    FormItemList* ret = NULL;
    FormSelectOptionItem* opt;
    FormSelectOptionItem* curopt;
    FormSelectOptionItem* srcopt;

    if (src == NULL)
        return NULL;
    srclist = src->parent;
    list = New(FormList);
    list->method = srclist->method;
    list->action = Strdup(srclist->action);
    list->charset = srclist->charset;
    list->enctype = srclist->enctype;
    list->nitems = srclist->nitems;
    list->body = srclist->body;
    list->boundary = srclist->boundary;
    list->length = srclist->length;

    for (srcitem = srclist->item; srcitem; srcitem = srcitem->next) {
        item = New(FormItemList);
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
            opt = New(FormSelectOptionItem);
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

static pStr
conv_form_encoding(pStr is, FormItemList* fi, Buffer* buf)
{
    wc_ces charset = SystemCharset;

    if (fi->parent->charset)
        charset = fi->parent->charset;
    else if (buf->document_charset && buf->document_charset != WC_CES_US_ASCII)
        charset = buf->document_charset;
    pStr os = Strnew();
    struct Writer w = makeWriter(os);
    wc_Str_conv_strict(&WcOption, &w,
        (const uint8_t*)is->ptr, (const uint8_t*)is->ptr + is->len, InnerCharset, charset);
    return os;
}

static void
query_from_followform(pStr* query, FormItemList* fi, int multipart)
{
    FormItemList* f2;
    FILE* body = NULL;

    if (multipart) {
        *query = tmpfname(CurrentPid, TMPF_DFL, NULL);
        body = fopen((*query)->ptr, "w");
        if (body == NULL) {
            return;
        }
        fi->parent->body = (*query)->ptr;
        fi->parent->boundary = Sprintf("------------------------------%d%ld%ld%ld", CurrentPid,
            (long)fi->parent, (long)fi->parent->body, (long)fi->parent->boundary)
                                   ->ptr;
    }
    *query = Strnew();
    for (f2 = fi->parent->item; f2; f2 = f2->next) {
        if (f2->name == NULL)
            continue;
        /* <ISINDEX> is translated into single text form */
        if (f2->name->len == 0 && (multipart || f2->type != FORM_INPUT_TEXT))
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
            } else if (f2->name && f2->name->len > 0 && f2->value != NULL) {
                /* not IMAGE */
                *query = conv_form_encoding(f2->value, fi, Currentbuf);
                if (f2->type == FORM_INPUT_FILE)
                    form_write_from_file(body, fi->parent->boundary,
                        conv_form_encoding(f2->name, fi,
                            Currentbuf)
                            ->ptr,
                        (*query)->ptr,
                        Str_conv_to_system(&WcOption, f2->value)->ptr);
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
                if (f2->name && f2->name->len > 0) {
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

/* submit form */
DEFUN(submitForm, SUBMIT, "Submit form")
{
    _followForm(true);
}

static void
_followForm(int submit)
{
    Anchor *a, *a2;
    char* p;
    FormItemList *fi, *f2;
    pStr tmp, tmp2;
    int multipart = 0, i;

    if (Currentbuf->firstLine == NULL)
        return;

    a = retrieveCurrentForm(Currentbuf);
    if (a == NULL)
        return;
    fi = a->formitem;
    switch (fi->type) {
    case FORM_INPUT_TEXT:
        if (submit)
            goto do_submit;
        if (fi->readonly)
            /* FIXME: gettextize? */
            disp_message_nsec("Read only field!", false, 1, true, false);
        p = inputStrHist(_("TEXT:"), fi->value ? fi->value->ptr : NULL, TextHist).ptr;
        if (p == NULL || fi->readonly)
            break;
        fi->value = Strnew_charp(p);
        formUpdateBuffer(a, Currentbuf, fi);
        if (fi->accept || fi->parent->nitems == 1)
            goto do_submit;
        break;
    case FORM_INPUT_FILE:
        if (submit)
            goto do_submit;
        if (fi->readonly)
            /* FIXME: gettextize? */
            disp_message_nsec("Read only field!", false, 1, true, false);
        p = inputFilenameHist(_("Filename:"), fi->value ? fi->value->ptr : NULL,
            NULL)
                .ptr;
        if (p == NULL || fi->readonly)
            break;
        fi->value = Strnew_charp(p);
        formUpdateBuffer(a, Currentbuf, fi);
        if (fi->accept || fi->parent->nitems == 1)
            goto do_submit;
        break;
    case FORM_INPUT_PASSWORD:
        if (submit)
            goto do_submit;
        if (fi->readonly) {
            /* FIXME: gettextize? */
            disp_message_nsec("Read only field!", false, 1, true, false);
            break;
        }
        p = inputLine(_("Password:"), fi->value ? fi->value->ptr : NULL,
            IN_PASSWORD)
                .ptr;
        if (p == NULL)
            break;
        fi->value = Strnew_charp(p);
        formUpdateBuffer(a, Currentbuf, fi);
        if (fi->accept)
            goto do_submit;
        break;
    case FORM_TEXTAREA:
        if (submit)
            goto do_submit;
        if (fi->readonly)
            /* FIXME: gettextize? */
            disp_message_nsec("Read only field!", false, 1, true, false);
        input_textarea(fi);
        formUpdateBuffer(a, Currentbuf, fi);
        break;
    case FORM_INPUT_RADIO:
        if (submit)
            goto do_submit;
        if (fi->readonly) {
            /* FIXME: gettextize? */
            disp_message_nsec("Read only field!", false, 1, true, false);
            break;
        }
        formRecheckRadio(a, Currentbuf, fi);
        break;
    case FORM_INPUT_CHECKBOX:
        if (submit)
            goto do_submit;
        if (fi->readonly) {
            /* FIXME: gettextize? */
            disp_message_nsec("Read only field!", false, 1, true, false);
            break;
        }
        fi->checked = !fi->checked;
        formUpdateBuffer(a, Currentbuf, fi);
        break;
    case FORM_SELECT:
        if (submit)
            goto do_submit;
        if (!formChooseOptionByMenu(fi,
                Currentbuf->cursorX - Currentbuf->pos + a->start.pos + Currentbuf->rootX,
                Currentbuf->cursorY + Currentbuf->rootY))
            break;
        formUpdateBuffer(a, Currentbuf, fi);
        if (fi->parent->nitems == 1)
            goto do_submit;
        break;
    case FORM_INPUT_IMAGE:
    case FORM_INPUT_SUBMIT:
    case FORM_INPUT_BUTTON:
    do_submit:
        tmp = Strnew();
        multipart = (fi->parent->method == FORM_METHOD_POST && fi->parent->enctype == FORM_ENCTYPE_MULTIPART);
        query_from_followform(&tmp, fi, multipart);

        tmp2 = Strdup(fi->parent->action);
        if (!Strcmp_charp(tmp2, "!CURRENT_URL!")) {
            /* It means "current URL" */
            tmp2 = parsedURL2Str(&Currentbuf->currentURL);
            if ((p = strchr(tmp2->ptr, '?')) != NULL)
                Strshrink(tmp2, (tmp2->ptr + tmp2->len) - p);
        }

        if (fi->parent->method == FORM_METHOD_GET) {
            pStr fragment = NULL;

            if ((p = strchr(tmp2->ptr, '#'))) {
                fragment = Strnew_charp(p);
                Strshrink(tmp2, (tmp2->ptr + tmp2->len) - p);
            }

            if ((p = strchr(tmp2->ptr, '?')))
                Strshrink(tmp2, (tmp2->ptr + tmp2->len) - p);

            Strcat_char(tmp2, '?');
            Strcat(tmp2, tmp);

            if (fragment)
                Strcat(tmp2, fragment);

            loadLink(tmp2->ptr, a->target, NULL, NULL);
        } else if (fi->parent->method == FORM_METHOD_POST) {
            Buffer* buf;
            if (multipart) {
                struct stat st;
                stat(fi->parent->body, &st);
                fi->parent->length = st.st_size;
            } else {
                fi->parent->body = tmp->ptr;
                fi->parent->length = tmp->len;
            }
            buf = loadLink(tmp2->ptr, a->target, NULL, fi->parent);
            if (multipart) {
                unlink(fi->parent->body);
            }
            if (buf && !(buf->bufferprop & BP_REDIRECTED)) { /* buf must be Currentbuf */
                /* BP_REDIRECTED means that the buffer is obtained through
                 * Location: header. In this case, buf->form_submit must not be set
                 * because the page is not loaded by POST method but GET method.
                 */
                buf->form_submit = save_submit_formlist(fi);
            }
        } else if ((fi->parent->method == FORM_METHOD_INTERNAL && (!Strcmp_charp(fi->parent->action, "map") || !Strcmp_charp(fi->parent->action, "none"))) || Currentbuf->bufferprop & BP_INTERNAL) { /* internal */
            do_internal(tmp2->ptr, tmp->ptr);
        } else {
            disp_err_message("Can't send form because of illegal method.",
                false);
        }
        break;
    case FORM_INPUT_RESET:
        for (i = 0; i < Currentbuf->formList->nanchor; i++) {
            a2 = &Currentbuf->formList->anchors[i];
            f2 = a2->formitem;
            if (f2->parent == fi->parent && f2->name && f2->value && f2->type != FORM_INPUT_SUBMIT && f2->type != FORM_INPUT_HIDDEN && f2->type != FORM_INPUT_RESET) {
                f2->value = f2->init_value;
                f2->checked = f2->init_checked;
                f2->label = f2->init_label;
                f2->selected = f2->init_selected;
                formUpdateBuffer(a2, Currentbuf, f2);
            }
        }
        break;
    case FORM_INPUT_HIDDEN:
    default:
        break;
    }
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

DEFUN(goMain, MAIN, "Goto main content")
{
    if (!Currentbuf->mainline) {
        disp_err_message(_("<main> not found"), false);
        return;
    }
    gotoLine(Currentbuf, Currentbuf->mainline);
    Currentbuf->pos = 0;
    arrangeCursor(Currentbuf);
    displayBuffer(Currentbuf, B_NORMAL);
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

    if (prec_num > hl->nmark)
        hseq = hl->nmark - 1;
    else if (prec_num > 0)
        hseq = prec_num - 1 + !!zeroBasedLinkNo;
    do {
        if (hseq >= hl->nmark)
            return;
        po = hl->marks + hseq;
        an = retrieveAnchor(Currentbuf->hrefList, po->line, po->pos);
        if (an == NULL)
            an = retrieveAnchor(Currentbuf->formList, po->line, po->pos);
        hseq++;
    } while (an == NULL);

    gotoLine(Currentbuf, po->line);
    Currentbuf->pos = po->pos;
    arrangeCursor(Currentbuf);
    displayBuffer(Currentbuf, B_NORMAL);
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

    if (prec_num >= hl->nmark)
        hseq = 0;
    else if (prec_num > 0)
        hseq = hl->nmark - prec_num;
    else
        hseq = hl->nmark - 1;
    do {
        if (hseq < 0)
            return;
        po = hl->marks + hseq;
        an = retrieveAnchor(Currentbuf->hrefList, po->line, po->pos);
        if (an == NULL)
            an = retrieveAnchor(Currentbuf->formList, po->line, po->pos);
        hseq--;
    } while (an == NULL);

    gotoLine(Currentbuf, po->line);
    Currentbuf->pos = po->pos;
    arrangeCursor(Currentbuf);
    displayBuffer(Currentbuf, B_NORMAL);
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
    an = retrieveAnchor(Currentbuf->hrefList, po->line, po->pos);
    if (an == NULL)
        an = retrieveAnchor(Currentbuf->formList, po->line, po->pos);
    if (an == NULL)
        return;

    gotoLine(Currentbuf, po->line);
    Currentbuf->pos = po->pos;
    arrangeCursor(Currentbuf);
    displayBuffer(Currentbuf, B_NORMAL);
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
                an = retrieveAnchor(Currentbuf->hrefList, po->line, po->pos);
                if (visited != true && an == NULL)
                    an = retrieveAnchor(Currentbuf->formList, po->line,
                        po->pos);
                hseq++;
                if (visited == true && an) {
                    url = parseURL2(an->url, baseURL(Currentbuf));
                    if (getHashHist(URLHist, parsedURL2Str(&url)->ptr)) {
                        goto _end;
                    }
                }
            } while (an == NULL || an == pan);
        } else {
            an = closest_next_anchor(Currentbuf->hrefList, NULL, x, y);
            if (visited != true)
                an = closest_next_anchor(Currentbuf->formList, an, x, y);
            if (an == NULL) {
                if (visited == true)
                    return;
                an = pan;
                break;
            }
            x = an->start.pos;
            y = an->start.line;
            if (visited == true) {
                url = parseURL2(an->url, baseURL(Currentbuf));
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
    displayBuffer(Currentbuf, B_NORMAL);
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
                an = retrieveAnchor(Currentbuf->hrefList, po->line, po->pos);
                if (visited != true && an == NULL)
                    an = retrieveAnchor(Currentbuf->formList, po->line,
                        po->pos);
                hseq--;
                if (visited == true && an) {
                    url = parseURL2(an->url, baseURL(Currentbuf));
                    if (getHashHist(URLHist, parsedURL2Str(&url)->ptr)) {
                        goto _end;
                    }
                }
            } while (an == NULL || an == pan);
        } else {
            an = closest_prev_anchor(Currentbuf->hrefList, NULL, x, y);
            if (visited != true)
                an = closest_prev_anchor(Currentbuf->formList, an, x, y);
            if (an == NULL) {
                if (visited == true)
                    return;
                an = pan;
                break;
            }
            x = an->start.pos;
            y = an->start.line;
            if (visited == true) {
                url = parseURL2(an->url, baseURL(Currentbuf));
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
    displayBuffer(Currentbuf, B_NORMAL);
}

/* go to the next image */
static void
_nextI(void)
{
    BufferPoint* po;
    Anchor *an, *pan;
    AnchorList* al = Currentbuf->imgList;
    int i, x, y, n = searchKeyNum();

    if (Currentbuf->firstLine == NULL)
        return;
    if (al == NULL)
        return;
    if (!al || al->nanchor == 0)
        return;

    an = retrieveCurrentImg(Currentbuf);
    if (an == NULL) {
        an = retrieveCurrentAnchor(Currentbuf);
        if (an == NULL)
            an = retrieveCurrentForm(Currentbuf);
    }
    if (an != NULL) {
        y = an->start.line;
        x = an->start.pos;
    } else {
        y = Currentbuf->currentLine->linenumber;
        x = Currentbuf->pos;
    }
    for (i = 0; i < n; i++) {
        pan = an;
        an = closest_next_anchor(Currentbuf->imgList, NULL, x, y);
        if (an == NULL && pan == NULL)
            return;
        if (an == NULL) {
            an = pan;
            break;
        }
        x = an->start.pos;
        y = an->start.line;
    }
    po = &an->start;
    gotoLine(Currentbuf, po->line);
    Currentbuf->pos = po->pos;
    arrangeCursor(Currentbuf);
    displayBuffer(Currentbuf, B_NORMAL);
}

/* go to the previous image */
static void
_prevI(void)
{
    BufferPoint* po;
    Anchor *an, *pan;
    AnchorList* al = Currentbuf->imgList;
    int i, x, y, n = searchKeyNum();

    if (Currentbuf->firstLine == NULL)
        return;
    if (al == NULL)
        return;

    an = retrieveCurrentImg(Currentbuf);
    if (an == NULL) {
        an = retrieveCurrentAnchor(Currentbuf);
        if (an == NULL) {
            an = retrieveCurrentForm(Currentbuf);
        }
    }
    if (an != NULL) {
        y = an->start.line;
        x = an->start.pos;
    } else {
        y = Currentbuf->currentLine->linenumber;
        x = Currentbuf->pos;
    }
    for (i = 0; i < n; i++) {
        pan = an;
        an = closest_prev_anchor(Currentbuf->imgList, NULL, x, y);
        if (an == NULL && pan == NULL)
            return;
        if (an == NULL) {
            an = pan;
            break;
        }
        x = an->start.pos;
        y = an->start.line;
    }
    po = &an->start;
    gotoLine(Currentbuf, po->line);
    Currentbuf->pos = po->pos;
    arrangeCursor(Currentbuf);
    displayBuffer(Currentbuf, B_NORMAL);
}

DEFUN(nextI, NEXT_IMAGE, "Move to the next image hyperlink")
{
    _nextI();
}

DEFUN(prevI, PREV_IMAGE, "Move to the previous image hyperlink")
{
    _prevI();
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
                an = retrieveAnchor(Currentbuf->hrefList, y, x);
                if (!an)
                    an = retrieveAnchor(Currentbuf->formList, y, x);
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
    displayBuffer(Currentbuf, B_NORMAL);
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
            an = retrieveAnchor(Currentbuf->hrefList, y, x);
            if (!an)
                an = retrieveAnchor(Currentbuf->formList, y, x);
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
    displayBuffer(Currentbuf, B_NORMAL);
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
    Buffer* buf;
    int i;

    for (i = 0; i < PREC_NUM; i++) {
        buf = prevBuffer(Firstbuf, Currentbuf);
        if (!buf) {
            if (i == 0)
                return;
            break;
        }
        Currentbuf = buf;
    }
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

/* go to the previous bufferr */
DEFUN(prevBf, PREV, "Switch to the previous buffer")
{
    Buffer* buf;
    int i;

    for (i = 0; i < PREC_NUM; i++) {
        buf = Currentbuf->nextBuffer;
        if (!buf) {
            if (i == 0)
                return;
            break;
        }
        Currentbuf = buf;
    }
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

static int
checkBackBuffer(Buffer* buf)
{
    Buffer* fbuf = buf->linkBuffer[LB_N_FRAME];

    if (fbuf) {
        if (fbuf->frameQ)
            return true; /* Currentbuf has stacked frames */
        /* when no frames stacked and next is frame source, try next's
         * nextBuffer */
        if (RenderFrame && fbuf == buf->nextBuffer) {
            if (fbuf->nextBuffer != NULL)
                return true;
            else
                return false;
        }
    }

    if (buf->nextBuffer)
        return true;

    return false;
}

/* delete current buffer and back to the previous buffer */
DEFUN(backBf, BACK, "Close current buffer and return to the one below in stack")
{
    Buffer* buf = Currentbuf->linkBuffer[LB_N_FRAME];

    if (!checkBackBuffer(Currentbuf)) {
        if (exit_on_last && nTab == 1)
            _quitfm(false);
        else if (close_tab_back || exit_on_last) {
            deleteTab(CurrentTab);
            displayBuffer(Currentbuf, B_FORCE_REDRAW);
        } else
            disp_message(_("Can't go back..."), true);
        return;
    }

    delBuffer(Currentbuf);

    if (buf) {
        if (buf->frameQ) {
            struct frameset* fs;
            long linenumber = buf->frameQ->linenumber;
            long top = buf->frameQ->top_linenumber;
            int pos = buf->frameQ->pos;
            int currentColumn = buf->frameQ->currentColumn;
            AnchorList* formitem = buf->frameQ->formitem;

            fs = popFrameTree(&(buf->frameQ));
            deleteFrameSet(buf->frameset);
            buf->frameset = fs;

            if (buf == Currentbuf) {
                rFrame();
                Currentbuf->topLine = lineSkip(Currentbuf,
                    Currentbuf->firstLine, top - 1,
                    false);
                gotoLine(Currentbuf, linenumber);
                Currentbuf->pos = pos;
                Currentbuf->currentColumn = currentColumn;
                arrangeCursor(Currentbuf);
                formResetBuffer(Currentbuf, formitem);
            }
        } else if (RenderFrame && buf == Currentbuf) {
            delBuffer(Currentbuf);
        }
    }
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

DEFUN(deletePrevBuf, DELETE_PREVBUF, "Delete previous buffer (mainly for local CGI-scripts)")
{
    Buffer* buf = Currentbuf->nextBuffer;
    if (buf)
        delBuffer(buf);
}

static void
cmd_loadURL(const char* url, struct Url* current, const char* referer, FormList* request)
{

    if (handleMailto(url))
        return;

    refresh(tty_file());
    Buffer* buf = loadGeneralFile(url, current, referer, 0, request);
    if (buf == NULL) {
        pStr os = Strnew();
        struct Writer w = makeWriter(os);
        conv_from_system(&WcOption, &w, url);
        char* emsg = Sprintf(_("Can't load %s"), os->ptr)->ptr;
        disp_err_message(emsg, false);
    } else if (buf != NO_BUFFER) {
        pushBuffer(buf);
        if (RenderFrame && Currentbuf->frameset != NULL)
            rFrame();
    }
    displayBuffer(Currentbuf, B_NORMAL);
}

/* go to specified URL */
static void
goURL0(const char* prompt, bool relative, const char* url, bool force)
{
    Buffer* cur_buf = Currentbuf;
    const char* referer = NULL;
    struct Url* current = NULL;
    if (force || url == NULL) {
        Hist* hist = copyHist(URLHist);

        current = baseURL(Currentbuf);
        if (current) {
            char* c_url = parsedURL2Str(current)->ptr;
            if (DefaultURLString == DEFAULT_URL_CURRENT)
                url = url_decode2(c_url, NULL)->ptr;
            else
                pushHist(hist, c_url);
        }

        Anchor* a = retrieveCurrentAnchor(Currentbuf);
        if (a) {
            struct Url p_url = parseURL2(a->url, current);
            char* a_url = parsedURL2Str(&p_url)->ptr;
            if (DefaultURLString == DEFAULT_URL_LINK)
                url = url_decode2(a_url, Currentbuf)->ptr;
            else
                pushHist(hist, a_url);
        }
        url = inputLineHist(prompt, url, IN_URL, hist).ptr;
        if (url != NULL)
            SKIP_BLANKS(url);
    }

    if (relative) {
        const int* no_referer_ptr = query_SCONF_NO_REFERER_FROM(&Currentbuf->currentURL);
        current = baseURL(Currentbuf);
        if ((no_referer_ptr && *no_referer_ptr) || current == NULL || current->scheme == SCM_LOCAL || current->scheme == SCM_LOCAL_CGI || current->scheme == SCM_DATA)
            referer = NO_REFERER;
        else
            referer = parsedURL2RefererStr(&Currentbuf->currentURL)->ptr;
        if (url)
            url = url_encode(url, current, Currentbuf->document_charset);
    } else {
        current = NULL;
        referer = NULL;
        if (url)
            url = url_encode(url, NULL, 0);
    }

    if (url == NULL || *url == '\0') {
        displayBuffer(Currentbuf, B_FORCE_REDRAW);
        return;
    }
    if (*url == '#') {
        gotoLabel(url + 1);
        return;
    }

    struct Url p_url = parseURL2(url, current);
    pushHashHist(URLHist, parsedURL2Str(&p_url)->ptr);
    cmd_loadURL(url, current, referer, NULL);
    if (Currentbuf != cur_buf) /* success */
        pushHashHist(URLHist, parsedURL2Str(&Currentbuf->currentURL)->ptr);
}

DEFUN(goURL, GOTO, "Open specified document in a new buffer")
{
    const char* url = searchKeyData();
    if (!url) {
        url = parsedURL2Str(&Currentbuf->currentURL)->ptr;
    }
    goURL0("Goto URL: ", false, url, true);
}

DEFUN(goHome, GOTO_HOME, "Open home page in a new buffer")
{
    char* url;

    if (!(non_null(url = getenv("HTTP_HOME")))
        && !(non_null(url = getenv("WWW_HOME"))))
        return;

    struct Url p_url;
    Buffer* cur_buf = Currentbuf;
    SKIP_BLANKS(url);
    url = url_encode(url, NULL, 0);
    p_url = parseURL2(url, NULL);
    pushHashHist(URLHist, parsedURL2Str(&p_url)->ptr);
    cmd_loadURL(url, NULL, NULL, NULL);
    if (Currentbuf != cur_buf) /* success */
        pushHashHist(URLHist, parsedURL2Str(&Currentbuf->currentURL)->ptr);
}

DEFUN(gorURL, GOTO_RELATIVE, "Go to relative address")
{
    const char* url = searchKeyData();
    goURL0("Goto relative URL: ", true, url, false);
}

/* load bookmark */
DEFUN(ldBmark, BOOKMARK VIEW_BOOKMARK, "View bookmarks")
{
    cmd_loadURL(BookmarkFile, NULL, NO_REFERER, NULL);
}

/* Add current to bookmark */
DEFUN(adBmark, ADD_BOOKMARK, "Add current page to bookmarks")
{
    pStr tmp;
    FormList* request;

    pStr os = Strnew();
    struct Writer w = makeWriter(os);
    wc_Str_conv_strict(&WcOption, &w,
        (const uint8_t*)Currentbuf->buffername,
        (const uint8_t*)Currentbuf->buffername + strlen(Currentbuf->buffername),
        InnerCharset,
        BookmarkCharset);

    tmp = Sprintf("mode=panel&cookie=%s&bmark=%s&url=%s&title=%s"
                  "&charset=%s",
        (Str_form_quote(localCookie()))->ptr,
        (Str_form_quote(Strnew_charp(BookmarkFile)))->ptr,
        (Str_form_quote(parsedURL2Str(&Currentbuf->currentURL)))->ptr,
        (Str_form_quote(os))->ptr,
        wc_ces_to_charset(BookmarkCharset));
    request = newFormList(NULL, "post", NULL, NULL, NULL, NULL, NULL);
    request->body = tmp->ptr;
    request->length = tmp->len;
    cmd_loadURL("file:///$LIB/" W3MBOOKMARK_CMDNAME, NULL, NO_REFERER,
        request);
}

int _strSession(char* sf)
{
    Buffer* buf;
    FILE* f;
    struct Url* url;
    char* sep;
    struct stat st;

    if (!sf)
        sf = session_file ? session_file : rcFile(SESSION_FILE)->ptr;

    while (stat(sf, &st) == 0) {
        if (confirm(_("Session file exists. Overwrite?")))
            break;
        sf = inputFilenameHist(_("Session file (empty: Don't store)? "), sf,
            LoadHist)
                 .ptr;
        if (!*sf)
            return 0;
    }
    if (!(f = fopen(sf, "w")))
        goto fail;

    sep = "";
    for (TabBuffer* tab = FirstTab; tab; tab = tab->nextTab) {
        fputs(sep, f);
        for (buf = tab->firstBuffer; buf; buf = buf->nextBuffer) {
            if (buf->bufferprop & BP_INTERNAL)
                continue;
            if ((url = baseURL(buf)))
                fprintf(f, "%s\n", parsedURL2Str(url)->ptr);
            else
                disp_err_message(buf->buffername, false);
        }
        sep = "\n";
    }

    if ((fclose(f)) == EOF)
        goto fail;
    return 0;

fail:
    disp_err_message(strerror(errno), false);
    return -1;
}

/* Store session */
DEFUN(strSession, STORE, "Store session")
{
    char *def, *sf;

    save_cookies();
    saveUrlHistory();

    def = session_file ? session_file : rcFile(SESSION_FILE)->ptr;
    if (!(sf = inputFilenameHist(Strnew_m_charp("Session file [",
                                     def,
                                     "]? ", NULL)
                                     ->ptr,
              NULL, LoadHist)
                .ptr))
        return;
    if (!*sf)
        sf = def;
    if (_strSession(sf))
        disp_err_message("Unable to store session", false);
}

/* option setting */
DEFUN(ldOpt, OPTIONS, "Display options setting panel")
{
    cmd_loadBuffer(load_option_panel(), BP_NO_URL, LB_NOLINK);
}

/* set an option */
DEFUN(setOpt, SET_OPTION, "Set option")
{
    const char* opt = searchKeyData();
    if (opt == NULL || *opt == '\0' || strchr(opt, '=') == NULL) {
        if (opt != NULL && *opt != '\0') {
            const char* v = get_param_option(opt);
            opt = Sprintf("%s=%s", opt, v ? v : "")->ptr;
        }
        opt = inputStrHist("Set option: ", opt, TextHist).ptr;
        if (opt == NULL || *opt == '\0') {
            displayBuffer(Currentbuf, B_NORMAL);
            return;
        }
    }
    if (set_param_option(opt))
        sync_with_option();
    displayBuffer(Currentbuf, B_REDRAW_IMAGE);
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
        displayBuffer(Currentbuf, B_NORMAL);
        return;
    }
    if ((buf = Currentbuf->linkBuffer[LB_INFO]) != NULL)
        delBuffer(buf);
    buf = page_info_panel(Currentbuf);
    cmd_loadBuffer(buf, BP_NORMAL, LB_INFO);
}

void follow_map(struct parsed_tagarg* arg)
{
    char* name = tag_get_value(arg, "link");
    Anchor* an;
    struct MapArea* a;
    int x, y;
    struct Url p_url;

    an = retrieveCurrentImg(Currentbuf);
    x = Currentbuf->cursorX + Currentbuf->rootX;
    y = Currentbuf->cursorY + Currentbuf->rootY;
    a = follow_map_menu(Currentbuf, name, an, x, y);
    if (a == NULL || a->url == NULL || *(a->url) == '\0') {

#if defined(USE_MENU) || defined(USE_IMAGE)
        return;
    }
    if (*(a->url) == '#') {
        gotoLabel(a->url + 1);
        return;
    }
    p_url = parseURL2(a->url, baseURL(Currentbuf));
    pushHashHist(URLHist, parsedURL2Str(&p_url)->ptr);
    if (check_target && open_tab_blank && a->target && (!strcasecmp(a->target, "_new") || !strcasecmp(a->target, "_blank"))) {
        Buffer* buf;

        _newT();
        buf = Currentbuf;
        cmd_loadURL(a->url, baseURL(Currentbuf),
            parsedURL2Str(&Currentbuf->currentURL)->ptr, NULL);
        if (buf != Currentbuf)
            delBuffer(buf);
        else
            deleteTab(CurrentTab);
        displayBuffer(Currentbuf, B_FORCE_REDRAW);
        return;
    }
    cmd_loadURL(a->url, baseURL(Currentbuf),
        parsedURL2Str(&Currentbuf->currentURL)->ptr, NULL);
#endif
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
    p_url = parseURL2(l->url, baseURL(Currentbuf));
    pushHashHist(URLHist, parsedURL2Str(&p_url)->ptr);
    cmd_loadURL(l->url, baseURL(Currentbuf),
        parsedURL2Str(&Currentbuf->currentURL)->ptr, NULL);
}

static void
anchorMn(Anchor* (*menu_func)(Buffer*), int go)
{
    Anchor* a;
    BufferPoint* po;

    if (!Currentbuf->hrefList || !Currentbuf->hmarklist)
        return;
    a = menu_func(Currentbuf);
    if (!a || a->hseq < 0)
        return;
    po = &Currentbuf->hmarklist->marks[a->hseq];
    gotoLine(Currentbuf, po->line);
    Currentbuf->pos = po->pos;
    arrangeCursor(Currentbuf);
    displayBuffer(Currentbuf, B_NORMAL);
    if (go)
        followA();
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
    Buffer* buf;

    buf = link_list_panel(Currentbuf);
    if (buf != NULL) {
        buf->document_charset = Currentbuf->document_charset;
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

/* Save history */
DEFUN(svHist, SAVE_HISTORY, "Save browsing history")
{
    saveUrlHistory();
    disp_message_nsec("History written", true, 1, false, true);
}

/* download HREF link */
DEFUN(svA, SAVE_LINK, "Save hyperlink target")
{
    do_download = true;
    followA();
    do_download = false;
}

/* download IMG link */
DEFUN(svI, SAVE_IMAGE, "Save inline image")
{
    do_download = true;
    followI();
    do_download = false;
}

/* save buffer */
DEFUN(svBuf, PRINT SAVE_SCREEN, "Save rendered document")
{
    char* qfile = NULL;
    FILE* f;
    int is_pipe;

    const char* file = searchKeyData();
    if (file == NULL || *file == '\0') {
        qfile = inputLineHist(_("Save buffer to: "), NULL, IN_COMMAND, SaveHist).ptr;
        if (qfile == NULL || *qfile == '\0') {
            displayBuffer(Currentbuf, B_NORMAL);
            return;
        }
    }
    pStr os = Strnew();
    struct Writer w = makeWriter(os);
    conv_to_system(&WcOption, &w, qfile ? qfile : file);
    file = os->ptr;
    if (*file == '|') {
        is_pipe = true;
        f = popen(file + 1, "w");
    } else {
        if (qfile) {
            file = unescape_spaces(Strnew_charp(qfile))->ptr;
            pStr os = Strnew();
            struct Writer w = makeWriter(os);
            conv_to_system(&WcOption, &w, file);
            file = os->ptr;
        }
        file = expandPath(file)->ptr;
        if (!canOverWrite(file)) {
            displayBuffer(Currentbuf, B_NORMAL);
            return;
        }
        f = fopen(file, "w");
        is_pipe = false;
    }
    if (f == NULL) {
        pStr os = Strnew();
        struct Writer w = makeWriter(os);
        conv_from_system(&WcOption, &w, file);
        char* emsg = Sprintf(_("Can't open %s"), os->ptr)->ptr;
        disp_err_message(emsg, true);
        return;
    }
    saveBuffer(Currentbuf, f, true);
    if (is_pipe)
        pclose(f);
    else
        fclose(f);
    displayBuffer(Currentbuf, B_NORMAL);
}

/* save source */
DEFUN(svSrc, DOWNLOAD SAVE, "Save document source")
{
    if (Currentbuf->sourcefile == NULL)
        return;
    PermitSaveToPipe = true;

    char* file;
    if (Currentbuf->real_scheme == SCM_LOCAL) {
        pStr os = Strnew();
        struct Writer w = makeWriter(os);
        conv_from_system(&WcOption, &w, guess_save_name(NULL, Currentbuf->currentURL.real_file)->ptr);
        file = os->ptr;
    } else
        file = guess_save_name(Currentbuf, Currentbuf->currentURL.file)->ptr;

    pStr fn;
    if (param_dl_dir) {
        fn = expandPath(param_dl_dir);
        if (Strlastchar(fn) != '/')
            Strcat_char(fn, '/');
        Strcat_charp(fn, file);
        file = fn->ptr;
    }

    doFileCopy(Currentbuf->sourcefile, file);
    PermitSaveToPipe = false;
    displayBuffer(Currentbuf, B_NORMAL);
}

static void
_peekURL(int only_img)
{

    Anchor* a;
    struct Url pu;
    static pStr s = NULL;
    static Lineprop* p = NULL;
    Lineprop* pp;
    static int offset = 0, n;

    if (Currentbuf->firstLine == NULL)
        return;
    if (CurrentKey == prev_key && s != NULL) {
        if (s->len - offset >= COLS)
            offset++;
        else if (s->len <= offset) /* bug ? */
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
            s = Strnew_charp(form2str(a->formitem));
    }
    if (s == NULL) {
        pu = parseURL2(a->url, baseURL(Currentbuf));
        s = parsedURL2Str(&pu);
    }
    if (DecodeURL)
        s = Strnew_charp(url_decode2(s->ptr, Currentbuf)->ptr);
    s = checkType(s, &pp, NULL);
    p = NewAtom_N(Lineprop, s->len);
    memmove(p, pp, s->len * sizeof(Lineprop));
disp:
    n = searchKeyNum();
    if (n > 1 && s->len > (n - 1) * (COLS - 1))
        offset = (n - 1) * (COLS - 1);
    while (offset < s->len && p[offset] & PC_WCHAR2)
        offset++;
    disp_message(&s->ptr[offset], true);
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
static pStr
currentURL(void)
{
    if (Currentbuf->bufferprop & BP_INTERNAL)
        return Strnew_size(0);
    return parsedURL2Str(&Currentbuf->currentURL);
}

DEFUN(curURL, PEEK, "Show current address")
{
    static pStr s = NULL;
    static Lineprop* p = NULL;
    Lineprop* pp;
    static int offset = 0, n;

    if (Currentbuf->bufferprop & BP_INTERNAL)
        return;
    if (CurrentKey == prev_key && s != NULL) {
        if (s->len - offset >= COLS)
            offset++;
        else if (s->len <= offset) /* bug ? */
            offset = 0;
    } else {
        offset = 0;
        s = currentURL();
        if (DecodeURL)
            s = Strnew_charp(url_decode2(s->ptr, NULL)->ptr);
        s = checkType(s, &pp, NULL);
        p = NewAtom_N(Lineprop, s->len);
        memmove(p, pp, s->len * sizeof(Lineprop));
    }
    n = searchKeyNum();
    if (n > 1 && s->len > (n - 1) * (COLS - 1))
        offset = (n - 1) * (COLS - 1);
    while (offset < s->len && p[offset] & PC_WCHAR2)
        offset++;
    disp_message(&s->ptr[offset], true);
}
/* view HTML source */

DEFUN(vwSrc, SOURCE VIEW, "Toggle between HTML shown or processed")
{
    Buffer* buf;

    if (Currentbuf->type == NULL || Currentbuf->bufferprop & BP_FRAME)
        return;
    if ((buf = Currentbuf->linkBuffer[LB_SOURCE]) != NULL || (buf = Currentbuf->linkBuffer[LB_N_SOURCE]) != NULL) {
        Currentbuf = buf;
        displayBuffer(Currentbuf, B_NORMAL);
        return;
    }
    if (Currentbuf->sourcefile == NULL) {
        if (Currentbuf->pagerSource && !strcasecmp(Currentbuf->type, "text/plain")) {
            wc_ces old_charset;
            bool old_fix_width_conv;
            FILE* f;
            pStr tmpf = tmpfname(CurrentPid, TMPF_SRC, NULL);
            f = fopen(tmpf->ptr, "w");
            if (f == NULL)
                return;
            old_charset = DisplayCharset;
            old_fix_width_conv = WcOption.fix_width_conv;
            DisplayCharset = (Currentbuf->document_charset != WC_CES_US_ASCII)
                ? Currentbuf->document_charset
                : 0;
            WcOption.fix_width_conv = false;
            saveBufferBody(Currentbuf, f, true);
            DisplayCharset = old_charset;
            WcOption.fix_width_conv = old_fix_width_conv;
            fclose(f);
            Currentbuf->sourcefile = tmpf->ptr;
        } else {
            return;
        }
    }

    buf = newBuffer(INIT_BUFFER_WIDTH);

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
    buf->header_source = Currentbuf->header_source;
    buf->search_header = Currentbuf->search_header;
    buf->document_charset = Currentbuf->document_charset;
    buf->clone = Currentbuf->clone;
    (*buf->clone)++;

    reshapeBuffer(buf);
    pushBuffer(buf);
    displayBuffer(Currentbuf, B_NORMAL);
}

DEFUN(foldPre, FOLD_PRE, "Fold long lines in <pre> elements")
{
    fold_pre = 1;
    Currentbuf->need_reshape = true;
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
    fold_pre = FoldPre;
}

/* reload */
DEFUN(reload, RELOAD, "Load current document anew")
{
    Buffer *buf, *fbuf = NULL, sbuf;
    wc_ces old_charset;
    pStr url;
    FormList* request;
    int multipart;

    if (Currentbuf->bufferprop & BP_INTERNAL) {
        if (!strcmp(Currentbuf->buffername, DOWNLOAD_LIST_TITLE)) {
            ldDL();
            return;
        }
        disp_err_message(_("Can't reload..."), true);
        return;
    }
    if (Currentbuf->currentURL.scheme == SCM_LOCAL && !strcmp(Currentbuf->currentURL.file, "-")) {
        /* file is std input */
        disp_err_message(_("Can't reload stdin"), true);
        return;
    }
    fold_pre = FoldPre;
    copyBuffer(&sbuf, Currentbuf);
    if (Currentbuf->bufferprop & BP_FRAME && (fbuf = Currentbuf->linkBuffer[LB_N_FRAME])) {
        if (fmInitialized) {
            message("Rendering frame", 0, 0);
            refresh(tty_file());
        }
        if (!(buf = renderFrame(fbuf, 1))) {
            displayBuffer(Currentbuf, B_NORMAL);
            return;
        }
        if (fbuf->linkBuffer[LB_FRAME]) {
            if (buf->sourcefile && fbuf->linkBuffer[LB_FRAME]->sourcefile && !strcmp(buf->sourcefile, fbuf->linkBuffer[LB_FRAME]->sourcefile))
                fbuf->linkBuffer[LB_FRAME]->sourcefile = NULL;
            delBuffer(fbuf->linkBuffer[LB_FRAME]);
        }
        fbuf->linkBuffer[LB_FRAME] = buf;
        buf->linkBuffer[LB_N_FRAME] = fbuf;
        pushBuffer(buf);
        Currentbuf = buf;
        if (Currentbuf->firstLine) {
            COPY_BUFROOT(Currentbuf, &sbuf);
            restorePosition(Currentbuf, &sbuf);
        }
        displayBuffer(Currentbuf, B_FORCE_REDRAW);
        return;
    } else if (Currentbuf->frameset != NULL)
        fbuf = Currentbuf->linkBuffer[LB_FRAME];
    multipart = 0;
    if (Currentbuf->form_submit) {
        request = Currentbuf->form_submit->parent;
        if (request->method == FORM_METHOD_POST
            && request->enctype == FORM_ENCTYPE_MULTIPART) {
            pStr query;
            struct stat st;
            multipart = 1;
            query_from_followform(&query, Currentbuf->form_submit, multipart);
            stat(request->body, &st);
            request->length = st.st_size;
        }
    } else {
        request = NULL;
    }
    url = parsedURL2Str(&Currentbuf->currentURL);
    message(_("Reloading..."), 0, 0);
    refresh(tty_file());
    old_charset = DocumentCharset;
    if (Currentbuf->document_charset != WC_CES_US_ASCII)
        DocumentCharset = Currentbuf->document_charset;
    SearchHeader = Currentbuf->search_header;
    DefaultType = Currentbuf->real_type;
    buf = loadGeneralFile(url->ptr, NULL, NO_REFERER, RG_NOCACHE, request);
    DocumentCharset = old_charset;
    SearchHeader = false;
    DefaultType = NULL;

    if (multipart)
        unlink(request->body);
    if (buf == NULL) {
        disp_err_message(_("Can't reload..."), true);
        return;
    } else if (buf == NO_BUFFER) {
        displayBuffer(Currentbuf, B_NORMAL);
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
    Currentbuf->search_header = sbuf.search_header;
    Currentbuf->form_submit = sbuf.form_submit;
    if (Currentbuf->firstLine) {
        COPY_BUFROOT(Currentbuf, &sbuf);
        restorePosition(Currentbuf, &sbuf);
    }
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
    fold_pre = FoldPre;
}

/* reshape */
DEFUN(reshape, RESHAPE, "Re-render document")
{
    reshapeBuffer(Currentbuf);
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

static void
_docCSet(wc_ces charset)
{
    if (Currentbuf->bufferprop & BP_INTERNAL)
        return;
    if (Currentbuf->sourcefile == NULL) {
        disp_message("Can't reload...", false);
        return;
    }
    Currentbuf->document_charset = charset;
    Currentbuf->need_reshape = true;
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

void change_charset(struct parsed_tagarg* arg)
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
    const char* cs = searchKeyData();
    if (cs == NULL || *cs == '\0')
        cs = inputStr(_("Document charset: "),
            wc_ces_to_charset(Currentbuf->document_charset))
                 .ptr;
    wc_ces charset = wc_guess_charset_short(cs, 0);
    if (charset == 0) {
        displayBuffer(Currentbuf, B_NORMAL);
        return;
    }
    _docCSet(charset);
}

DEFUN(defCSet, DEFAULT_CHARSET, "Change the default character encoding")
{
    const char* cs = searchKeyData();
    if (cs == NULL || *cs == '\0')
        cs = inputStr(_("Default document charset: "),
            wc_ces_to_charset(DocumentCharset))
                 .ptr;
    wc_ces charset = wc_guess_charset_short(cs, 0);
    if (charset != 0)
        DocumentCharset = charset;
    displayBuffer(Currentbuf, B_NORMAL);
}

DEFUN(chkURL, MARK_URL, "Turn URL-like strings into hyperlinks")
{
    chkURLBuffer(Currentbuf);
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

DEFUN(chkWORD, MARK_WORD, "Turn current word into hyperlink")
{
    char* p;
    int spos, epos;
    p = getCurWord(Currentbuf, &spos, &epos);
    if (p == NULL)
        return;
    reAnchorWord(Currentbuf, Currentbuf->currentLine, spos, epos);
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

DEFUN(chkNMID, MARK_MID, "Turn Message-ID-like strings into hyperlinks")
{
    chkNMIDBuffer(Currentbuf);
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

/* render frames */
DEFUN(rFrame, FRAME, "Toggle rendering HTML frames")
{
    Buffer* buf;

    if ((buf = Currentbuf->linkBuffer[LB_FRAME]) != NULL) {
        Currentbuf = buf;
        displayBuffer(Currentbuf, B_NORMAL);
        return;
    }
    if (Currentbuf->frameset == NULL) {
        if ((buf = Currentbuf->linkBuffer[LB_N_FRAME]) != NULL) {
            Currentbuf = buf;
            displayBuffer(Currentbuf, B_NORMAL);
        }
        return;
    }
    if (fmInitialized) {
        message("Rendering frame", 0, 0);
        refresh(tty_file());
    }
    buf = renderFrame(Currentbuf, 0);
    if (buf == NULL) {
        displayBuffer(Currentbuf, B_NORMAL);
        return;
    }
    buf->linkBuffer[LB_N_FRAME] = Currentbuf;
    Currentbuf->linkBuffer[LB_FRAME] = buf;
    pushBuffer(buf);
    if (fmInitialized && display_ok)
        displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

/* spawn external browser */
static void
invoke_browser(const char* url)
{
    pStr cmd;
    const char* browser = NULL;
    int bg = 0, len;

    browser = searchKeyData();
    if (browser == NULL || *browser == '\0') {
        switch (prec_num) {
        case 0:
        case 1:
            browser = ExtBrowser;
            break;
        case 2:
            browser = ExtBrowser2;
            break;
        case 3:
            browser = ExtBrowser3;
            break;
        case 4:
            browser = ExtBrowser4;
            break;
        case 5:
            browser = ExtBrowser5;
            break;
        case 6:
            browser = ExtBrowser6;
            break;
        case 7:
            browser = ExtBrowser7;
            break;
        case 8:
            browser = ExtBrowser8;
            break;
        case 9:
            browser = ExtBrowser9;
            break;
        }
        if (browser == NULL || *browser == '\0') {
            browser = inputStr("Browse command: ", NULL).ptr;
            if (browser != NULL) {
                pStr os = Strnew();
                struct Writer w = makeWriter(os);
                conv_to_system(&WcOption, &w, browser);
                browser = os->ptr;
            }
        }
    } else {
        pStr os = Strnew();
        struct Writer w = makeWriter(os);
        conv_to_system(&WcOption, &w, browser);
        browser = os->ptr;
    }
    if (browser == NULL || *browser == '\0') {
        displayBuffer(Currentbuf, B_NORMAL);
        return;
    }

    if ((len = strlen(browser)) >= 2 && browser[len - 1] == '&' && browser[len - 2] != '\\') {
        browser = allocStr_n(browser, len - 2).ptr;
        bg = 1;
    }
    cmd = myExtCommand(browser, shell_quote(url)->ptr, false);
    Strremovetrailingspaces(cmd);
    fmTerm();
    mySystem(cmd->ptr, bg);
    fmInit();
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

DEFUN(extbrz, EXTERN, "Display using an external browser")
{
    if (Currentbuf->bufferprop & BP_INTERNAL) {
        disp_err_message(_("Can't browse..."), true);
        return;
    }
    if (Currentbuf->currentURL.scheme == SCM_LOCAL && !strcmp(Currentbuf->currentURL.file, "-")) {
        /* file is std input */
        disp_err_message(_("Can't browse stdin"), true);
        return;
    }
    invoke_browser(parsedURL2Str(&Currentbuf->currentURL)->ptr);
}

DEFUN(linkbrz, EXTERN_LINK, "Display target using an external browser")
{
    Anchor* a;
    struct Url pu;

    if (Currentbuf->firstLine == NULL)
        return;
    a = retrieveCurrentAnchor(Currentbuf);
    if (a == NULL)
        return;
    pu = parseURL2(a->url, baseURL(Currentbuf));
    invoke_browser(parsedURL2Str(&pu)->ptr);
}

/* show current line number and number of lines in the entire document */
DEFUN(curlno, LINE_INFO, "Display current position in document")
{
    Line* l = Currentbuf->currentLine;
    pStr tmp;
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
    if (Currentbuf->pagerSource && !(Currentbuf->bufferprop & BP_CLOSE))
        tmp = Sprintf("line %d col %d/%d", cur, col, len);
    else
        tmp = Sprintf("line %d/%d (%d%%) col %d/%d", cur, all,
            (int)((double)cur * 100.0 / (double)(all ? all : 1)
                + 0.5),
            col, len);
    Strcat_charp(tmp, "  ");
    Strcat_charp(tmp, wc_ces_to_charset_desc(Currentbuf->document_charset));

    disp_message(tmp->ptr, false);
}

DEFUN(dispI, DISPLAY_IMAGE, "Restart loading and drawing of images")
{
    if (!displayImage)
        initImage();
    if (!activeImage)
        return;
    displayImage = true;
    Currentbuf->image_flag = IMG_FLAG_AUTO;
    Currentbuf->need_reshape = true;
    displayBuffer(Currentbuf, B_REDRAW_IMAGE);
}

DEFUN(stopI, STOP_IMAGE, "Stop loading and drawing of images")
{
    if (!activeImage)
        return;
    Currentbuf->image_flag = IMG_FLAG_SKIP;
    displayBuffer(Currentbuf, B_REDRAW_IMAGE);
}

DEFUN(dispVer, VERSION, "Display the version of w3m")
{
    disp_message(Sprintf("w3m version %s", W3M_VERSION)->ptr, true);
}

DEFUN(wrapToggle, WRAP_TOGGLE, "Toggle wrapping mode in searches")
{
    if (WrapSearch) {
        WrapSearch = false;
        disp_message(_("Wrap search off"), true);
    } else {
        WrapSearch = true;
        disp_message(_("Wrap search on"), true);
    }
}

static void execdict(const char* word)
{
    if (!UseDictCommand || word == NULL || *word == '\0') {
        displayBuffer(Currentbuf, B_NORMAL);
        return;
    }

    pStr os = Strnew();
    struct Writer ww = makeWriter(os);
    conv_to_system(&WcOption, &ww, word);
    const char* w = os->ptr;
    if (*w == '\0') {
        displayBuffer(Currentbuf, B_NORMAL);
        return;
    }

    const char* dictcmd = Sprintf("%s?%s", DictCommand,
        Str_form_quote(Strnew_charp(w))->ptr)
                              ->ptr;
    Buffer* buf = loadGeneralFile(dictcmd, NULL, NO_REFERER, 0, NULL);
    if (buf == NULL) {
        disp_message("Execution failed", true);
        return;
    } else if (buf != NO_BUFFER) {
        buf->filename = w;
        buf->buffername = Sprintf("%s %s", DICTBUFFERNAME, word)->ptr;
        if (buf->type == NULL)
            buf->type = "text/plain";
        pushBuffer(buf);
    }
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

DEFUN(dictword, DICT_WORD, "Execute dictionary command (see README.dict)")
{
    execdict(inputStrHist(DictPrompt, "", DictHist).ptr);
}

DEFUN(dictwordat, DICT_WORD_AT,
    "Execute dictionary command for word at cursor")
{
    execdict(GetWord(Currentbuf).ptr);
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
        const char* s = GetWord(buf).ptr;
        set_environ("W3M_CURRENT_WORD", s ? s : "");
        Anchor* a = retrieveCurrentAnchor(buf);
        struct Url pu;
        if (a) {
            pu = parseURL2(a->url, baseURL(buf));
            set_environ("W3M_CURRENT_LINK", parsedURL2Str(&pu)->ptr);
        } else
            set_environ("W3M_CURRENT_LINK", "");
        a = retrieveCurrentImg(buf);
        if (a) {
            pu = parseURL2(a->url, baseURL(buf));
            set_environ("W3M_CURRENT_IMG", parsedURL2Str(&pu)->ptr);
        } else
            set_environ("W3M_CURRENT_IMG", "");
        a = retrieveCurrentForm(buf);
        if (a)
            set_environ("W3M_CURRENT_FORM", form2str(a->formitem));
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

const char* searchKeyData(void)
{
    const char* data = NULL;

    if (CurrentCmdData != NULL && *CurrentCmdData != '\0')
        data = CurrentCmdData;
    else if (CurrentKey >= 0)
        data = getKeyData(CurrentKey);
    CurrentCmdData = NULL;
    if (data == NULL || *data == '\0')
        return NULL;
    return allocStr(data).ptr;
}

static int
searchKeyNum(void)
{
    int n = 1;
    const char* d = searchKeyData();
    if (d != NULL)
        n = atoi(d);
    return n * PREC_NUM;
}

static void deleteFiles(void)
{
    Buffer* buf;

    for (CurrentTab = FirstTab; CurrentTab; CurrentTab = CurrentTab->nextTab) {
        while (Firstbuf && Firstbuf != NO_BUFFER) {
            buf = Firstbuf->nextBuffer;
            discardBuffer(Firstbuf);
            Firstbuf = buf;
        }
    }

    const char* f;
    while ((f = popFileToDelete()) != NULL) {
        unlink(f);
        if (enable_inline_image == INLINE_IMG_SIXEL && strcmp(f + strlen(f) - 4, ".gif") == 0) {
            pStr firstframe = Strnew_charp(f);
            Strcat_charp(firstframe, "-1");
            unlink(firstframe->ptr);
        }
    }

    rmdir(Sprintf("%s/%d", tmp_dir, CurrentPid)->ptr);
    rmdir(Sprintf("%s/%d", rc_dir, CurrentPid)->ptr);
}

DEFUN(execCmd, COMMAND, "Invoke w3m function(s)")
{
    const char* data = searchKeyData();
    if (data == NULL || *data == '\0') {
        data = inputStrHist("command [; ...]: ", "", TextHist).ptr;
        if (data == NULL) {
            displayBuffer(Currentbuf, B_NORMAL);
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
        const char* p = getWord(&data).ptr;
        int cmd = getFuncList(p);
        if (cmd < 0) {
            pStr e = Sprintf("Unknown command: %s", p);
            disp_err_message(e->ptr, false);
            break;
        }
        p = getQWord(&data).ptr;
        CurrentKey = -1;
        CurrentCmdData = *p ? p : NULL;
        w3mFuncList[cmd].func();
        CurrentCmdData = NULL;
    }
    displayBuffer(Currentbuf, B_NORMAL);
}

static void
SigAlarm(SIGNAL_ARG)
{
    if (CurrentAlarm->sec > 0) {
        CurrentKey = -1;
        CurrentCmdData = (const char*)CurrentAlarm->data;
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
            setAlarmEventOrDefaultAlarm(0);
        if (CurrentAlarm->sec > 0) {
            mySignal(SIGALRM, SigAlarm);
            alarm(CurrentAlarm->sec);
        }
    }
}

DEFUN(setAlarm, ALARM, "Set alarm")
{
    const char* data = searchKeyData();
    if (data == NULL || *data == '\0') {
        data = inputStrHist("(Alarm)sec command: ", "", TextHist).ptr;
        if (data == NULL) {
            displayBuffer(Currentbuf, B_NORMAL);
            return;
        }
    }

    int sec = 0;
    int cmd = -1;
    if (*data != '\0') {
        sec = atoi(getWord(&data).ptr);
        if (sec > 0)
            cmd = getFuncList(getWord(&data).ptr);
    }
    if (cmd >= 0) {
        data = getQWord(&data).ptr;
        setAlarmEvent(getDefaultAlarm(), sec, AL_EXPLICIT, cmd, data);
        disp_message_nsec(Sprintf("%dsec %s %s", sec, w3mFuncList[cmd].id,
                              data)
                              ->ptr,
            false, 1, false, true);
    } else {
        setAlarmEvent(getDefaultAlarm(), 0, AL_UNSET, FUNCNAME_nulcmd, NULL);
    }
    displayBuffer(Currentbuf, B_NORMAL);
}

DEFUN(reinit, REINIT, "Reload configuration file")
{
    const char* resource = searchKeyData();

    if (resource == NULL) {
        init_rc();
        sync_with_option();
        initCookie();
        displayBuffer(Currentbuf, B_REDRAW_IMAGE);
        return;
    }

    if (!strcasecmp(resource, "CONFIG") || !strcasecmp(resource, "RC")) {
        init_rc();
        sync_with_option();
        displayBuffer(Currentbuf, B_REDRAW_IMAGE);
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

    disp_err_message(Sprintf("Don't know how to reinitialize '%s'", resource)->ptr, false);
}

DEFUN(defKey, DEFINE_KEY, "Define a binding between a key stroke combination and a command")
{
    const char* data;

    data = searchKeyData();
    if (data == NULL || *data == '\0') {
        data = inputStrHist("Key definition: ", "", TextHist).ptr;
        if (data == NULL || *data == '\0') {
            displayBuffer(Currentbuf, B_NORMAL);
            return;
        }
    }
    setKeymap(allocStr(data).ptr, -1, true);
    displayBuffer(Currentbuf, B_NORMAL);
}

TabBuffer*
newTab(void)
{
    TabBuffer* n;

    n = New(TabBuffer);
    if (n == NULL)
        return NULL;
    n->nextTab = NULL;
    n->currentBuffer = NULL;
    n->firstBuffer = NULL;
    return n;
}

static void
_newT(void)
{
    TabBuffer* tag;
    Buffer* buf;
    int i;

    tag = newTab();
    if (!tag)
        return;

    buf = newBuffer(Currentbuf->width);
    copyBuffer(buf, Currentbuf);
    buf->nextBuffer = NULL;
    for (i = 0; i < MAX_LB; i++)
        buf->linkBuffer[i] = NULL;
    (*buf->clone)++;
    tag->firstBuffer = tag->currentBuffer = buf;

    tag->nextTab = CurrentTab->nextTab;
    tag->prevTab = CurrentTab;
    if (CurrentTab->nextTab)
        CurrentTab->nextTab->prevTab = tag;
    else
        LastTab = tag;
    CurrentTab->nextTab = tag;
    CurrentTab = tag;
    nTab++;
}

DEFUN(newT, NEW_TAB, "Open a new tab (with current document)")
{
    _newT();
    displayBuffer(Currentbuf, B_REDRAW_IMAGE);
}

static TabBuffer*
numTab(int n)
{
    TabBuffer* tab;
    int i;

    if (n == 0)
        return CurrentTab;
    if (n == 1)
        return FirstTab;
    if (nTab <= 1)
        return NULL;
    for (tab = FirstTab, i = 1; tab && i < n; tab = tab->nextTab, i++)
        ;
    return tab;
}

TabBuffer*
deleteTab(TabBuffer* tab)
{
    Buffer *buf, *next;

    if (nTab <= 1)
        return FirstTab;
    if (tab->prevTab) {
        if (tab->nextTab)
            tab->nextTab->prevTab = tab->prevTab;
        else
            LastTab = tab->prevTab;
        tab->prevTab->nextTab = tab->nextTab;
        if (tab == CurrentTab)
            CurrentTab = tab->prevTab;
    } else { /* tab == FirstTab */
        tab->nextTab->prevTab = NULL;
        FirstTab = tab->nextTab;
        if (tab == CurrentTab)
            CurrentTab = tab->nextTab;
    }
    nTab--;
    buf = tab->firstBuffer;
    while (buf && buf != NO_BUFFER) {
        next = buf->nextBuffer;
        discardBuffer(buf);
        buf = next;
    }
    return FirstTab;
}

DEFUN(closeT, CLOSE_TAB, "Close tab")
{
    TabBuffer* tab;

    if (nTab <= 1)
        return;
    if (prec_num)
        tab = numTab(PREC_NUM);
    else
        tab = CurrentTab;
    if (tab)
        deleteTab(tab);
    displayBuffer(Currentbuf, B_REDRAW_IMAGE);
}

DEFUN(nextT, NEXT_TAB, "Switch to the next tab")
{
    int i;

    if (nTab <= 1)
        return;
    for (i = 0; i < PREC_NUM; i++) {
        if (CurrentTab->nextTab)
            CurrentTab = CurrentTab->nextTab;
        else
            CurrentTab = FirstTab;
    }
    displayBuffer(Currentbuf, B_REDRAW_IMAGE);
}

DEFUN(prevT, PREV_TAB, "Switch to the previous tab")
{
    int i;

    if (nTab <= 1)
        return;
    for (i = 0; i < PREC_NUM; i++) {
        if (CurrentTab->prevTab)
            CurrentTab = CurrentTab->prevTab;
        else
            CurrentTab = LastTab;
    }
    displayBuffer(Currentbuf, B_REDRAW_IMAGE);
}

static void
followTab(TabBuffer* tab)
{
    Buffer* buf;
    Anchor* a;

    a = retrieveCurrentImg(Currentbuf);
    if (!(a && a->image && a->image->map))
        a = retrieveCurrentAnchor(Currentbuf);
    if (a == NULL)
        return;

    if (tab == CurrentTab) {
        check_target = false;
        followA();
        check_target = true;
        return;
    }
    _newT();
    buf = Currentbuf;
    check_target = false;
    followA();
    check_target = true;
    if (tab == NULL) {
        if (buf != Currentbuf)
            delBuffer(buf);
        else
            deleteTab(CurrentTab);
    } else if (buf != Currentbuf) {
        /* buf <- p <- ... <- Currentbuf = c */
        Buffer *c, *p;

        c = Currentbuf;
        if ((p = prevBuffer(c, buf)))
            p->nextBuffer = NULL;
        Firstbuf = buf;
        deleteTab(CurrentTab);
        CurrentTab = tab;
        for (buf = p; buf; buf = p) {
            p = prevBuffer(c, buf);
            pushBuffer(buf);
        }
    }
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

DEFUN(tabA, TAB_LINK, "Follow current hyperlink in a new tab")
{
    followTab(prec_num ? numTab(PREC_NUM) : NULL);
}

static void
tabURL0(TabBuffer* tab, const char* prompt, int relative)
{

    if (tab == CurrentTab) {
        const char* url = searchKeyData();
        goURL0(prompt, relative, url, false);
        return;
    }
    _newT();

    Buffer* buf = Currentbuf;
    goURL0(prompt, relative, searchKeyData(), false);
    if (tab == NULL) {
        if (buf != Currentbuf)
            delBuffer(buf);
        else
            deleteTab(CurrentTab);
    } else if (buf != Currentbuf) {
        /* buf <- p <- ... <- Currentbuf = c */
        Buffer *c, *p;

        c = Currentbuf;
        if ((p = prevBuffer(c, buf)))
            p->nextBuffer = NULL;
        Firstbuf = buf;
        deleteTab(CurrentTab);
        CurrentTab = tab;
        for (buf = p; buf; buf = p) {
            p = prevBuffer(c, buf);
            pushBuffer(buf);
        }
    }
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

DEFUN(tabURL, TAB_GOTO, "Open specified document in a new tab")
{
    tabURL0(prec_num ? numTab(PREC_NUM) : NULL,
        "Goto URL on new tab: ", false);
}

DEFUN(tabrURL, TAB_GOTO_RELATIVE, "Open relative address in a new tab")
{
    tabURL0(prec_num ? numTab(PREC_NUM) : NULL,
        "Goto relative URL on new tab: ", true);
}

static void
moveTab(TabBuffer* t, TabBuffer* t2, int right)
{
    if (t2 == NO_TABBUFFER)
        t2 = FirstTab;
    if (!t || !t2 || t == t2 || t == NO_TABBUFFER)
        return;
    if (t->prevTab) {
        if (t->nextTab)
            t->nextTab->prevTab = t->prevTab;
        else
            LastTab = t->prevTab;
        t->prevTab->nextTab = t->nextTab;
    } else {
        t->nextTab->prevTab = NULL;
        FirstTab = t->nextTab;
    }
    if (right) {
        t->nextTab = t2->nextTab;
        t->prevTab = t2;
        if (t2->nextTab)
            t2->nextTab->prevTab = t;
        else
            LastTab = t;
        t2->nextTab = t;
    } else {
        t->prevTab = t2->prevTab;
        t->nextTab = t2;
        if (t2->prevTab)
            t2->prevTab->nextTab = t;
        else
            FirstTab = t;
        t2->prevTab = t;
    }
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

DEFUN(tabR, TAB_RIGHT, "Move right along the tab bar")
{
    TabBuffer* tab;
    int i;

    for (tab = CurrentTab, i = 0; tab && i < PREC_NUM;
        tab = tab->nextTab, i++)
        ;
    moveTab(CurrentTab, tab ? tab : LastTab, true);
}

DEFUN(tabL, TAB_LEFT, "Move left along the tab bar")
{
    TabBuffer* tab;
    int i;

    for (tab = CurrentTab, i = 0; tab && i < PREC_NUM;
        tab = tab->prevTab, i++)
        ;
    moveTab(CurrentTab, tab ? tab : FirstTab, false);
}

void addDownloadList(pid_t pid, const char* url, const char* save, const char* lock, size_t size)
{
    DownloadList* d = New(DownloadList);
    d->pid = pid;
    d->url = url;
    if (save[0] != '/' && save[0] != '~')
        save = Strnew_m_charp(CurrentDir, "/", save, NULL)->ptr;
    d->save = expandPath(save)->ptr;
    d->lock = lock;
    d->size = size;
    d->time = time(0);
    d->running = true;
    d->err = 0;
    d->next = NULL;
    d->prev = LastDL;
    if (LastDL)
        LastDL->next = d;
    else
        FirstDL = d;
    LastDL = d;
    add_download_list = true;
}

int checkDownloadList(void)
{
    DownloadList* d;
    struct stat st;

    if (!FirstDL)
        return false;
    for (d = FirstDL; d != NULL; d = d->next) {
        if (d->running && !lstat(d->lock, &st))
            return true;
    }
    return false;
}

static char*
convert_size3(size_t size)
{
    pStr tmp = Strnew();
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
    pStr src = NULL;
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
        pStr os = Strnew();
        struct Writer w = makeWriter(os);
        conv_from_system(&WcOption, &w, d->save);
        Strcat(src, Sprintf("%s\n  --&gt; %s\n  ", html_quote(d->url), html_quote(os->ptr)));
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
            int i, l = COLS - 6;
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

void download_action(struct parsed_tagarg* arg)
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
                if (nTab > 1)
                    deleteTab(CurrentTab);
            } else
                delBuffer(Currentbuf);
            displayBuffer(Currentbuf, B_FORCE_REDRAW);
        }
        return;
    }
    reload = checkDownloadList();
    buf = DownloadListBuffer();
    if (!buf) {
        displayBuffer(Currentbuf, B_NORMAL);
        return;
    }
    buf->bufferprop |= (BP_INTERNAL | BP_NO_URL);
    if (replace) {
        COPY_BUFROOT(buf, Currentbuf);
        restorePosition(buf, Currentbuf);
    }
    if (!replace && open_tab_dl_list) {
        _newT();
        new_tab = true;
    }
    pushBuffer(buf);
    if (replace || new_tab)
        deletePrevBuf();
    if (reload)
        Currentbuf->event = setAlarmEvent(Currentbuf->event, 1, AL_IMPLICIT,
            FUNCNAME_reload, NULL);
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
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
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

DEFUN(undoPos, UNDO, "Cancel the last cursor movement")
{
    BufferPos* b = Currentbuf->undo;
    int i;

    if (!Currentbuf->firstLine)
        return;
    if (!b || !b->prev)
        return;
    for (i = 0; i < PREC_NUM && b->prev; i++, b = b->prev)
        ;
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
    for (i = 0; i < PREC_NUM && b->next; i++, b = b->next)
        ;
    resetPos(b);
}

DEFUN(cursorTop, CURSOR_TOP, "Move cursor to the top of the screen")
{
    if (Currentbuf->firstLine == NULL)
        return;
    Currentbuf->currentLine = lineSkip(Currentbuf, Currentbuf->topLine,
        0, false);
    arrangeLine(Currentbuf);
    displayBuffer(Currentbuf, B_NORMAL);
}

DEFUN(cursorMiddle, CURSOR_MIDDLE, "Move cursor to the middle of the screen")
{
    int offsety;
    if (Currentbuf->firstLine == NULL)
        return;
    offsety = (Currentbuf->lines - 1) / 2;
    Currentbuf->currentLine = currentLineSkip(Currentbuf, Currentbuf->topLine,
        offsety, false);
    arrangeLine(Currentbuf);
    displayBuffer(Currentbuf, B_NORMAL);
}

DEFUN(cursorBottom, CURSOR_BOTTOM, "Move cursor to the bottom of the screen")
{
    if (Currentbuf->firstLine == NULL)
        return;
    int offsety = Currentbuf->lines - 1;
    Currentbuf->currentLine = currentLineSkip(Currentbuf, Currentbuf->topLine,
        offsety, false);
    arrangeLine(Currentbuf);
    displayBuffer(Currentbuf, B_NORMAL);
}

DEFUN(userMessage, MESSAGE, "Display a message")
{
    const char* msg = CurrentCmdData;
    if (msg == NULL || *msg == '\0') {
        displayBuffer(Currentbuf, B_NORMAL);
        return;
    }

    disp_message_nsec(msg, false, MessageDelay, false, true);
}

DEFUN(lineTop, LINE_TOP, "Redraw screen with current line at top")
{
    if (Currentbuf->firstLine == NULL)
        return;
    int offsety = Currentbuf->cursorY;
    Currentbuf->topLine = lineSkip(Currentbuf, Currentbuf->topLine,
        offsety, false);
    arrangeLine(Currentbuf);
    displayBuffer(Currentbuf, B_NORMAL);
}

DEFUN(lineBottom, LINE_BOTTOM, "Redraw screen with current line at bottom")
{
    int offsety;
    if (Currentbuf->firstLine == NULL)
        return;
    /* subtract 1 to exclude current line */
    offsety = (Currentbuf->lines - 1) - Currentbuf->cursorY;
    Currentbuf->topLine = lineSkip(Currentbuf, Currentbuf->topLine,
        -offsety, false);
    arrangeLine(Currentbuf);
    displayBuffer(Currentbuf, B_NORMAL);
}

static void rm_mkd_tmp_dir()
{
    if (mkd_tmp_dir)
        if (rmdir(mkd_tmp_dir) != 0) {
            err_msg = Strcat(err_msg,
                Sprintf("Can't remove temporary directory (%s)!\n",
                    mkd_tmp_dir));
            // i = i ? i : 1;
        }
    if (err_msg)
        fprintf(stderr, "%s", err_msg->ptr);
}

int main(int argc, char** argv)
{
    w3m_init();
    w3m_register_callback(W3M_EVENT_ON_EXIT, fmTerm);
    w3m_register_callback(W3M_EVENT_ON_EXIT, stopDownload);
    w3m_register_callback(W3M_EVENT_ON_EXIT, deleteFiles);
    w3m_register_callback(W3M_EVENT_ON_EXIT, free_ssl_ctx);
    w3m_register_callback(W3M_EVENT_ON_EXIT, disconnectFTP);
    w3m_register_callback(W3M_EVENT_ON_EXIT, disconnectNews);
    w3m_register_callback(W3M_EVENT_ON_EXIT, rm_mkd_tmp_dir);

    Buffer* newbuf = NULL;
    char* p;
    int c, i;
    struct input_stream* redin;
    char* line_str = NULL;
    char** load_argv;
    FormList* request;
    int load_argc = 0;
    int load_bookmark = false;
    int visual_start = false;
    int open_new_tab = false;
    char search_header = false;
    char* default_type = NULL;
    char* post_file = NULL;
    int opt_restore = false;
    char* Locale = NULL;
    uint8_t auto_detect;
#if defined(DONT_CALL_GC_AFTER_FORK) && defined(USE_IMAGE)
    char** getimage_args = NULL;
#endif /* defined(DONT_CALL_GC_AFTER_FORK) && defined(USE_IMAGE) */
    if (!getenv("GC_LARGE_ALLOC_WARN_INTERVAL"))
        set_environ("GC_LARGE_ALLOC_WARN_INTERVAL", "30000");
    GC_INIT();
    GC_set_oom_fn(die_oom);
#if defined(ENABLE_NLS) || defined(USE_M17N)
    setlocale(LC_ALL, "");
#endif

    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);

#if (defined(__MINGW32_VERSION) || defined(__EMX__)) \
    && !defined(SILENCE_DEPRECATION_WARNING)
    deprecated = 1 << 2;
#endif

    initFileToDelete();

    /*
     * An empty URL means to open a new tab. If -N was provided we need
     * to double the size.
     */
    load_argv = New_N(char*, (argc - 1) * (1 + !!open_new_tab));
    load_argc = 0;

    CurrentDir = currentdir();
    CurrentPid = getpid();

#if defined(DONT_CALL_GC_AFTER_FORK) && defined(USE_IMAGE)
    if (argv[0] && *argv[0])
        MyProgramName = argv[0];
#endif /* defined(DONT_CALL_GC_AFTER_FORK) && defined(USE_IMAGE) */
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
                HostName = allocStr_n(hostname, hostname_len).ptr;
        }
    }

    /* argument search 1 */
    for (i = 1; i < argc; i++) {
        if (ISOPT("-config")) {
            argv[i] = "-dummy";
            config_file = NXTARG();
            argv[i] = "-dummy";
        }
    }

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
    DictHist = newHist();

    if (FollowLocale && Locale) {
        DisplayCharset = wc_guess_locale_charset(Locale, DisplayCharset);
        SystemCharset = wc_guess_locale_charset(Locale, SystemCharset);
    }
    auto_detect = WcOption.auto_detect;
    BookmarkCharset = DocumentCharset;

    init_from_env();
    mkdir(Sprintf("%s/%d", rc_dir, CurrentPid)->ptr, 0700);
    mkdir(Sprintf("%s/%d", tmp_dir, CurrentPid)->ptr, 0700);

    /* argument search 2 */
    for (i = 1; i < argc; i++) {
        if (*argv[i] == '+') {
            line_str = argv[i] + 1;
            continue;
        }

        if (*argv[i] != '-') {
            if (open_new_tab && load_argc)
                load_argv[load_argc++] = "";
            load_argv[load_argc++] = argv[i];
            continue;
        }

        if (!strcmp("-", argv[i]) || !strcmp("-dummy", argv[i]))
            continue;

        /*
         * Check for multi-letter flags first to avoid confusion with
         * single-letter flags that get their option-argument in the same
         * argument string without intervening <blank> characters.
         */
        if (ISOPT("-backend")) {
            deprecated |= 1;
            w3m_backend = true;
        } else if (ISOPT("-backend_batch")) {
            w3m_backend = true;
            if (!backend_batch_commands)
                backend_batch_commands = TextList_new();
            TextList_push(backend_batch_commands, NXTARG());
        } else if (ISOPT("-bookmark")) {
            BookmarkFile = NXTARG();
            if (BookmarkFile[0] != '~' && BookmarkFile[0] != '/') {
                pStr tmp = Strnew_charp(CurrentDir);
                if (Strlastchar(tmp) != '/')
                    Strcat_char(tmp, '/');
                Strcat_charp(tmp, BookmarkFile);
                BookmarkFile = cleanupName(tmp->ptr)->ptr;
            }
        } else if (ISOPT("-cols"))
            opt_cols = atoi(NXTARG());
        else if (ISOPT("-debug"))
            w3m_debug = true;
        else if (ISOPT("-dump"))
            w3m_dump = DUMP_BUFFER;
        else if (ISOPT("-dump_both"))
            w3m_dump = (DUMP_HEAD | DUMP_SOURCE);
        else if (ISOPT("-dump_extra"))
            w3m_dump = (DUMP_HEAD | DUMP_SOURCE | DUMP_EXTRA);
        else if (ISOPT("-dump_head"))
            w3m_dump = DUMP_HEAD;
        else if (ISOPT("-dump_source"))
            w3m_dump = DUMP_SOURCE;
        else if (ISOPT("-graph"))
            UseGraphicChar = GRAPHIC_CHAR_DEC;
        else if (ISOPT("-halfdump"))
            w3m_dump = DUMP_HALFDUMP;
        else if (ISOPT("-halfload")) {
            w3m_dump = 0;
            w3m_halfload = true;
            DefaultType = default_type = "text/html";
        } else if (ISOPT("-header")) {
            pStr hs;
            if ((hs = make_optional_header_string(NXTARG())))
                header_string = header_string ? Strcat(header_string, hs) : hs;
        } else if (ISOPT("-help"))
            help();
        else if (ISOPT("-no-graph"))
            UseGraphicChar = GRAPHIC_CHAR_ASCII;
        else if (ISOPT("-no-proxy"))
            use_proxy = false;
        else if (ISOPT("-num"))
            showLineNum = true;
        else if (ISOPT("-post"))
            post_file = NXTARG();
        else if (ISOPT("-ppc")) {
            double ppc;
            ppc = atof(NXTARG());
            if (ppc >= MINIMUM_PIXEL_PER_CHAR && ppc <= MAXIMUM_PIXEL_PER_CHAR) {
                pixel_per_char = ppc;
                set_pixel_per_char = true;
            }
        } else if (ISOPT("-reqlog"))
            w3m_reqlog = rcFile("request.log")->ptr;
        else if (ISOPT("-session")) {
            session_file = NXTARG();
        } else if (ISOPT("-show-option")) {
            show_params(stdout);
            exit(0);
        } else if (ISOPT("-title"))
            displayTitleTerm = argv[i][6] == '=' ? argv[i] + 7 : getenv("TERM");
        else if (ISOPT("-version")) {
            fversion(stdout);
            exit(0);
        }

        else if (ISOPT("-no-cookie"))
            use_cookie = accept_cookie = false;
        else if (ISOPT("-cookie"))
            use_cookie = accept_cookie = true;
        else if (ISOPT("-cookie-jar")) {
            CookieFile = NXTARG();
            if (CookieFile[0] != '~' && CookieFile[0] != '/') {
                pStr tmp = Strnew_charp(CurrentDir);
                if (Strlastchar(tmp) != '/')
                    Strcat_char(tmp, '/');
                Strcat_charp(tmp, CookieFile);
                CookieFile = cleanupName(tmp->ptr)->ptr;
            }
        }

        else if (ISOPT("-ppl")) {
            double ppc;
            ppc = atof(NXTARG());
            if (ppc >= MINIMUM_PIXEL_PER_CHAR && ppc <= MAXIMUM_PIXEL_PER_CHAR * 2) {
                pixel_per_line = ppc;
                set_pixel_per_line = true;
            }
        } else if (ISOPT("-ri"))
            enable_inline_image = INLINE_IMG_OSC5379;
        else if (ISOPT("-sixel"))
            enable_inline_image = INLINE_IMG_SIXEL;

        else if (ISOPT("-insecure")) {
#ifdef OPENSSL_TLS_SECURITY_LEVEL
            set_param_option("ssl_cipher=ALL:eNULL:@SECLEVEL=0");
#else
                set_param_option("ssl_cipher=ALL:eNULL");
#endif
            set_param_option("ssl_min_version=all");
            set_param_option("ssl_forbid_method=");
            set_param_option("ssl_verify_server=0");
        }

        /* Single-letter flags */
        else if (ISOPT("-B"))
            load_bookmark = true;
        else if (ISOPT("-F"))
            RenderFrame = true;
        else if (ISOPT("-N"))
            open_new_tab = true;
        else if (ISOPT("-R"))
            opt_restore = true;
        else if (CHKOPT("-T"))
            DefaultType = default_type = getarg(argv, &i);
        else if (ISOPT("-V")) {
            fversion(stdout);
            exit(0);
        } else if (ISOPT("-W"))
            WrapDefault = !WrapDefault;
        else if (ISOPT("-X"))
            use_ti_te = false;

        else if (ISOPT("-h"))
            help();
        else if (CHKOPT("-l")) {
            if (atoi(getarg(argv, &i)) > 0)
                PagerMax = atoi(argv[i]);
        } else if (ISOPT("-m"))
            SearchHeader = search_header = true;
        else if (ISOPT("-o")) {
            /* "?" is undocumented and only kept for backwards compatibility */
            if (!argv[i + 1] || !strcmp(argv[i + 1], "?")) {
                show_params(stdout);
                exit(0);
            }
            p = NXTARG();
            goto setopt;
        } else if (CHKOPT("-o")) {
            p = getarg(argv, &i);
        setopt:
            if (!set_param_option(p)) {
                fprintf(stderr, _("%s: bad option\n"), p);
                fputs(_("Use 'w3m -o' to see all options\n"), stderr);
                exit(2);
            }
        } else if (ISOPT("-r"))
            ShowEffect = false;
        else if (ISOPT("-s"))
            squeezeBlankLine = true;
        else if (CHKOPT("-t")) {
            if (atoi(getarg(argv, &i)) > 0)
                Tabstop = atoi(argv[i]);
        } else if (ISOPT("-v"))
            visual_start = true;

        else if (ISOPT("-4") || ISOPT("-6"))
            set_param_option(Sprintf("dns_order=%c", argv[i][1])->ptr);

        else if (ISOPT("-M"))
            useColor = false;
        else if (ISOPT("-H")) {
            deprecated |= 1;
            highIntensityColors = true;
        }

        else if (CHKOPT("-I")) {
            DocumentCharset = wc_guess_charset_short(getarg(argv, &i),
                DocumentCharset);
            WcOption.auto_detect = WC_OPT_DETECT_OFF;
            UseContentCharset = false;
        } else if (CHKOPT("-O"))
            DisplayCharset = wc_guess_charset_short(getarg(argv, &i),
                DisplayCharset);

#if defined(DONT_CALL_GC_AFTER_FORK) && defined(USE_IMAGE)
        else if (!strcmp("-$$getimage", argv[i])) {
            ++i;
            getimage_args = argv + i;
            i += 4;
            if (i > argc)
                usage();
        }
#endif /* defined(DONT_CALL_GC_AFTER_FORK) && defined(USE_IMAGE) */
        else {
            usage();
        }
    }
#undef ISOPT
#undef CHKOPT
#undef NXTARG

    FirstTab = NULL;
    LastTab = NULL;
    nTab = 0;
    CurrentTab = NULL;
    CurrentKey = -1;
    if (BookmarkFile == NULL)
        BookmarkFile = rcFile(BOOKMARK)->ptr;
    if (!CookieFile)
        CookieFile = rcFile(COOKIE_FILE)->ptr;

    if (!isatty(1) && !w3m_dump) /* redirected output */
        w3m_dump = DUMP_BUFFER;
    if (w3m_dump) {
        // COLS = opt_cols ? opt_cols : MaxCols ? MaxCols : DEFAULT_COLS;
    }

    if (!w3m_dump && !w3m_backend) {
        fmInit();
        mySignal(SIGWINCH, resize_hook);
    } else if (w3m_halfdump && displayImage)
        activeImage = true;

    sync_with_option();
    initCookie();
    if (UseHistory)
        loadUrlHistory();

    /* Restore a previously saved session */
    if (opt_restore) {
        FILE* fp;
        pStr line;
        char *sf, **session;
        int max = 16, n = 0;

        sf = session_file ? session_file : rcFile(SESSION_FILE)->ptr;
        session = New_N(char*, max);
        if (!(fp = fopen(sf, "r"))) {
            pStr err = Sprintf("Cannot restore session %s - %s", sf,
                strerror(errno));
            disp_err_message(err->ptr, false);
            fmTerm();
            return 1;
        }

        for (int i = 0; i < load_argc; i++) {
            if (n > max) {
                max <<= 1;
                New_Reuse(char*, session, max);
            }
            session[n++] = load_argv[i];
        }

        if (open_new_tab) {
            if (n > max) {
                max <<= 1;
                session = New_Reuse(char*, session, max);
            }
            session[n++] = "";
        }

        for (;;) {
            line = Strfgets(fp);
            if (line->len == 0)
                break;
            Strchop(line);
            if (n > max) {
                max <<= 1;
                session = New_Reuse(char*, session, max);
            }
            session[n++] = line->ptr;
        }
        load_argv = session;
        load_argc = n;
        if (!session_file) {
            session_bak = Strnew_m_charp(sf, "~", NULL)->ptr;
            rename(sf, session_bak);
        }
    }

    if (w3m_backend)
        backend();
#if defined(DONT_CALL_GC_AFTER_FORK) && defined(USE_IMAGE)
    if (getimage_args) {
        char* image_url = conv_from_system(getimage_args[0]);
        char* base_url = conv_from_system(getimage_args[1]);
        struct Url base_pu;

        base_pu = parseURL2(base_url, NULL);
        image_source = getimage_args[2];
        newbuf = loadGeneralFile(image_url, &base_pu, NULL, 0, NULL);
        if (!newbuf || !newbuf->real_type || strncasecmp(newbuf->real_type, "image/", 6))
            unlink(getimage_args[2]);
        symlink(getimage_args[2], getimage_args[3]);
        w3m_exit(0);
    }
#endif /* defined(DONT_CALL_GC_AFTER_FORK) && defined(USE_IMAGE) */

    if (w3m_dump)
        mySignal(SIGINT, SIG_IGN);
    mySignal(SIGCHLD, sig_chld);
    mySignal(SIGPIPE, SigPipe);

    orig_GC_warn_proc = GC_get_warn_proc();
    GC_set_warn_proc(wrap_GC_warn_proc);

    if (load_bookmark) {
        if (!(newbuf = loadGeneralFile(BookmarkFile, NULL, NO_REFERER, 0, NULL))) {
            err_msg = Strcat_charp(err_msg, "w3m: Can't load bookmark.\n");
            w3m_exit(1);
        }
        proc_buf(newbuf, search_header, open_new_tab);
    }

    if (load_argc == 0) {
        /* no URL specified */
        if (!isatty(0)) {
            redin = newFileStream(fdopen(dup(0), "rb"), pclose);
            newbuf = openGeneralPagerBuffer(redin);
            dup2(1, 0);
        } else if (visual_start) {
            /* FIXME: gettextize? */
            pStr s_page;
            s_page = Strnew_charp("<title>W3M startup page</title><center><b>Welcome to ");
            Strcat_charp(s_page, "<a href='http://w3m.sourceforge.net/'>");
            Strcat_m_charp(s_page,
                "w3m</a>!<p><p>This is w3m version ",
                W3M_VERSION,
                "<br>Written by <a href='mailto:aito@fw.ipsj.or.jp'>Akinori Ito</a>",
                NULL);
            if (!(newbuf = loadHTMLString(s_page)))
                err_msg = Strcat_charp(err_msg,
                    "w3m: Can't load string.\n"); /* sigint */
        } else if ((non_null(p = getenv("HTTP_HOME"))) || non_null((p = getenv("WWW_HOME")))) {
            newbuf = loadGeneralFile(p, NULL, NO_REFERER, 0, NULL);
            if (newbuf == NULL)
                err_msg = Strcat(err_msg, Sprintf("w3m: Can't load %s.\n", p));
            else if (newbuf != NO_BUFFER)
                pushHashHist(URLHist, parsedURL2Str(&newbuf->currentURL)->ptr);
        } else {
            usage();
        }
        if (!newbuf)
            w3m_exit(1);
        proc_buf(newbuf, search_header, open_new_tab);
    }

    for (i = 0; i < load_argc; i++) {
        SearchHeader = search_header;
        DefaultType = default_type;
        int retry = 0;

        if (!*load_argv[i]) {
            open_new_tab = true;
            continue;
        }

        const char* url = load_argv[i];
        if (getURLScheme(&url) == SCM_MISSING && !ArgvIsURL)
        retry_as_local_file:
            url = file_to_url(load_argv[i], CurrentDir)->ptr;
        else {
            pStr os = Strnew();
            struct Writer w = makeWriter(os);
            conv_from_system(&WcOption, &w, load_argv[i]);
            url = url_encode(os->ptr, NULL, 0);
        }
        if (w3m_dump == DUMP_HEAD) {
            request = New(FormList);
            request->method = FORM_METHOD_HEAD;
            newbuf = loadGeneralFile(url, NULL, NO_REFERER, 0, request);
        } else {
            if (post_file && i == 0) {
                FILE* fp;
                pStr body;
                if (!strcmp(post_file, "-"))
                    fp = stdin;
                else
                    fp = fopen(post_file, "r");
                if (fp == NULL) {
                    err_msg = Strcat(err_msg,
                        Sprintf(_("w3m: Can't open %s.\n"),
                            post_file));
                    continue;
                }
                body = Strfgetall(fp);
                if (fp != stdin)
                    fclose(fp);
                request = newFormList(NULL, "post", NULL, NULL, NULL, NULL,
                    NULL);
                request->body = body->ptr;
                request->boundary = NULL;
                request->length = body->len;
            } else {
                request = NULL;
            }
            newbuf = loadGeneralFile(url, NULL, NO_REFERER, 0, request);
        }
        if (newbuf == NULL) {
            if (ArgvIsURL && !retry) {
                retry = 1;
                goto retry_as_local_file;
            }
            err_msg = Strcat(err_msg,
                Sprintf(_("w3m: Can't load %s.\n"),
                    load_argv[i]));
            continue;
        } else if (newbuf == NO_BUFFER)
            continue;
        switch (newbuf->real_scheme) {
        case SCM_MAILTO:
            break;
        case SCM_LOCAL:
        case SCM_LOCAL_CGI:
            unshiftHist(LoadHist, url);
        default:
            pushHashHist(URLHist, parsedURL2Str(&newbuf->currentURL)->ptr);
            break;
        }
        proc_buf(newbuf, search_header, open_new_tab);
        open_new_tab = false;
    }
    if (w3m_dump) {
        save_cookies();
        w3m_exit(!!err_msg);
    }

    if (add_download_list) {
        add_download_list = false;
        CurrentTab = LastTab;
        if (!FirstTab) {
            FirstTab = LastTab = CurrentTab = newTab();
            nTab = 1;
        }
        if (!Firstbuf || Firstbuf == NO_BUFFER) {
            Firstbuf = Currentbuf = newBuffer(INIT_BUFFER_WIDTH);
            Currentbuf->bufferprop = BP_INTERNAL | BP_NO_URL;
            Currentbuf->buffername = DOWNLOAD_LIST_TITLE;
        } else
            Currentbuf = Firstbuf;
        ldDL();
    } else
        CurrentTab = FirstTab;
    if (!FirstTab || !Firstbuf || Firstbuf == NO_BUFFER) {
        if (newbuf == NO_BUFFER) {
            if (fmInitialized)
                inputChar(_("Hit any key to quit w3m:"));
        }
        if (newbuf == NO_BUFFER)
            save_cookies();
        w3m_exit(!!err_msg);
    }
    if (err_msg)
        disp_message_nsec(err_msg->ptr, false, 1, true, false);

    SearchHeader = false;
    DefaultType = NULL;
    UseContentCharset = true;
    WcOption.auto_detect = auto_detect;

    Currentbuf = Firstbuf;
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
    if (line_str) {
        _goLine(line_str);
    }
    /* main loop */
    for (;;) {
        if (add_download_list) {
            add_download_list = false;
            ldDL();
        }
        if (Currentbuf->submit) {
            Anchor* a = Currentbuf->submit;
            Currentbuf->submit = NULL;
            gotoLine(Currentbuf, a->start.line);
            Currentbuf->pos = a->start.pos;
            _followForm(true);
            continue;
        }
        /* event processing */
        if (CurrentEvent) {
            CurrentKey = -1;
            CurrentCmdData = CurrentEvent->data;
            w3mFuncList[CurrentEvent->cmd].func();
            CurrentCmdData = NULL;
            CurrentEvent = CurrentEvent->next;
            continue;
        }
        /* get keypress event */

        if (Currentbuf->event) {
            if (Currentbuf->event->status != AL_UNSET) {
                setAlarmEventOrDefaultAlarm(Currentbuf->event);
                if (Currentbuf->event->sec == 0) { /* refresh (0sec) */
                    Currentbuf->event = NULL;
                    CurrentKey = -1;
                    CurrentCmdData = CurrentAlarm->data;
                    w3mFuncList[CurrentAlarm->cmd].func();
                    CurrentCmdData = NULL;
                    continue;
                }
            } else
                Currentbuf->event = NULL;
        }
        if (!Currentbuf->event)
            setAlarmEventOrDefaultAlarm(0);

        if (CurrentAlarm->sec > 0) {
            mySignal(SIGALRM, SigAlarm);
            alarm(CurrentAlarm->sec);
        }
        mySignal(SIGWINCH, resize_hook);
        if (activeImage && displayImage && Currentbuf->imgList && !Currentbuf->image_loaded) {
            do {
                if (need_resize_screen)
                    resize_screen();
                loadImage(Currentbuf, IMG_FLAG_NEXT);
            } while (tty_sleep_till_anykey(1, 0) <= 0);
        } else {
            do {
                if (need_resize_screen)
                    resize_screen();
            } while (tty_sleep_till_anykey(1, 0) <= 0);
        }
        c = tty_getch();
        if (CurrentAlarm->sec > 0) {
            alarm(0);
        }
        if (IS_ASCII(c)) { /* Ascii */
            if (('0' <= c) && (c <= '9') && (prec_num || (GlobalKeymap[c] == FUNCNAME_nulcmd))) {
                prec_num = prec_num * 10 + (int)(c - '0');
                if (prec_num > PREC_LIMIT)
                    prec_num = PREC_LIMIT;
            } else {
                set_buffer_environ(Currentbuf);
                save_buffer_position(Currentbuf);
                keyPressEventProc(c);
                prec_num = 0;
            }
        }
        prev_key = CurrentKey;
        CurrentKey = -1;
    }
}
