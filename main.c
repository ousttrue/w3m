#include "main.h"
#include "siteconf.h"
#include "form.h"
#include "frame.h"
#include "parsetag.h"
#include "func.h"
#include "istream.h"
#include "indep.h"
#include "textlist.h"
#include "anchor.h"
#include "proxy.h"
#include "signal_util.h"
#include "downloadlist.h"
#include "alarm.h"
#include "backend.h"
#include "search.h"
#include "wc_util.h"
#include "maparea.h"
#include "etc.h"
#include "ftp.h"
#include "news.h"
#include "url.h"
#include "buffer.h"
#include "cookie.h"
#include "rc.h"
#include "local.h"
#include "image.h"
#include "global.h"
#include "constants.h"
#include "history.h"
#include "line_input.h"
#include "terms.h"
#include "defun_impl.h"
#include "keybind.h"
#include "fm.h"
#include "proto.h"
#include "display.h"
#include "terms.h"
#include "myctype.h"
#include "regex.h"
#include "rc.h"
#include "setjmp_util.h"

#include <libwc/charset.h>
#include <libwc/ucs.h>

#include <stdio.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <locale.h>
#include <sys/wait.h>
#include <time.h>

static TextList* fileToDelete = 0;

void addDeleteFile(const char* file)
{
    pushText(fileToDelete, file);
}

#define USE_IMAGE 1
unsigned char last_key = 0;

#include "util.h"

#define DSTR_LEN 256
#define BOOKMARK "bookmark.html"

typedef struct _Event {
    const char* cmd;
    void* data;
    struct _Event* next;
} Event;
static Event* CurrentEvent = NULL;
static Event* LastEvent = NULL;

AlarmEvent DefaultAlarm = {
    0, AL_UNSET, "NOTHING", NULL
};
static AlarmEvent* CurrentAlarm = &DefaultAlarm;
static MySignalHandler SigAlarm(SIGNAL_ARG);

static int need_resize_screen = FALSE;
static MySignalHandler resize_hook(SIGNAL_ARG);
static void resize_screen(void);

static MySignalHandler SigPipe(SIGNAL_ARG);

char* MarkString = NULL;


static sigjmp_buf IntReturn;

static void keyPressEventProc(int c);
int show_params_p = 0;
void show_params(FILE* fp);

int display_ok = FALSE;
static void do_dump(Buffer*);
int prec_num = 0;
int prev_key = -1;
int on_target = 1;

void set_buffer_environ(Buffer*);
static void save_buffer_position(Buffer* buf);

int check_target = TRUE;
#define PREC_LIMIT 10000

#define help() fusage(stdout, 0)
#define usage() fusage(stderr, 1)

static void
fversion(FILE* f)
{
    fprintf(f, "w3m version %s, options %s\n", w3m_version,
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
        ",mark");
}

static void
fusage(FILE* f, int err)
{
    fversion(f);
    /* FIXME: gettextize? */
    fprintf(f, "usage: w3m [options] [URL or filename]\noptions:\n");
    fprintf(f, "    -t tab           set tab width\n");
    fprintf(f, "    -r               ignore backspace effect\n");
    fprintf(f, "    -l line          # of preserved line (default 10000)\n");
    fprintf(f, "    -I charset       document charset\n");
    fprintf(f, "    -O charset       display/output charset\n");
#if 0 /* use -O{s|j|e} instead */
    fprintf(f, "    -e               EUC-JP\n");
    fprintf(f, "    -s               Shift_JIS\n");
    fprintf(f, "    -j               JIS\n");
#endif
    fprintf(f, "    -B               load bookmark\n");
    fprintf(f, "    -bookmark file   specify bookmark file\n");
    fprintf(f, "    -T type          specify content-type\n");
    fprintf(f, "    -m               internet message mode\n");
    fprintf(f, "    -v               visual startup mode\n");
    fprintf(f, "    -M               monochrome display\n");
    fprintf(f, "    -H               use high-intensity colors\n");
    fprintf(f,
        "    -N               open URL of command line on each new tab\n");
    fprintf(f, "    -F               automatically render frames\n");
    fprintf(f,
        "    -cols width      specify column width (used with -dump)\n");
    fprintf(f,
        "    -ppc count       specify the number of pixels per character (4.0...32.0)\n");
    fprintf(f,
        "    -ppl count       specify the number of pixels per line (4.0...64.0)\n");
    fprintf(f, "    -dump            dump formatted page into stdout\n");
    fprintf(f,
        "    -dump_head       dump response of HEAD request into stdout\n");
    fprintf(f, "    -dump_source     dump page source into stdout\n");
    fprintf(f, "    -dump_both       dump HEAD and source into stdout\n");
    fprintf(f,
        "    -dump_extra      dump HEAD, source, and extra information into stdout\n");
    fprintf(f, "    -post file       use POST method with file content\n");
    fprintf(f, "    -header string   insert string as a header\n");
    fprintf(f, "    +<num>           goto <num> line\n");
    fprintf(f, "    -num             show line number\n");
    fprintf(f, "    -no-proxy        don't use proxy\n");
    fprintf(f, "    -4               IPv4 only (-o dns_order=4)\n");
    fprintf(f, "    -6               IPv6 only (-o dns_order=6)\n");
    fprintf(f, "    -insecure        use insecure SSL config options\n");
    fprintf(f,
        "    -cookie          use cookie (-no-cookie: don't use cookie)\n");
    fprintf(f, "    -graph           use DEC special graphics for border of table and menu\n");
    fprintf(f, "    -no-graph        use ASCII character for border of table and menu\n");
#if 1 /* pager requires -s */
    fprintf(f, "    -s               squeeze multiple blank lines\n");
#else
    fprintf(f, "    -S               squeeze multiple blank lines\n");
#endif
    fprintf(f, "    -W               toggle search wrap mode\n");
    fprintf(f, "    -X               don't use termcap init/deinit\n");
    fprintf(f,
        "    -title[=TERM]    set buffer name to terminal title string\n");
    fprintf(f, "    -o opt=value     assign value to config option\n");
    fprintf(f, "    -show-option     print all config options\n");
    fprintf(f, "    -config file     specify config file\n");
    fprintf(f, "    -debug           use debug mode (only for debugging)\n");
    fprintf(f, "    -reqlog          write request logfile\n");
    fprintf(f, "    -help            print this usage message\n");
    fprintf(f, "    -version         print w3m version\n");
    if (show_params_p)
        show_params(f);
    exit(err);
}

static GC_warn_proc orig_GC_warn_proc = NULL;
#define GC_WARN_KEEP_MAX (20)

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
                sleep_till_anykey(1, 1);
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
    exitDownloadList();
    mySignal(SIGCHLD, sig_chld);
    return;
}

static Str
make_optional_header_string(char* s)
{
    char* p;
    Str hs;

    if (strchr(s, '\n') || strchr(s, '\r'))
        return NULL;
    for (p = s; *p && *p != ':'; p++)
        ;
    if (*p != ':' || p == s)
        return NULL;
    hs = Strnew_size(strlen(s) + 3);
    Strcopy_charp_n(hs, s, p - s);
    if (!Strcasecmp_charp(hs, "content-type"))
        override_content_type = TRUE;
    if (!Strcasecmp_charp(hs, "user-agent"))
        override_user_agent = TRUE;
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
    fprintf(stderr, "Out of memory: %lu bytes unavailable!\n", (unsigned long)bytes);
    exit(1);
    /*
     * Suppress compiler warning: function might return no value
     * This code is never reached.
     */
    return NULL;
}

int w3m_main(int argc, char** argv)
{
    Buffer* newbuf = NULL;
    char* p;
    int c, i;
    InputStream redin;
    char* line_str = NULL;
    char** load_argv;
    FormList* request;
    int load_argc = 0;
    int load_bookmark = FALSE;
    int visual_start = FALSE;
    int open_new_tab = FALSE;
    char search_header = FALSE;
    char* default_type = NULL;
    char* post_file = NULL;
    Str err_msg;
    char* Locale = NULL;
    wc_uint8 auto_detect;
    if (!getenv("GC_LARGE_ALLOC_WARN_INTERVAL"))
        set_environ("GC_LARGE_ALLOC_WARN_INTERVAL", "30000");
    GC_INIT();
#if (GC_VERSION_MAJOR > 7) || ((GC_VERSION_MAJOR == 7) && (GC_VERSION_MINOR >= 2))
    GC_set_oom_fn(die_oom);
#else
    GC_oom_fn = die_oom;
#endif

    setlocale(LC_ALL, "");

    proxyInit();
    fileToDelete = newTextList();

    load_argv = New_N(char*, argc - 1);
    load_argc = 0;

    CurrentDir = currentdir();
    CurrentPid = (int)getpid();
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
            if (hostname_len <= HOST_NAME_MAX && hostname_len < STR_SIZE_MAX)
                HostName = allocStr(hostname, (int)hostname_len);
        }
    }

    /* argument search 1 */
    for (i = 1; i < argc; i++) {
        if (*argv[i] == '-') {
            if (!strcmp("-config", argv[i])) {
                argv[i] = "-dummy";
                if (++i >= argc)
                    usage();
                config_file = argv[i];
                argv[i] = "-dummy";
            } else if (!strcmp("-h", argv[i]) || !strcmp("-help", argv[i]))
                help();
            else if (!strcmp("-V", argv[i]) || !strcmp("-version", argv[i])) {
                fversion(stdout);
                exit(0);
            }
        }
    }

    if (non_null(Locale = getenv("LC_ALL")) || non_null(Locale = getenv("LC_CTYPE")) || non_null(Locale = getenv("LANG"))) {
        DisplayCharset = wc_guess_locale_charset(Locale, DisplayCharset);
        DocumentCharset = wc_guess_locale_charset(Locale, DocumentCharset);
        SystemCharset = wc_guess_locale_charset(Locale, SystemCharset);
    }

    /* initializations */
    init_rc();

    initHist();

    if (FollowLocale && Locale) {
        DisplayCharset = wc_guess_locale_charset(Locale, DisplayCharset);
        SystemCharset = wc_guess_locale_charset(Locale, SystemCharset);
    }
    auto_detect = WcOption.auto_detect;
    BookmarkCharset = DocumentCharset;

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

    /* argument search 2 */
    i = 1;
    while (i < argc) {
        if (*argv[i] == '-') {
            if (!strcmp("-t", argv[i])) {
                if (++i >= argc)
                    usage();
                if (atoi(argv[i]) > 0)
                    Tabstop = atoi(argv[i]);
            } else if (!strcmp("-r", argv[i]))
                ShowEffect = FALSE;
            else if (!strcmp("-l", argv[i])) {
                if (++i >= argc)
                    usage();
                if (atoi(argv[i]) > 0)
                    PagerMax = atoi(argv[i]);
            }
#if 0 /* use -O{s|j|e} instead */
	    else if (!strcmp("-s", argv[i]))
		DisplayCharset = WC_CES_SHIFT_JIS;
	    else if (!strcmp("-j", argv[i]))
		DisplayCharset = WC_CES_ISO_2022_JP;
	    else if (!strcmp("-e", argv[i]))
		DisplayCharset = WC_CES_EUC_JP;
#endif
            else if (!strncmp("-I", argv[i], 2)) {
                if (argv[i][2] != '\0')
                    p = argv[i] + 2;
                else {
                    if (++i >= argc)
                        usage();
                    p = argv[i];
                }
                DocumentCharset = wc_guess_charset_short(p, DocumentCharset);
                WcOption.auto_detect = WC_OPT_DETECT_OFF;
                UseContentCharset = FALSE;
            } else if (!strncmp("-O", argv[i], 2)) {
                if (argv[i][2] != '\0')
                    p = argv[i] + 2;
                else {
                    if (++i >= argc)
                        usage();
                    p = argv[i];
                }
                DisplayCharset = wc_guess_charset_short(p, DisplayCharset);
            } else if (!strcmp("-graph", argv[i]))
                UseGraphicChar = GRAPHIC_CHAR_DEC;
            else if (!strcmp("-no-graph", argv[i]))
                UseGraphicChar = GRAPHIC_CHAR_ASCII;
            else if (!strcmp("-T", argv[i])) {
                if (++i >= argc)
                    usage();
                DefaultType = default_type = argv[i];
            } else if (!strcmp("-m", argv[i]))
                SearchHeader = search_header = TRUE;
            else if (!strcmp("-v", argv[i]))
                visual_start = TRUE;
            else if (!strcmp("-N", argv[i]))
                open_new_tab = TRUE;
            else if (!strcmp("-M", argv[i]))
                useColor = FALSE;
            else if (!strcmp("-H", argv[i]))
                highIntensityColors = TRUE;
            else if (!strcmp("-B", argv[i]))
                load_bookmark = TRUE;
            else if (!strcmp("-bookmark", argv[i])) {
                if (++i >= argc)
                    usage();
                BookmarkFile = argv[i];
                if (BookmarkFile[0] != '~' && BookmarkFile[0] != '/') {
                    Str tmp = Strnew_charp(CurrentDir);
                    if (Strlastchar(tmp) != '/')
                        Strcat_char(tmp, '/');
                    Strcat_charp(tmp, BookmarkFile);
                    BookmarkFile = cleanupName(tmp->ptr);
                }
            } else if (!strcmp("-F", argv[i]))
                RenderFrame = TRUE;
            else if (!strcmp("-W", argv[i])) {
                if (WrapDefault)
                    WrapDefault = FALSE;
                else
                    WrapDefault = TRUE;
            } else if (!strcmp("-dump", argv[i]))
                w3m_dump = DUMP_BUFFER;
            else if (!strcmp("-dump_source", argv[i]))
                w3m_dump = DUMP_SOURCE;
            else if (!strcmp("-dump_head", argv[i]))
                w3m_dump = DUMP_HEAD;
            else if (!strcmp("-dump_both", argv[i]))
                w3m_dump = (DUMP_HEAD | DUMP_SOURCE);
            else if (!strcmp("-dump_extra", argv[i]))
                w3m_dump = (DUMP_HEAD | DUMP_SOURCE | DUMP_EXTRA);
            else if (!strcmp("-halfdump", argv[i]))
                w3m_dump = DUMP_HALFDUMP;
            else if (!strcmp("-halfload", argv[i])) {
                w3m_dump = 0;
                w3m_halfload = TRUE;
                DefaultType = default_type = "text/html";
            } else if (!strcmp("-backend", argv[i])) {
                w3m_backend = TRUE;
            } else if (!strcmp("-backend_batch", argv[i])) {
                w3m_backend = TRUE;
                if (++i >= argc)
                    usage();
                if (!backend_batch_commands)
                    backend_batch_commands = newTextList();
                pushText(backend_batch_commands, argv[i]);
            } else if (!strcmp("-cols", argv[i])) {
                if (++i >= argc)
                    usage();
                COLS = atoi(argv[i]);
                if (COLS > MAXIMUM_COLS) {
                    COLS = MAXIMUM_COLS;
                }
            } else if (!strcmp("-ppc", argv[i])) {
                double ppc;
                if (++i >= argc)
                    usage();
                ppc = atof(argv[i]);
                if (ppc >= MINIMUM_PIXEL_PER_CHAR && ppc <= MAXIMUM_PIXEL_PER_CHAR) {
                    pixel_per_char = ppc;
                    set_pixel_per_char = TRUE;
                }
            } else if (!strcmp("-ppl", argv[i])) {
                double ppc;
                if (++i >= argc)
                    usage();
                ppc = atof(argv[i]);
                if (ppc >= MINIMUM_PIXEL_PER_CHAR && ppc <= MAXIMUM_PIXEL_PER_CHAR * 2) {
                    pixel_per_line = ppc;
                    set_pixel_per_line = TRUE;
                }
            } else if (!strcmp("-ri", argv[i])) {
                enable_inline_image = INLINE_IMG_OSC5379;
            } else if (!strcmp("-sixel", argv[i])) {
                enable_inline_image = INLINE_IMG_SIXEL;
            } else if (!strcmp("-num", argv[i]))
                showLineNum = TRUE;
            else if (!strcmp("-no-proxy", argv[i]))
                use_proxy = FALSE;
            else if (!strcmp("-4", argv[i]) || !strcmp("-6", argv[i]))
                set_param_option(Sprintf("dns_order=%c", argv[i][1])->ptr);
            else if (!strcmp("-post", argv[i])) {
                if (++i >= argc)
                    usage();
                post_file = argv[i];
            } else if (!strcmp("-header", argv[i])) {
                Str hs;
                if (++i >= argc)
                    usage();
                if ((hs = make_optional_header_string(argv[i])) != NULL) {
                    if (header_string == NULL)
                        header_string = hs;
                    else
                        Strcat(header_string, hs);
                }
                while (argv[i][0]) {
                    argv[i][0] = '\0';
                    argv[i]++;
                }
            } else if (!strcmp("-no-cookie", argv[i])) {
                use_cookie = FALSE;
                accept_cookie = FALSE;
            } else if (!strcmp("-cookie", argv[i])) {
                use_cookie = TRUE;
                accept_cookie = TRUE;
            }
#if 1 /* pager requires -s */
            else if (!strcmp("-s", argv[i]))
#else
            else if (!strcmp("-S", argv[i]))
#endif
                squeezeBlankLine = TRUE;
            else if (!strcmp("-X", argv[i]))
                Do_not_use_ti_te = TRUE;
            else if (!strcmp("-title", argv[i]))
                displayTitleTerm = getenv("TERM");
            else if (!strncmp("-title=", argv[i], 7))
                displayTitleTerm = argv[i] + 7;
            else if (!strcmp("-insecure", argv[i])) {
#ifdef OPENSSL_TLS_SECURITY_LEVEL
                set_param_option("ssl_cipher=ALL:eNULL:@SECLEVEL=0");
#else
                set_param_option("ssl_cipher=ALL:eNULL");
#endif
#ifdef SSL_CTX_set_min_proto_version
                set_param_option("ssl_min_version=all");
#endif
                set_param_option("ssl_forbid_method=");
                set_param_option("ssl_verify_server=0");
            } else if (!strcmp("-o", argv[i]) || !strcmp("-show-option", argv[i])) {
                if (!strcmp("-show-option", argv[i]) || ++i >= argc || !strcmp(argv[i], "?")) {
                    show_params(stdout);
                    exit(0);
                }
                if (!set_param_option(argv[i])) {
                    /* option set failed */
                    /* FIXME: gettextize? */
                    fprintf(stderr, "%s: bad option\n", argv[i]);
                    show_params_p = 1;
                    usage();
                }
            } else if (!strcmp("-", argv[i]) || !strcmp("-dummy", argv[i])) {
                /* do nothing */
            } else if (!strcmp("-debug", argv[i])) {
                w3m_debug = TRUE;
            } else if (!strcmp("-reqlog", argv[i])) {
                w3m_reqlog = rcFile("request.log");
            }
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
        } else if (*argv[i] == '+') {
            line_str = argv[i] + 1;
        } else {
            load_argv[load_argc++] = argv[i];
        }
        i++;
    }

    FirstTab = NULL;
    LastTab = NULL;
    nTab = 0;
    CurrentTab = NULL;
    CurrentKey = -1;
    if (BookmarkFile == NULL)
        BookmarkFile = rcFile(BOOKMARK);

    if (!isatty(1) && !w3m_dump) {
        /* redirected output */
        w3m_dump = DUMP_BUFFER;
    }
    if (w3m_dump) {
        if (COLS == 0)
            COLS = DEFAULT_COLS;
    }

    if (!w3m_dump && !w3m_backend) {
        fmInit();
        mySignal(SIGWINCH, resize_hook);
    } else if (w3m_halfdump && displayImage)
        activeImage = TRUE;

    sync_with_option();
    initCookie();
    if (UseHistory)
        loadHistory(URLHist);

    /*  if (w3m_dump)
     *    WcOption.pre_conv = WC_TRUE;
     */

    if (w3m_backend)
        backend();
#if defined(DONT_CALL_GC_AFTER_FORK) && defined(USE_IMAGE)
    if (getimage_args) {
        char* image_url = conv_from_system(getimage_args[0]);
        char* base_url = conv_from_system(getimage_args[1]);
        ParsedURL base_pu;

        parseURL2(base_url, &base_pu, NULL);
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

#if (GC_VERSION_MAJOR > 7) || ((GC_VERSION_MAJOR == 7) && (GC_VERSION_MINOR >= 2))
    orig_GC_warn_proc = GC_get_warn_proc();
    GC_set_warn_proc(wrap_GC_warn_proc);
#else
    orig_GC_warn_proc = GC_set_warn_proc(wrap_GC_warn_proc);
#endif
    err_msg = Strnew();
    if (load_argc == 0) {
        /* no URL specified */
        if (!isatty(0)) {
            redin = newFileStream(fdopen(dup(0), "rb"), (void (*)())pclose);
            newbuf = openGeneralPagerBuffer(redin);
            dup2(1, 0);
        } else if (load_bookmark) {
            newbuf = loadGeneralFile(BookmarkFile, NULL, NO_REFERER, 0, NULL);
            if (newbuf == NULL)
                Strcat_charp(err_msg, "w3m: Can't load bookmark.\n");
        } else if (visual_start) {
            /* FIXME: gettextize? */
            Str s_page;
            s_page = Strnew_charp("<title>W3M startup page</title><center><b>Welcome to ");
            Strcat_charp(s_page, "<a href='http://w3m.sourceforge.net/'>");
            Strcat_m_charp(s_page,
                "w3m</a>!<p><p>This is w3m version ",
                w3m_version,
                "<br>Written by <a href='mailto:aito@fw.ipsj.or.jp'>Akinori Ito</a>",
                NULL);
            newbuf = loadHTMLString(s_page);
            if (newbuf == NULL)
                Strcat_charp(err_msg, "w3m: Can't load string.\n");
            else if (newbuf != NO_BUFFER)
                newbuf->bufferprop |= (BP_INTERNAL | BP_NO_URL);
        } else if ((p = getenv("HTTP_HOME")) != NULL || (p = getenv("WWW_HOME")) != NULL) {
            newbuf = loadGeneralFile(p, NULL, NO_REFERER, 0, NULL);
            if (newbuf == NULL)
                Strcat(err_msg, Sprintf("w3m: Can't load %s.\n", p));
            else if (newbuf != NO_BUFFER)
                pushHashHist(URLHist, parsedURL2Str(&newbuf->currentURL)->ptr);
        } else {
            if (fmInitialized)
                fmTerm();
            usage();
        }
        if (newbuf == NULL) {
            if (fmInitialized)
                fmTerm();
            if (err_msg->length)
                fprintf(stderr, "%s", err_msg->ptr);
            w3m_exit(2);
        }
        i = -1;
    } else {
        i = 0;
    }
    for (; i < load_argc; i++) {
        if (i >= 0) {
            SearchHeader = search_header;
            DefaultType = default_type;
            int retry = 0;

            const char* url = load_argv[i];
            if (getURLScheme(&url) == SCM_MISSING && !ArgvIsURL)
            retry_as_local_file:
                url = file_to_url(load_argv[i]);
            else
                url = url_encode(conv_from_system(load_argv[i]), NULL, 0);
            if (w3m_dump == DUMP_HEAD) {
                request = New(FormList);
                request->method = FORM_METHOD_HEAD;
                newbuf = loadGeneralFile(url, NULL, NO_REFERER, 0, request);
            } else {
                if (post_file && i == 0) {
                    FILE* fp;
                    Str body;
                    if (!strcmp(post_file, "-"))
                        fp = stdin;
                    else
                        fp = fopen(post_file, "r");
                    if (fp == NULL) {
                        /* FIXME: gettextize? */
                        Strcat(err_msg,
                            Sprintf("w3m: Can't open %s.\n", post_file));
                        continue;
                    }
                    body = Strfgetall(fp);
                    if (fp != stdin)
                        fclose(fp);
                    request = newFormList(NULL, "post", NULL, NULL, NULL, NULL,
                        NULL);
                    request->body = body->ptr;
                    request->boundary = NULL;
                    request->length = body->length;
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
                /* FIXME: gettextize? */
                Strcat(err_msg,
                    Sprintf("w3m: Can't load %s.\n", load_argv[i]));
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
        } else if (newbuf == NO_BUFFER)
            continue;
        if (newbuf->pagerSource || (newbuf->real_scheme == SCM_LOCAL && newbuf->header_source && newbuf->currentURL.file && strcmp(newbuf->currentURL.file, "-")))
            newbuf->search_header = search_header;
        if (CurrentTab == NULL) {
            FirstTab = LastTab = CurrentTab = newTab();
            if (!FirstTab) {
                fprintf(stderr, "%s\n", "Can't allocated memory");
                exit(1);
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
        if (!w3m_dump || w3m_dump == DUMP_BUFFER) {
            if (Currentbuf->frameset != NULL && RenderFrame)
                rFrame((struct CmdArgs) { 0 });
        }
        if (w3m_dump)
            do_dump(Currentbuf);
        else {
            Currentbuf = newbuf;
        }
    }
    if (w3m_dump) {
        if (err_msg->length)
            fprintf(stderr, "%s", err_msg->ptr);
        save_cookies();
        w3m_exit(0);
    }

    if (checkAddDownloadList()) {
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
        ldDL((struct CmdArgs) { 0 });
    } else
        CurrentTab = FirstTab;
    if (!FirstTab || !Firstbuf || Firstbuf == NO_BUFFER) {
        if (newbuf == NO_BUFFER) {
            if (fmInitialized)
                /* FIXME: gettextize? */
                inputChar("Hit any key to quit w3m:");
        }
        if (fmInitialized)
            fmTerm();
        if (err_msg->length)
            fprintf(stderr, "%s", err_msg->ptr);
        if (newbuf == NO_BUFFER) {
            save_cookies();
            if (!err_msg->length)
                w3m_exit(0);
        }
        w3m_exit(2);
    }
    if (err_msg->length)
        disp_message_nsec(err_msg->ptr, FALSE, 1, TRUE, FALSE);

    SearchHeader = FALSE;
    DefaultType = NULL;
    UseContentCharset = TRUE;
    WcOption.auto_detect = auto_detect;

    Currentbuf = Firstbuf;
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
    if (line_str) {
        _goLine(line_str);
    }
    for (;;) {
        if (checkDownloadList()) {
            ldDL((struct CmdArgs) { 0 });
        }
        if (Currentbuf->submit) {
            Anchor* a = Currentbuf->submit;
            Currentbuf->submit = NULL;
            gotoLine(Currentbuf, a->start.line);
            Currentbuf->pos = a->start.pos;
            _followForm(TRUE);
            continue;
        }
        /* event processing */
        if (CurrentEvent) {
            CurrentKey = -1;
            CurrentKeyData = NULL;
            CurrentCmdData = (char*)CurrentEvent->data;
            w3mFunc(CurrentEvent->cmd);
            CurrentCmdData = NULL;
            CurrentEvent = CurrentEvent->next;
            continue;
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
                    w3mFunc(CurrentAlarm->cmd);
                    CurrentCmdData = NULL;
                    continue;
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
            do {
                if (need_resize_screen)
                    resize_screen();
                loadImage(Currentbuf, IMG_FLAG_NEXT);
            } while (sleep_till_anykey(1, 0) <= 0);
        } else {
            do {
                if (need_resize_screen)
                    resize_screen();
            } while (sleep_till_anykey(1, 0) <= 0);
        }
        c = getch();
        last_key = c;
        if (CurrentAlarm->sec > 0) {
            alarm(0);
        }
        if (IS_ASCII(c)) { /* Ascii */
            if (('0' <= c) && (c <= '9') && (prec_num || 0 == strcmp(GlobalKeymap[c], "NOTHING"))) {
                prec_num = prec_num * 10 + (int)(c - '0');
                if (prec_num > PREC_LIMIT)
                    prec_num = PREC_LIMIT;
            } else {
                set_buffer_environ(Currentbuf);
                save_buffer_position(Currentbuf);
                keyPressEventProc((int)c);
                prec_num = 0;
            }
        }
        prev_key = CurrentKey;
        CurrentKey = -1;
        CurrentKeyData = NULL;
    }
}

static void
keyPressEventProc(int c)
{
    CurrentKey = c;
    w3mFunc(GlobalKeymap[c]);
}

void pushEvent(const char* cmd, void* data)
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
    if (buf->document_header == NULL) {
        if (w3m_dump & DUMP_EXTRA)
            printf("\n");
        return;
    }
    for (TextListItem* ti = buf->document_header->first; ti; ti = ti->next) {
        printf("%s", Strnew_wc_output(wc_conv_strict(WcOption, ti->ptr, InnerCharset, buf->document_charset))->ptr);
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
        Str tmp = Strnew();
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
    return (*((const Anchor**)a))->hseq - (*((const Anchor**)b))->hseq;
}

static void
do_dump(Buffer* buf)
{
    MySignalHandler (*volatile prevtrap)(SIGNAL_ARG) = NULL;

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
        saveBuffer(buf, stdout, FALSE);
        if (displayLinkNumber && buf->href) {
            int nanchor = buf->href->nanchor;
            printf("\nReferences:\n\n");
            Anchor** in_order = New_N(Anchor*, buf->href->nanchor);
            for (i = 0; i < nanchor; i++)
                in_order[i] = buf->href->anchors + i;
            qsort(in_order, nanchor, sizeof(Anchor*), cmp_anchor_hseq);
            for (i = 0; i < nanchor; i++) {
                ParsedURL pu;
                char* url;
                if (in_order[i]->slave)
                    continue;
                parseURL2(in_order[i]->url, &pu, baseURL(buf));
                url = url_decode2(parsedURL2Str(&pu)->ptr, Currentbuf);
                printf("[%d] %s\n", in_order[i]->hseq + 1, url);
            }
        }
    }
    mySignal(SIGINT, prevtrap);
}

void pcmap(void)
{
}

void escKeyProc(int c, int esc, const char* map[128])
{
    if (CurrentKey >= 0 && CurrentKey & K_MULTI) {
        const char*** mmap = (const char***)getKeyData(MULTI_KEY(CurrentKey));
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
        w3mFunc(map[c]);
}

void escdmap(char c)
{
    int d;
    d = (int)c - (int)'0';
    c = getch();
    if (IS_DIGIT(c)) {
        d = d * 10 + (int)c - (int)'0';
        c = getch();
    }
    if (c == '~')
        escKeyProc((int)d, K_ESCD, EscDKeymap);
}

void tmpClearBuffer(Buffer* buf)
{
    if (buf->pagerSource == NULL && writeBufferCache(buf) == 0) {
        buf->firstLine = NULL;
        buf->topLine = NULL;
        buf->currentLine = NULL;
        buf->lastLine = NULL;
    }
}

void pushBuffer(Buffer* buf)
{
    Buffer* b;

    deleteImage(Currentbuf);
    if (clear_buffer)
        tmpClearBuffer(Currentbuf);
    if (Firstbuf == Currentbuf) {
        buf->nextBuffer = Firstbuf;
        Firstbuf = Currentbuf = buf;
    } else if ((b = prevBuffer(Firstbuf, Currentbuf)) != NULL) {
        b->nextBuffer = buf;
        buf->nextBuffer = Currentbuf;
        Currentbuf = buf;
    }
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

void repBuffer(Buffer* oldbuf, Buffer* buf)
{
    Firstbuf = replaceBuffer(Firstbuf, oldbuf, buf);
    Currentbuf = buf;
}

MySignalHandler
intTrap(SIGNAL_ARG)
{ /* Interrupt catcher */
    LONGJMP(IntReturn, 0);
    SIGNAL_RETURN;
}

static MySignalHandler
resize_hook(SIGNAL_ARG)
{
    need_resize_screen = TRUE;
    mySignal(SIGWINCH, resize_hook);
    SIGNAL_RETURN;
}

static void
resize_screen(void)
{
    need_resize_screen = FALSE;
    setlinescols();
    setupscreen();
    if (CurrentTab)
        displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

static MySignalHandler
SigPipe(SIGNAL_ARG)
{
    mySignal(SIGPIPE, SigPipe);
    SIGNAL_RETURN;
}

/*
 * Command functions: These functions are called with a keystroke.
 */

void nscroll(int n, int mode)
{
    Buffer* buf = Currentbuf;
    Line *top = buf->topLine, *cur = buf->currentLine;
    int lnum, tlnum, llnum, diff_n;

    if (buf->firstLine == NULL)
        return;
    lnum = cur->linenumber;
    buf->topLine = lineSkip(buf, top, n, FALSE);
    if (buf->topLine == top) {
        lnum += n;
        if (lnum < buf->topLine->linenumber)
            lnum = buf->topLine->linenumber;
        else if (lnum > buf->lastLine->linenumber)
            lnum = buf->lastLine->linenumber;
    } else {
        tlnum = buf->topLine->linenumber;
        llnum = buf->topLine->linenumber + buf->LINES - 1;
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



void shiftvisualpos(Buffer* buf, int shift)
{
    Line* l = buf->currentLine;
    buf->visualpos -= shift;
    if (buf->visualpos - l->bwidth >= buf->COLS)
        buf->visualpos = l->bwidth + buf->COLS - 1;
    else if (buf->visualpos - l->bwidth < 0)
        buf->visualpos = l->bwidth;
    arrangeLine(buf);
    if (buf->visualpos - l->bwidth == -shift && buf->cursorX == 0)
        buf->visualpos = l->bwidth;
}

void cmd_loadfile(char* fn)
{
    Buffer* buf;

    buf = loadGeneralFile(file_to_url(fn), NULL, NO_REFERER, 0, NULL);
    if (buf == NULL) {
        /* FIXME: gettextize? */
        char* emsg = Sprintf("%s not found", conv_from_system(fn))->ptr;
        disp_err_message(emsg, FALSE);
    } else if (buf != NO_BUFFER) {
        pushBuffer(buf);
        if (RenderFrame && Currentbuf->frameset != NULL)
            rFrame((struct CmdArgs) { 0 });
    }
    displayBuffer(Currentbuf, B_NORMAL);
}

/* Move cursor left */
void _movL(int n)
{
    int i, m = searchKeyNum();
    if (Currentbuf->firstLine == NULL)
        return;
    for (i = 0; i < m; i++)
        cursorLeft(Currentbuf, n);
    displayBuffer(Currentbuf, B_NORMAL);
}

/* Move cursor downward */
void _movD(int n)
{
    int i, m = searchKeyNum();
    if (Currentbuf->firstLine == NULL)
        return;
    for (i = 0; i < m; i++)
        cursorDown(Currentbuf, n);
    displayBuffer(Currentbuf, B_NORMAL);
}

/* move cursor upward */
void _movU(int n)
{
    int i, m = searchKeyNum();
    if (Currentbuf->firstLine == NULL)
        return;
    for (i = 0; i < m; i++)
        cursorUp(Currentbuf, n);
    displayBuffer(Currentbuf, B_NORMAL);
}

/* Move cursor right */
void _movR(int n)
{
    int i, m = searchKeyNum();
    if (Currentbuf->firstLine == NULL)
        return;
    for (i = 0; i < m; i++)
        cursorRight(Currentbuf, n);
    displayBuffer(Currentbuf, B_NORMAL);
}

/* movLW, movRW */
/*
 * From: Takashi Nishimoto <g96p0935@mse.waseda.ac.jp> Date: Mon, 14 Jun
 * 1999 09:29:56 +0900
 */

wc_uint32 getChar(char* p)
{
    return wc_any_to_ucs(WcOption, wtf_parse1((wc_uchar**)&p));
}

int is_wordchar(wc_uint32 c)
{
    return wc_is_ucs_alnum(c);
}

int prev_nonnull_line(Line* line)
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

int next_nonnull_line(Line* line)
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

void _quitfm(int confirm)
{
    char* ans = "y";

    if (checkDownloadList())
        /* FIXME: gettextize? */
        ans = inputChar("Download process retains. "
                        "Do you want to exit w3m? (y/n)");
    else if (confirm)
        /* FIXME: gettextize? */
        ans = inputChar("Do you want to exit w3m? (y/n)");
    if (!(ans && TOLOWER(*ans) == 'y')) {
        displayBuffer(Currentbuf, B_NORMAL);
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

/* Go to specified line */
void _goLine(char* l)
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
            -(Currentbuf->LINES + 1) / 2, TRUE);
        Currentbuf->currentLine = Currentbuf->lastLine;
    } else
        gotoRealLine(Currentbuf, atoi(l));
    arrangeCursor(Currentbuf);
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

int cur_real_linenumber(Buffer* buf)
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

static Buffer*
loadNormalBuf(Buffer* buf, int renderframe)
{
    pushBuffer(buf);
    if (renderframe && RenderFrame && Currentbuf->frameset != NULL)
        rFrame((struct CmdArgs) { 0 });
    return buf;
}

Buffer* loadLink(char* url, char* target, char* referer, FormList* request)
{
    Buffer *buf, *nfbuf;
    union frameset_element* f_element = NULL;
    int flag = 0;
    ParsedURL *base, pu;
    const int* no_referer_ptr;

    message(Sprintf("loading %s", url)->ptr, 0, 0);
    refresh();

    no_referer_ptr = query_SCONF_NO_REFERER_FROM(&Currentbuf->currentURL);
    base = baseURL(Currentbuf);
    if ((no_referer_ptr && *no_referer_ptr) || base == NULL || base->scheme == SCM_LOCAL || base->scheme == SCM_LOCAL_CGI || base->scheme == SCM_DATA)
        referer = NO_REFERER;
    if (referer == NULL)
        referer = parsedURL2RefererStr(&Currentbuf->currentURL)->ptr;
    buf = loadGeneralFile(url, baseURL(Currentbuf), referer, flag, request);
    if (buf == NULL) {
        char* emsg = Sprintf("Can't load %s", url)->ptr;
        disp_err_message(emsg, FALSE);
        return NULL;
    }

    parseURL2(url, &pu, base);
    pushHashHist(URLHist, parsedURL2Str(&pu)->ptr);

    if (buf == NO_BUFFER) {
        return NULL;
    }
    if (!on_target) /* open link as an indivisual page */
        return loadNormalBuf(buf, TRUE);

    if (do_download) /* download (thus no need to render frames) */
        return loadNormalBuf(buf, FALSE);

    if (target == NULL || /* no target specified (that means this page is not a frame page) */
        !strcmp(target, "_top") || /* this link is specified to be opened as an indivisual * page */
        !(Currentbuf->bufferprop & BP_FRAME) /* This page is not a frame page */
    ) {
        return loadNormalBuf(buf, TRUE);
    }
    nfbuf = Currentbuf->linkBuffer[LB_N_FRAME];
    if (nfbuf == NULL) {
        /* original page (that contains <frameset> tag) doesn't exist */
        return loadNormalBuf(buf, TRUE);
    }

    f_element = search_frame(nfbuf->frameset, target);
    if (f_element == NULL) {
        /* specified target doesn't exist in this frameset */
        return loadNormalBuf(buf, TRUE);
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
    rFrame((struct CmdArgs) { 0 });
    {
        const char* label = pu.label;

        Anchor* al = NULL;
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
                    FALSE);
            Currentbuf->pos = al->start.pos;
            arrangeCursor(Currentbuf);
        }
    }
    displayBuffer(Currentbuf, B_NORMAL);
    return buf;
}

void gotoLabel(const char* label)
{
    Buffer* buf;
    Anchor* al;
    int i;

    al = searchURLLabel(Currentbuf, label);
    if (al == NULL) {
        /* FIXME: gettextize? */
        disp_message(Sprintf("%s is not found", label)->ptr, TRUE);
        return;
    }
    buf = newBuffer(Currentbuf->width);
    copyBuffer(buf, Currentbuf);
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
            FALSE);
    Currentbuf->pos = al->start.pos;
    arrangeCursor(Currentbuf);
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
    return;
}

int handleMailto(const char* url)
{
    Str to;
    char* pos;

    if (strncasecmp(url, "mailto:", 7))
        return 0;
    if (!non_null(Mailer)) {
        /* FIXME: gettextize? */
        disp_err_message("no mailer is specified", TRUE);
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
    exec_cmd(myExtCommand(Mailer, shell_quote(file_unquote(to->ptr)),
        FALSE)
            ->ptr);
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
    pushHashHist(URLHist, url);
    return 1;
}

/* follow HREF link in the buffer */
void bufferA(void)
{
    on_target = FALSE;
    followA((struct CmdArgs) { 0 });
    on_target = TRUE;
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

static Str
conv_form_encoding(Str val, FormItemList* fi, Buffer* buf)
{
    wc_ces charset = SystemCharset;
    if (fi->parent->charset)
        charset = fi->parent->charset;
    else if (buf->document_charset && buf->document_charset != WC_CES_US_ASCII)
        charset = buf->document_charset;
    return Strnew_wc_output(wc_Str_conv_strict(WcOption, val->ptr, val->length, InnerCharset, charset));
}

void query_from_followform(Str* query, FormItemList* fi, int multipart)
{
    FormItemList* f2;
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
                        conv_form_encoding(f2->name, fi, Currentbuf)->ptr,
                        (*query)->ptr,
                        Str_conv_to_system(f2->value->ptr, f2->value->length)->ptr);
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

/* process form */
void followForm(void)
{
    _followForm(FALSE);
}

void _followForm(int submit)
{
    Anchor *a, *a2;
    char* p;
    FormItemList *fi, *f2;
    Str tmp, tmp2;
    int multipart = 0, i;

    if (Currentbuf->firstLine == NULL)
        return;

    a = retrieveCurrentForm(Currentbuf);
    if (a == NULL)
        return;
    fi = (FormItemList*)a->url;
    switch (fi->type) {
    case FORM_INPUT_TEXT:
        if (submit)
            goto do_submit;
        if (fi->readonly)
            /* FIXME: gettextize? */
            disp_message_nsec("Read only field!", FALSE, 1, TRUE, FALSE);
        /* FIXME: gettextize? */
        p = inputStrHist("TEXT:", fi->value ? fi->value->ptr : NULL, TextHist);
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
            disp_message_nsec("Read only field!", FALSE, 1, TRUE, FALSE);
        /* FIXME: gettextize? */
        p = inputFilenameHist("Filename:", fi->value ? fi->value->ptr : NULL,
            NULL);
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
            disp_message_nsec("Read only field!", FALSE, 1, TRUE, FALSE);
            break;
        }
        /* FIXME: gettextize? */
        p = inputLine("Password:", fi->value ? fi->value->ptr : NULL,
            IN_PASSWORD);
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
            disp_message_nsec("Read only field!", FALSE, 1, TRUE, FALSE);
        input_textarea(fi);
        formUpdateBuffer(a, Currentbuf, fi);
        break;
    case FORM_INPUT_RADIO:
        if (submit)
            goto do_submit;
        if (fi->readonly) {
            /* FIXME: gettextize? */
            disp_message_nsec("Read only field!", FALSE, 1, TRUE, FALSE);
            break;
        }
        formRecheckRadio(a, Currentbuf, fi);
        break;
    case FORM_INPUT_CHECKBOX:
        if (submit)
            goto do_submit;
        if (fi->readonly) {
            /* FIXME: gettextize? */
            disp_message_nsec("Read only field!", FALSE, 1, TRUE, FALSE);
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
                Strshrink(tmp2, (tmp2->ptr + tmp2->length) - p);
        }

        if (fi->parent->method == FORM_METHOD_GET) {
            if ((p = strchr(tmp2->ptr, '?')) != NULL)
                Strshrink(tmp2, (tmp2->ptr + tmp2->length) - p);
            Strcat_charp(tmp2, "?");
            Strcat(tmp2, tmp);
            loadLink(tmp2->ptr, a->target, NULL, NULL);
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
                FALSE);
        }
        break;
    case FORM_INPUT_RESET:
        for (i = 0; i < Currentbuf->formitem->nanchor; i++) {
            a2 = &Currentbuf->formitem->anchors[i];
            f2 = (FormItemList*)a2->url;
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

/* go to the next [visited] anchor */
void _nextA(int visited)
{
    struct HmarkerList* hl = Currentbuf->hmarklist;
    BufferPoint* po;
    Anchor *an, *pan;
    int i, x, y, n = searchKeyNum();
    ParsedURL url;

    if (Currentbuf->firstLine == NULL)
        return;
    if (!hl || hl->nmark == 0)
        return;

    an = retrieveCurrentAnchor(Currentbuf);
    if (visited != TRUE && an == NULL)
        an = retrieveCurrentForm(Currentbuf);

    y = Currentbuf->currentLine->linenumber;
    x = Currentbuf->pos;

    if (visited == TRUE) {
        n = hl->nmark;
    }

    for (i = 0; i < n; i++) {
        pan = an;
        if (an && an->hseq >= 0) {
            int hseq = an->hseq + 1;
            do {
                if (hseq >= hl->nmark) {
                    if (visited == TRUE)
                        return;
                    an = pan;
                    goto _end;
                }
                po = &hl->marks[hseq];
                an = retrieveAnchor(Currentbuf->href, po->line, po->pos);
                if (visited != TRUE && an == NULL)
                    an = retrieveAnchor(Currentbuf->formitem, po->line,
                        po->pos);
                hseq++;
                if (visited == TRUE && an) {
                    parseURL2(an->url, &url, baseURL(Currentbuf));
                    if (getHashHist(URLHist, parsedURL2Str(&url)->ptr)) {
                        goto _end;
                    }
                }
            } while (an == NULL || an == pan);
        } else {
            an = closest_next_anchor(Currentbuf->href, NULL, x, y);
            if (visited != TRUE)
                an = closest_next_anchor(Currentbuf->formitem, an, x, y);
            if (an == NULL) {
                if (visited == TRUE)
                    return;
                an = pan;
                break;
            }
            x = an->start.pos;
            y = an->start.line;
            if (visited == TRUE) {
                parseURL2(an->url, &url, baseURL(Currentbuf));
                if (getHashHist(URLHist, parsedURL2Str(&url)->ptr)) {
                    goto _end;
                }
            }
        }
    }
    if (visited == TRUE)
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
void _prevA(int visited)
{
    struct HmarkerList* hl = Currentbuf->hmarklist;
    BufferPoint* po;
    Anchor *an, *pan;
    int i, x, y, n = searchKeyNum();
    ParsedURL url;

    if (Currentbuf->firstLine == NULL)
        return;
    if (!hl || hl->nmark == 0)
        return;

    an = retrieveCurrentAnchor(Currentbuf);
    if (visited != TRUE && an == NULL)
        an = retrieveCurrentForm(Currentbuf);

    y = Currentbuf->currentLine->linenumber;
    x = Currentbuf->pos;

    if (visited == TRUE) {
        n = hl->nmark;
    }

    for (i = 0; i < n; i++) {
        pan = an;
        if (an && an->hseq >= 0) {
            int hseq = an->hseq - 1;
            do {
                if (hseq < 0) {
                    if (visited == TRUE)
                        return;
                    an = pan;
                    goto _end;
                }
                po = hl->marks + hseq;
                an = retrieveAnchor(Currentbuf->href, po->line, po->pos);
                if (visited != TRUE && an == NULL)
                    an = retrieveAnchor(Currentbuf->formitem, po->line,
                        po->pos);
                hseq--;
                if (visited == TRUE && an) {
                    parseURL2(an->url, &url, baseURL(Currentbuf));
                    if (getHashHist(URLHist, parsedURL2Str(&url)->ptr)) {
                        goto _end;
                    }
                }
            } while (an == NULL || an == pan);
        } else {
            an = closest_prev_anchor(Currentbuf->href, NULL, x, y);
            if (visited != TRUE)
                an = closest_prev_anchor(Currentbuf->formitem, an, x, y);
            if (an == NULL) {
                if (visited == TRUE)
                    return;
                an = pan;
                break;
            }
            x = an->start.pos;
            y = an->start.line;
            if (visited == TRUE && an) {
                parseURL2(an->url, &url, baseURL(Currentbuf));
                if (getHashHist(URLHist, parsedURL2Str(&url)->ptr)) {
                    goto _end;
                }
            }
        }
    }
    if (visited == TRUE)
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

/* go to the next left/right anchor */
void nextX(int d, int dy)
{
    struct HmarkerList* hl = Currentbuf->hmarklist;
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
    displayBuffer(Currentbuf, B_NORMAL);
}

/* go to the next downward/upward anchor */
void nextY(int d)
{
    struct HmarkerList* hl = Currentbuf->hmarklist;
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
    displayBuffer(Currentbuf, B_NORMAL);
}

int checkBackBuffer(Buffer* buf)
{
    Buffer* fbuf = buf->linkBuffer[LB_N_FRAME];

    if (fbuf) {
        if (fbuf->frameQ)
            return TRUE; /* Currentbuf has stacked frames */
        /* when no frames stacked and next is frame source, try next's
         * nextBuffer */
        if (RenderFrame && fbuf == buf->nextBuffer) {
            if (fbuf->nextBuffer != NULL)
                return TRUE;
            else
                return FALSE;
        }
    }

    if (buf->nextBuffer)
        return TRUE;

    return FALSE;
}

void cmd_loadURL(const char* url, ParsedURL* current, char* referer, FormList* request)
{
    if (handleMailto(url))
        return;

    refresh();
    Buffer* buf = loadGeneralFile(url, current, referer, 0, request);
    if (buf == NULL) {
        /* FIXME: gettextize? */
        char* emsg = Sprintf("Can't load %s", conv_from_system(url))->ptr;
        disp_err_message(emsg, FALSE);
    } else if (buf != NO_BUFFER) {
        pushBuffer(buf);
        if (RenderFrame && Currentbuf->frameset != NULL)
            rFrame((struct CmdArgs) { 0 });
    }
    displayBuffer(Currentbuf, B_NORMAL);
}

/* go to specified URL */
void goURL0(char* prompt, int relative)
{
    char *url, *referer;
    ParsedURL p_url, *current;
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
            parseURL2(a->url, &p_url, current);
            a_url = parsedURL2Str(&p_url)->ptr;
            if (DefaultURLString == DEFAULT_URL_LINK)
                url = url_decode2(a_url, Currentbuf);
            else
                pushHist(hist, a_url);
        }
        url = inputLineHist(prompt, url, IN_URL, hist);
        if (url != NULL)
            SKIP_BLANKS(url);
    }
    if (relative) {
        no_referer_ptr = query_SCONF_NO_REFERER_FROM(&Currentbuf->currentURL);
        current = baseURL(Currentbuf);
        if ((no_referer_ptr && *no_referer_ptr) || current == NULL || current->scheme == SCM_LOCAL || current->scheme == SCM_LOCAL_CGI || current->scheme == SCM_DATA)
            referer = NO_REFERER;
        else
            referer = parsedURL2RefererStr(&Currentbuf->currentURL)->ptr;
        url = url_encode(url, current, Currentbuf->document_charset);
    } else {
        current = NULL;
        referer = NULL;
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
    parseURL2(url, &p_url, current);
    pushHashHist(URLHist, parsedURL2Str(&p_url)->ptr);
    cmd_loadURL(url, current, referer, NULL);
    if (Currentbuf != cur_buf) /* success */
        pushHashHist(URLHist, parsedURL2Str(&Currentbuf->currentURL)->ptr);
}

void cmd_loadBuffer(Buffer* buf, int prop, int linkid)
{
    if (buf == NULL) {
        disp_err_message("Can't load string", FALSE);
    } else if (buf != NO_BUFFER) {
        buf->bufferprop |= (BP_INTERNAL | prop);
        if (!(buf->bufferprop & BP_NO_URL))
            copyParsedURL(&buf->currentURL, &Currentbuf->currentURL);
        if (linkid != LB_NOLINK) {
            buf->linkBuffer[REV_LB[linkid]] = Currentbuf;
            Currentbuf->linkBuffer[linkid] = buf;
        }
        pushBuffer(buf);
    }
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

void follow_map(struct parsed_tagarg* arg)
{
    char* name = tag_get_value(arg, "link");
    Anchor* an;
    MapArea* a;
    int x, y;
    ParsedURL p_url;

    an = retrieveCurrentImg(Currentbuf);
    x = Currentbuf->cursorX + Currentbuf->rootX;
    y = Currentbuf->cursorY + Currentbuf->rootY;
    a = follow_map_menu(Currentbuf, name, an, x, y);
    if (a == NULL || a->url == NULL || *(a->url) == '\0') {
        return;
    }
    if (*(a->url) == '#') {
        gotoLabel(a->url + 1);
        return;
    }
    parseURL2(a->url, &p_url, baseURL(Currentbuf));
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
}

void anchorMn(Anchor* (*menu_func)(Buffer*), int go)
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
    displayBuffer(Currentbuf, B_NORMAL);
    if (go)
        followA((struct CmdArgs) { 0 });
}

void _peekURL(int only_img)
{

    Anchor* a;
    ParsedURL pu;
    static Str s = NULL;
    static Lineprop* p = NULL;
    Lineprop* pp;
    static int offset = 0, n;

    if (Currentbuf->firstLine == NULL)
        return;
    if (CurrentKey == prev_key && s != NULL) {
        if (s->length - offset >= COLS)
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
            s = Strnew_charp(form2str((FormItemList*)a->url));
    }
    if (s == NULL) {
        parseURL2(a->url, &pu, baseURL(Currentbuf));
        s = parsedURL2Str(&pu);
    }
    if (DecodeURL)
        s = Strnew_charp(url_decode2(s->ptr, Currentbuf));
    s = checkType(s, &pp, NULL);
    p = NewAtom_N(Lineprop, s->length);
    bcopy((void*)pp, (void*)p, s->length * sizeof(Lineprop));
disp:
    n = searchKeyNum();
    if (n > 1 && s->length > (n - 1) * (COLS - 1))
        offset = (n - 1) * (COLS - 1);
    while (offset < s->length && p[offset] & PC_WCHAR2)
        offset++;
    disp_message_nomouse(&s->ptr[offset], TRUE);
}

/* show current URL */
Str currentURL(void)
{
    if (Currentbuf->bufferprop & BP_INTERNAL)
        return Strnew_size(0);
    return parsedURL2Str(&Currentbuf->currentURL);
}

void _docCSet(wc_ces charset)
{
    if (Currentbuf->bufferprop & BP_INTERNAL)
        return;
    if (Currentbuf->sourcefile == NULL) {
        disp_message("Can't reload...", FALSE);
        return;
    }
    Currentbuf->document_charset = charset;
    Currentbuf->need_reshape = TRUE;
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

/* mark URL-like patterns as anchors */
void chkURLBuffer(Buffer* buf)
{
    static char* url_like_pat[] = {
        "https?://[a-zA-Z0-9][a-zA-Z0-9:%\\-\\./?=~_\\&+@#,\\$;]*[a-zA-Z0-9_/=\\-]",
        "file:/[a-zA-Z0-9:%\\-\\./=_\\+@#,\\$;]*",
        "gopher://[a-zA-Z0-9][a-zA-Z0-9:%\\-\\./_]*",
        "ftp://[a-zA-Z0-9][a-zA-Z0-9:%\\-\\./=_+@#,\\$]*[a-zA-Z0-9_/]",
        "news:[^<> 	][^<> 	]*",
        "nntp://[a-zA-Z0-9][a-zA-Z0-9:%\\-\\./_]*",
        "mailto:[^<> 	][^<> 	]*@[a-zA-Z0-9][a-zA-Z0-9\\-\\._]*[a-zA-Z0-9]",
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

/* mark Message-ID-like patterns as NEWS anchors */
void chkNMIDBuffer(Buffer* buf)
{
    static char* url_like_pat[] = {
        "<[!-;=?-~]+@[a-zA-Z0-9\\.\\-_]+>",
        NULL,
    };
    int i;
    for (i = 0; url_like_pat[i]; i++) {
        reAnchorNews(buf, url_like_pat[i]);
    }
    buf->check_url |= CHK_NMID;
}

/* spawn external browser */
void invoke_browser(char* url)
{
    Str cmd;
    const char* browser = NULL;
    int bg = 0, len;

    CurrentKeyData = NULL; /* not allowed in w3m-control: */
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
            browser = inputStr("Browse command: ", NULL);
            if (browser != NULL)
                browser = conv_to_system(browser);
        }
    } else {
        browser = conv_to_system(browser);
    }
    if (browser == NULL || *browser == '\0') {
        displayBuffer(Currentbuf, B_NORMAL);
        return;
    }

    if ((len = strlen(browser)) >= 2 && browser[len - 1] == '&' && browser[len - 2] != '\\') {
        browser = allocStr(browser, len - 2);
        bg = 1;
    }
    cmd = myExtCommand(browser, shell_quote(url), FALSE);
    Strremovetrailingspaces(cmd);
    fmTerm();
    mySystem(cmd->ptr, bg);
    fmInit();
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

char* getCurWord(Buffer* buf, int* spos, int* epos)
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

char* GetWord(Buffer* buf)
{
    int b, e;
    char* p;

    if ((p = getCurWord(buf, &b, &e)) != NULL) {
        return Strnew_charp_n(p, e - b)->ptr;
    }
    return NULL;
}

#define DICTBUFFERNAME "*dictionary*"
void execdict(char* word)
{
    char *w, *dictcmd;
    Buffer* buf;

    if (!UseDictCommand || word == NULL || *word == '\0') {
        displayBuffer(Currentbuf, B_NORMAL);
        return;
    }
    w = conv_to_system(word);
    if (*w == '\0') {
        displayBuffer(Currentbuf, B_NORMAL);
        return;
    }
    dictcmd = Sprintf("%s?%s", DictCommand,
        Str_form_quote(Strnew_charp(w))->ptr)
                  ->ptr;
    buf = loadGeneralFile(dictcmd, NULL, NO_REFERER, 0, NULL);
    if (buf == NULL) {
        disp_message("Execution failed", TRUE);
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
        ParsedURL pu;
        char* s = GetWord(buf);
        set_environ("W3M_CURRENT_WORD", s ? s : "");
        a = retrieveCurrentAnchor(buf);
        if (a) {
            parseURL2(a->url, &pu, baseURL(buf));
            set_environ("W3M_CURRENT_LINK", parsedURL2Str(&pu)->ptr);
        } else
            set_environ("W3M_CURRENT_LINK", "");
        a = retrieveCurrentImg(buf);
        if (a) {
            parseURL2(a->url, &pu, baseURL(buf));
            set_environ("W3M_CURRENT_IMG", parsedURL2Str(&pu)->ptr);
        } else
            set_environ("W3M_CURRENT_IMG", "");
        a = retrieveCurrentForm(buf);
        if (a)
            set_environ("W3M_CURRENT_FORM", form2str((FormItemList*)a->url));
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

int searchKeyNum(void)
{
    char* d;
    int n = 1;

    d = searchKeyData();
    if (d != NULL)
        n = atoi(d);
    return n * PREC_NUM;
}

static void deleteFiles()
{
    Buffer* buf;
    char* f;

    for (CurrentTab = FirstTab; CurrentTab; CurrentTab = CurrentTab->nextTab) {
        while (Firstbuf && Firstbuf != NO_BUFFER) {
            buf = Firstbuf->nextBuffer;
            discardBuffer(Firstbuf);
            Firstbuf = buf;
        }
    }
    while ((f = popText(fileToDelete)) != NULL) {
        unlink(f);
        if (enable_inline_image == INLINE_IMG_SIXEL && strcmp(f + strlen(f) - 4, ".gif") == 0) {
            Str firstframe = Strnew_charp(f);
            Strcat_charp(firstframe, "-1");
            unlink(firstframe->ptr);
        }
    }
}

void w3m_exit(int i)
{
    stopDownload();
    deleteFiles();
    free_ssl_ctx();
    disconnectFTP();
    disconnectNews();
    if (mkd_tmp_dir)
        if (rmdir(mkd_tmp_dir) != 0) {
            fprintf(stderr, "Can't remove temporary directory (%s)!\n", mkd_tmp_dir);
            exit(1);
        }
    exit(i);
}

static MySignalHandler
SigAlarm(SIGNAL_ARG)
{
    char* data;

    if (CurrentAlarm->sec > 0) {
        CurrentKey = -1;
        CurrentKeyData = NULL;
        CurrentCmdData = data = (char*)CurrentAlarm->data;
        w3mFunc(CurrentAlarm->cmd);
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
    SIGNAL_RETURN;
}

AlarmEvent*
setAlarmEvent(AlarmEvent* event, int sec, short status, const char* cmd, void* data)
{
    if (event == NULL)
        event = New(AlarmEvent);
    event->sec = sec;
    event->status = status;
    event->cmd = cmd;
    event->data = data;
    return event;
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

void _newT(void)
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

TabBuffer* numTab(int n)
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

void calcTabPos(void)
{
    TabBuffer* tab;
    int lcol = 0, rcol = 0, col;
    int n1, n2, na, nx, ny, ix, iy;

    if (nTab <= 0)
        return;
    n1 = (COLS - rcol - lcol) / TabCols;
    if (n1 >= nTab) {
        n2 = 1;
        ny = 1;
    } else {
        if (n1 < 0)
            n1 = 0;
        n2 = COLS / TabCols;
        if (n2 == 0)
            n2 = 1;
        ny = (nTab - n1 - 1) / n2 + 2;
    }
    na = n1 + n2 * (ny - 1);
    n1 -= (na - nTab) / ny;
    if (n1 < 0)
        n1 = 0;
    na = n1 + n2 * (ny - 1);
    tab = FirstTab;
    for (iy = 0; iy < ny && tab; iy++) {
        if (iy == 0) {
            nx = n1;
            col = COLS - rcol - lcol;
        } else {
            nx = n2 - (na - nTab + (iy - 1)) / (ny - 1);
            col = COLS;
        }
        for (ix = 0; ix < nx && tab; ix++, tab = tab->nextTab) {
            tab->x1 = col * ix / nx;
            tab->x2 = col * (ix + 1) / nx - 1;
            tab->y = iy;
            if (iy == 0) {
                tab->x1 += lcol;
                tab->x2 += lcol;
            }
        }
    }
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

void followTab(TabBuffer* tab)
{
    Buffer* buf;
    Anchor* a;

    a = retrieveCurrentImg(Currentbuf);
    if (!(a && a->image && a->image->map))
        a = retrieveCurrentAnchor(Currentbuf);
    if (a == NULL)
        return;

    if (tab == CurrentTab) {
        check_target = FALSE;
        followA((struct CmdArgs) { 0 });
        check_target = TRUE;
        return;
    }
    _newT();
    buf = Currentbuf;
    check_target = FALSE;
    followA((struct CmdArgs) { 0 });
    check_target = TRUE;
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

void tabURL0(TabBuffer* tab, char* prompt, int relative)
{
    Buffer* buf;

    if (tab == CurrentTab) {
        goURL0(prompt, relative);
        return;
    }
    _newT();
    buf = Currentbuf;
    goURL0(prompt, relative);
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

void moveTab(TabBuffer* t, TabBuffer* t2, int right)
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

void resetPos(BufferPos* b)
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
