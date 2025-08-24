#include "parseArgs.h"
#include "fm.h"
#include "rc.h"
#include "tty.h"
#include "display.h"
#include "w3m.h"
#include "config.h"
#include <setjmp.h>
#include <signal.h>
#include <sys/wait.h>
#include <gc.h>

static int show_params_p = 0;
extern JMP_BUF IntReturn;

#define help() fusage(stdout, 0)
#define usage() fusage(stderr, 1)

static MySignalHandler
SigPipe(SIGNAL_ARG)
{
#ifdef USE_MIGEMO
    init_migemo();
#endif
    mySignal(SIGPIPE, SigPipe);
    SIGNAL_RETURN;
}

extern MySignalHandler
    resize_hook(SIGNAL_ARG);

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
#ifdef USE_COLOR
        ",color"
        ",ansi-color"
#endif
#ifdef USE_MENU
        ",menu"
#endif
        ",cookie"
        ",ssl"
        ",ssl-verify"
#ifdef USE_W3MMAILER
        ",w3mmailer"
#endif
#ifdef USE_NNTP
        ",nntp"
#endif
#ifdef USE_GOPHER
        ",gopher"
#endif
#ifdef INET6
        ",ipv6"
#endif
        ",alarm"
#ifdef USE_MARK
        ",mark"
#endif
#ifdef USE_MIGEMO
        ",migemo"
#endif
    );
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
#ifdef USE_COLOR
    fprintf(f, "    -M               monochrome display\n");
    fprintf(f, "    -H               use high-intensity colors\n");
#endif /* USE_COLOR */
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
#ifdef INET6
    fprintf(f, "    -4               IPv4 only (-o dns_order=4)\n");
    fprintf(f, "    -6               IPv6 only (-o dns_order=6)\n");
#endif
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
                sleep_till_anykey(1000, 1);
            }

            lock = 0;
        }
    } else if (orig_GC_warn_proc)
        orig_GC_warn_proc(msg, arg);
    else
        fprintf(stderr, msg, (unsigned long)arg);
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
    TextListItem* ti;

    if (buf->document_header == NULL) {
        if (w3m_dump & DUMP_EXTRA)
            printf("\n");
        return;
    }
    for (ti = buf->document_header->first; ti; ti = ti->next) {
        printf("%s",
            wc_conv_strict(ti->ptr, InnerCharset,
                buf->document_charset)
                ->ptr);
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
        char* p;
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

const char* parseArgs(int argc, char** argv)
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
#if defined(DONT_CALL_GC_AFTER_FORK) && defined(USE_IMAGE)
    char** getimage_args = NULL;
#endif /* defined(DONT_CALL_GC_AFTER_FORK) && defined(USE_IMAGE) */
    if (!getenv("GC_LARGE_ALLOC_WARN_INTERVAL"))
        set_environ("GC_LARGE_ALLOC_WARN_INTERVAL", "30000");
    GC_INIT();
    GC_set_oom_fn(die_oom);
    setlocale(LC_ALL, "");
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);

    NO_proxy_domains = newTextList();
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

    LoadHist = newHist();
    SaveHist = newHist();
    ShellHist = newHist();
    TextHist = newHist();
    URLHist = newHist();

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
#ifdef USE_GOPHER
    if (!non_null(GOPHER_proxy) && ((p = getenv("GOPHER_PROXY")) || (p = getenv("gopher_proxy")) || (p = getenv("GOPHER_proxy"))))
        GOPHER_proxy = p;
#endif /* USE_GOPHER */
    if (!non_null(FTP_proxy) && ((p = getenv("FTP_PROXY")) || (p = getenv("ftp_proxy")) || (p = getenv("FTP_proxy"))))
        FTP_proxy = p;
    if (!non_null(NO_proxy) && ((p = getenv("NO_PROXY")) || (p = getenv("no_proxy")) || (p = getenv("NO_proxy"))))
        NO_proxy = p;
#ifdef USE_NNTP
    if (!non_null(NNTP_server) && (p = getenv("NNTPSERVER")) != NULL)
        NNTP_server = p;
    if (!non_null(NNTP_mode) && (p = getenv("NNTPMODE")) != NULL)
        NNTP_mode = p;
#endif

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
            } else if (!strncmp("-I", argv[i], 2)) {
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
            } else if (!strcmp("-s", argv[i]))
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
#if defined(HAVE_SYMLINK) && defined(HAVE_LSTAT)
        symlink(getimage_args[2], getimage_args[3]);
#else
        {
            FILE* f = fopen(getimage_args[3], "w");
            if (f)
                fclose(f);
        }
#endif
        w3m_exit(0);
    }
#endif /* defined(DONT_CALL_GC_AFTER_FORK) && defined(USE_IMAGE) */

    if (w3m_dump)
        mySignal(SIGINT, SIG_IGN);
    mySignal(SIGCHLD, sig_chld);
    mySignal(SIGPIPE, SigPipe);

    orig_GC_warn_proc = GC_get_warn_proc();
    GC_set_warn_proc(wrap_GC_warn_proc);

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
            char* url;
            int retry = 0;

            url = load_argv[i];
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
                rFrame();
        }
        if (w3m_dump)
            do_dump(Currentbuf);
        else {
            Currentbuf = newbuf;
            saveBufferInfo();
        }
    }
    if (w3m_dump) {
        if (err_msg->length)
            fprintf(stderr, "%s", err_msg->ptr);
        save_cookies();
        w3m_exit(0);
    }

    // if (add_download_list) {
    //     add_download_list = FALSE;
    //     CurrentTab = LastTab;
    //     if (!FirstTab) {
    //         FirstTab = LastTab = CurrentTab = newTab();
    //         nTab = 1;
    //     }
    //     if (!Firstbuf || Firstbuf == NO_BUFFER) {
    //         Firstbuf = Currentbuf = newBuffer(INIT_BUFFER_WIDTH);
    //         Currentbuf->bufferprop = BP_INTERNAL | BP_NO_URL;
    //         Currentbuf->buffername = DOWNLOAD_LIST_TITLE;
    //     } else
    //         Currentbuf = Firstbuf;
    //     ldDL();
    // } else
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

    return line_str;
}
