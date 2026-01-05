#include "maparea.h"
#include "func.h"
#include "menu.h"
#include "parsetag.h"
#include "frame.h"
#include "cookie.h"
#include "indep.h"
#include "alloc.h"
#include "ssl_stream.h"
#include "ftp.h"
#include "etc.h"
#include "content.h"
#include "local_cgi.h"
#include "mailcap.h"
#include "file.h"
#include "message.h"
#include "linein.h"
#include "funcheader.h"
#include "history.h"
#include "search.h"
#include "html_form.h"
#include "siteconf.h"
#include "anchor.h"
#include "w3m_rc.h"
#include "tab.h"
#include "buffer.h"
#include "image.h"
#include <libwc/conv.h>
#include <libwc/ces.h>
#include <libwc/status.h>
#include <string.h>
#include <locale.h>
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <time.h>
#include "display.h"
#include "myctype.h"
#include "funcname1.h"
#include "mysignal.h"

#include <libwc/wtf.h>
#include <libwc/ucs.h>
#include <libwc/charset.h>

#define PIPEBUFFERNAME "*stream*"
#define CPIPEBUFFERNAME "*stream(closed)*"
#define DICTBUFFERNAME "*dictionary*"
#define NO_TABBUFFER ((struct TabBuffer*)1)

#define DSTR_LEN 256

int show_params_p = 0;
void show_params(FILE* fp);

static void followTab(struct TabBuffer* tab);
static void moveTab(struct TabBuffer* t, struct TabBuffer* t2, int right);

#define help() fusage(stdout, 0)
#define usage() fusage(stderr, 1)

static void
fversion(FILE* f)
{
    fprintf(f, "w3m version %s, options %s\n", w3m_version,
        "lang=en"
        ",m17n"
        ",image"
        ",color"
        ",ansi-color"
        ",menu"
        ",cookie"
        ",ssl"
        ",ssl-verify"
        ",external-uri-loader"
        ",ipv6"
        ",alarm"
        ",mark");
}

static void
fusage(FILE* f, int err)
{
    fversion(f);
    /* FIXME: gettextize? */
    fprintf(f, "usage: w3m [options] [URL or content.filename]\noptions:\n");
    fprintf(f, "    -t tab           set tab width\n");
    fprintf(f, "    -r               ignore backspace effect\n");
    fprintf(f, "    -l line          # of preserved line (default 10000)\n");
    fprintf(f, "    -I charset       document charset\n");
    fprintf(f, "    -O charset       display/output charset\n");
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
    fprintf(f, "    -cookie          use cookie (-no-cookie: don't use cookie)\n");
    fprintf(f, "    -graph           use DEC special graphics for border of table and menu\n");
    fprintf(f, "    -no-graph        use ASCII character for border of table and menu\n");
    fprintf(f, "    -s               squeeze multiple blank lines\n");
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
    if (fmInitialized()) {
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
                // sleep_till_anykey(1, 1);
            }

            lock = 0;
        }
    } else if (orig_GC_warn_proc)
        orig_GC_warn_proc(msg, arg);
    else
        fprintf(stderr, msg, (unsigned long)arg);
}

static Str
make_optional_header_string(char* s)
{
    if (strchr(s, '\n') || strchr(s, '\r'))
        return NULL;

    const char* p;
    for (p = s; *p && *p != ':'; p++)
        ;
    if (*p != ':' || p == s)
        return NULL;

    Str hs = Strnew_size(strlen(s) + 3);
    Strcopy_charp_n(hs, s, p - s);
    if (!Strcasecmp_charp(hs, "content-type"))
        getRuntime()->override_content_type = TRUE;
    if (!Strcasecmp_charp(hs, "user-agent"))
        getRuntime()->override_user_agent = TRUE;
    Strcat_charp(hs, ": ");
    if (*(++p)) { /* not null header */
        p = skip_blanks(p); /* skip white spaces */
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

/// return ture if enter main loop
bool w3m_args(int argc, char** argv)
{
    if (!tty_init_termcap()) {
        return false;
    }

    struct Buffer* newbuf = NULL;
    char* p;
    // int c;
    int i;
    char* line_str = NULL;
    char** load_argv;
    struct FormList* request;
    int load_argc = 0;
    int load_bookmark = FALSE;
    int visual_start = FALSE;
    int open_new_tab = FALSE;
    // char search_header = FALSE;
    char* default_type = NULL;
    char* post_file = NULL;
    Str err_msg;
    char* Locale = NULL;
    wc_uint8 auto_detect;
    if (!getenv("GC_LARGE_ALLOC_WARN_INTERVAL"))
        set_environ("GC_LARGE_ALLOC_WARN_INTERVAL", "30000");
    GC_INIT();
    GC_oom_fn = die_oom;

    setlocale(LC_ALL, "");

    getRuntime()->fileToDelete = newTextList();

    load_argv = New_N(char*, argc - 1);
    load_argc = 0;

    getRuntime()->CurrentDir = currentdir();
    getRuntime()->CurrentPid = (int)getpid();
    getRuntime()->BookmarkFile = NULL;
    getRuntime()->config_file = NULL;

    {
        char hostname[HOST_NAME_MAX + 2];
        if (gethostname(hostname, HOST_NAME_MAX + 2) == 0) {
            size_t hostname_len;
            /* Don't use hostname if it is truncated.  */
            hostname[HOST_NAME_MAX + 1] = '\0';
            hostname_len = strlen(hostname);
            if (hostname_len <= HOST_NAME_MAX && hostname_len < STR_SIZE_MAX)
                getRuntime()->HostName = allocStr(hostname, (int)hostname_len);
        }
    }

    /* argument search 1 */
    for (i = 1; i < argc; i++) {
        if (*argv[i] == '-') {
            if (!strcmp("-config", argv[i])) {
                argv[i] = "-dummy";
                if (++i >= argc)
                    usage();
                getRuntime()->config_file = argv[i];
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
        getRuntime()->DisplayCharset = wc_guess_locale_charset(Locale, getRuntime()->DisplayCharset);
        getRuntime()->DocumentCharset = wc_guess_locale_charset(Locale, getRuntime()->DocumentCharset);
        getRuntime()->SystemCharset = wc_guess_locale_charset(Locale, getRuntime()->SystemCharset);
    }

    /* initializations */
    init_rc();

    if (getRuntime()->FollowLocale && Locale) {
        getRuntime()->DisplayCharset = wc_guess_locale_charset(Locale, getRuntime()->DisplayCharset);
        getRuntime()->SystemCharset = wc_guess_locale_charset(Locale, getRuntime()->SystemCharset);
    }
    auto_detect = WcOption.auto_detect;
    getRuntime()->BookmarkCharset = getRuntime()->DocumentCharset;

    if (!non_null(getRuntime()->HTTP_proxy) && ((p = getenv("HTTP_PROXY")) || (p = getenv("http_proxy")) || (p = getenv("HTTP_proxy"))))
        getRuntime()->HTTP_proxy = p;
    if (!non_null(getRuntime()->HTTPS_proxy) && ((p = getenv("HTTPS_PROXY")) || (p = getenv("https_proxy")) || (p = getenv("HTTPS_proxy"))))
        getRuntime()->HTTPS_proxy = p;
    if (getRuntime()->HTTPS_proxy == NULL && non_null(getRuntime()->HTTP_proxy))
        getRuntime()->HTTPS_proxy = getRuntime()->HTTP_proxy;
    if (!non_null(getRuntime()->FTP_proxy) && ((p = getenv("FTP_PROXY")) || (p = getenv("ftp_proxy")) || (p = getenv("FTP_proxy"))))
        getRuntime()->FTP_proxy = p;
    if (!non_null(getRuntime()->NO_proxy) && ((p = getenv("NO_PROXY")) || (p = getenv("no_proxy")) || (p = getenv("NO_proxy"))))
        getRuntime()->NO_proxy = p;

    if (!non_null(getRuntime()->Editor) && (p = getenv("EDITOR")) != NULL)
        getRuntime()->Editor = p;
    if (!non_null(getRuntime()->Mailer) && (p = getenv("MAILER")) != NULL)
        getRuntime()->Mailer = p;

    /* argument search 2 */
    i = 1;
    while (i < argc) {
        if (*argv[i] == '-') {
            if (!strcmp("-t", argv[i])) {
                if (++i >= argc)
                    usage();
                if (atoi(argv[i]) > 0)
                    getRuntime()->Tabstop = atoi(argv[i]);
            } else if (!strcmp("-r", argv[i]))
                getRuntime()->ShowEffect = false;
            else if (!strcmp("-l", argv[i])) {
                if (++i >= argc)
                    usage();
                if (atoi(argv[i]) > 0)
                    getRuntime()->PagerMax = atoi(argv[i]);
            } else if (!strncmp("-I", argv[i], 2)) {
                if (argv[i][2] != '\0')
                    p = argv[i] + 2;
                else {
                    if (++i >= argc)
                        usage();
                    p = argv[i];
                }
                getRuntime()->DocumentCharset = wc_guess_charset_short(p, getRuntime()->DocumentCharset);
                WcOption.auto_detect = WC_OPT_DETECT_OFF;
                getRuntime()->UseContentCharset = FALSE;
            } else if (!strncmp("-O", argv[i], 2)) {
                if (argv[i][2] != '\0')
                    p = argv[i] + 2;
                else {
                    if (++i >= argc)
                        usage();
                    p = argv[i];
                }
                getRuntime()->DisplayCharset = wc_guess_charset_short(p, getRuntime()->DisplayCharset);
            } else if (!strcmp("-graph", argv[i]))
                UseGraphicChar = GRAPHIC_CHAR_DEC;
            else if (!strcmp("-no-graph", argv[i]))
                UseGraphicChar = GRAPHIC_CHAR_ASCII;
            else if (!strcmp("-T", argv[i])) {
                if (++i >= argc)
                    usage();
                getRuntime()->DefaultType = default_type = argv[i];
            }
            // else if (!strcmp("-m", argv[i]))
            //     SearchHeader = search_header = TRUE;
            else if (!strcmp("-v", argv[i]))
                visual_start = TRUE;
            else if (!strcmp("-N", argv[i]))
                open_new_tab = TRUE;

            else if (!strcmp("-M", argv[i]))
                getRuntime()->useColor = FALSE;
            else if (!strcmp("-H", argv[i]))
                getRuntime()->highIntensityColors = TRUE;

            else if (!strcmp("-B", argv[i]))
                load_bookmark = TRUE;
            else if (!strcmp("-bookmark", argv[i])) {
                if (++i >= argc)
                    usage();
                getRuntime()->BookmarkFile = argv[i];
                if (getRuntime()->BookmarkFile[0] != '~' && getRuntime()->BookmarkFile[0] != '/') {
                    Str tmp = Strnew_charp(getRuntime()->CurrentDir);
                    if (Strlastchar(tmp) != '/')
                        Strcat_char(tmp, '/');
                    Strcat_charp(tmp, getRuntime()->BookmarkFile);
                    getRuntime()->BookmarkFile = cleanupName(tmp->ptr);
                }
            } else if (!strcmp("-F", argv[i]))
                getRuntime()->RenderFrame = TRUE;
            else if (!strcmp("-W", argv[i])) {
                if (getRuntime()->WrapDefault)
                    getRuntime()->WrapDefault = FALSE;
                else
                    getRuntime()->WrapDefault = TRUE;
            } else if (!strcmp("-cols", argv[i])) {
                if (++i >= argc)
                    usage();
                tty_set_cols(atoi(argv[i]));
            } else if (!strcmp("-ppc", argv[i])) {
                double ppc;
                if (++i >= argc)
                    usage();
                ppc = atof(argv[i]);
                if (ppc >= MINIMUM_PIXEL_PER_CHAR && ppc <= MAXIMUM_PIXEL_PER_CHAR) {
                    getRuntime()->pixel_per_char = ppc;
                    getRuntime()->set_pixel_per_char = true;
                }
            }

            else if (!strcmp("-ppl", argv[i])) {
                double ppc;
                if (++i >= argc)
                    usage();
                ppc = atof(argv[i]);
                if (ppc >= MINIMUM_PIXEL_PER_CHAR && ppc <= MAXIMUM_PIXEL_PER_CHAR * 2) {
                    getRuntime()->pixel_per_line = ppc;
                    getRuntime()->set_pixel_per_line = TRUE;
                }
            }

            else if (!strcmp("-ri", argv[i])) {
                getRuntime()->enable_inline_image = INLINE_IMG_OSC5379;
            } else if (!strcmp("-sixel", argv[i])) {
                getRuntime()->enable_inline_image = INLINE_IMG_SIXEL;
            } else if (!strcmp("-num", argv[i]))
                getRuntime()->showLineNum = TRUE;
            else if (!strcmp("-no-proxy", argv[i]))
                getRuntime()->use_proxy = FALSE;
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
                    if (getRuntime()->header_string == NULL)
                        getRuntime()->header_string = hs;
                    else
                        Strcat(getRuntime()->header_string, hs);
                }
                while (argv[i][0]) {
                    argv[i][0] = '\0';
                    argv[i]++;
                }
            } else if (!strcmp("-no-cookie", argv[i])) {
                getRuntime()->use_cookie = FALSE;
                getRuntime()->accept_cookie = FALSE;
            } else if (!strcmp("-cookie", argv[i])) {
                getRuntime()->use_cookie = TRUE;
                getRuntime()->accept_cookie = TRUE;
            } else if (!strcmp("-s", argv[i]))
                getRuntime()->squeezeBlankLine = TRUE;
            else if (!strcmp("-X", argv[i]))
                getRuntime()->Do_not_use_ti_te = TRUE;
            else if (!strcmp("-title", argv[i]))
                getRuntime()->displayTitleTerm = getenv("TERM");
            else if (!strncmp("-title=", argv[i], 7))
                getRuntime()->displayTitleTerm = argv[i] + 7;
            else if (!strcmp("-insecure", argv[i])) {
                set_param_option("ssl_cipher=ALL:eNULL");
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
            }
            // else if (!strcmp("-reqlog", argv[i])) {
            //     w3m_reqlog = rcFile("request.log");
            // }
            // #if defined(DONT_CALL_GC_AFTER_FORK) && defined(USE_IMAGE)
            //             else if (!strcmp("-$$getimage", argv[i])) {
            //                 ++i;
            //                 getimage_args = argv + i;
            //                 i += 4;
            //                 if (i > argc)
            //                     usage();
            //             }
            // #endif /* defined(DONT_CALL_GC_AFTER_FORK) && defined(USE_IMAGE) */
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

    if (getRuntime()->BookmarkFile == NULL)
        getRuntime()->BookmarkFile = rcFile(BOOKMARK);

    enterRawMode();

    sync_with_option();

    initCookie();

    // mySignal(SIGPIPE, SigPipe);

    orig_GC_warn_proc = GC_get_warn_proc();
    GC_set_warn_proc(wrap_GC_warn_proc);
    err_msg = Strnew();
    if (load_argc == 0) {
        /* no URL specified */
        if (isatty(0) == 0) {
            // not impl
            assert(false);
            // redin = newFileStream(fdopen(dup(0), "rb"), (void (*)())pclose);
            // newbuf = openGeneralPagerBuffer(redin);
            // dup2(1, 0);
        } else if (load_bookmark) {
            struct Content *content = get_content_cache(getRuntime()->BookmarkFile, NULL,
                (struct LoadOption) { .base_url = NULL, .referer = NO_REFERER, .flag = 0 });
            newbuf = newBuffer(INIT_BUFFER_WIDTH);
            newbuf->content = content;
            if (newbuf == NULL)
                Strcat_charp(err_msg, "w3m: Can't load bookmark.\n");
        } else if (visual_start) {
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
            else
                newbuf->bufferprop |= (BP_INTERNAL | BP_NO_URL);
        } else if ((p = getenv("HTTP_HOME")) != NULL || (p = getenv("WWW_HOME")) != NULL) {
            struct Content *content = get_content_cache(p, NULL,
                (struct LoadOption) { .base_url = NULL, .referer = NO_REFERER, .flag = 0 });
            newbuf = newBuffer(INIT_BUFFER_WIDTH);
            newbuf->content = content;
            if (newbuf == NULL)
                Strcat(err_msg, Sprintf("w3m: Can't load %s.\n", p));
            else
                pushHashHist(getRuntime()->URLHist, parsedURL2Str(&newbuf->content->url)->ptr);
        } else {
            if (fmInitialized())
                exitRawMode();
            usage();
        }
        if (newbuf == NULL) {
            if (fmInitialized())
                exitRawMode();
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
            // SearchHeader = search_header;
            getRuntime()->DefaultType = default_type;
            int retry = 0;
            const char* url = load_argv[i];
            if (getURLScheme(&url) == SCM_MISSING && !getRuntime()->ArgvIsURL)
            retry_as_local_file:
                url = file_to_url(load_argv[i]);
            else
                url = url_encode(conv_from_system(load_argv[i]), NULL, 0);
            {
                if (post_file && i == 0) {
                    FILE* fp;
                    Str body;
                    if (!strcmp(post_file, "-"))
                        fp = stdin;
                    else
                        fp = fopen(post_file, "r");
                    if (fp == NULL) {
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
                struct Content *content = get_content_cache(url, request,
                    (struct LoadOption) { .base_url = NULL, .referer = NO_REFERER, .flag = 0 });
                newbuf = newBuffer(INIT_BUFFER_WIDTH);
                newbuf->content = content;
            }
            if (newbuf == NULL) {
                if (getRuntime()->ArgvIsURL && !retry) {
                    retry = 1;
                    goto retry_as_local_file;
                }
                /* FIXME: gettextize? */
                Strcat(err_msg,
                    Sprintf("w3m: Can't load %s.\n", load_argv[i]));
                continue;
            }
        }
        if (CurrentTab() == NULL) {
            getRuntime()->FirstTab = getRuntime()->LastTab = getRuntime()->CurrentTab = newTab();
            if (!FirstTab()) {
                fprintf(stderr, "%s\n", "Can't allocated memory");
                exit(1);
            }
            getRuntime()->nTab = 1;
            Firstbuf = Currentbuf = newbuf;
        } else if (open_new_tab) {
            _newT();
            Currentbuf->nextBuffer = newbuf;
            delBuffer(Currentbuf);
        } else {
            Currentbuf->nextBuffer = newbuf;
            Currentbuf = newbuf;
        }
        assert(Currentbuf);
        assert(Firstbuf);

        Currentbuf = newbuf;
    }

    getRuntime()->CurrentTab = FirstTab();

    if (!FirstTab() || !Firstbuf) {
        if (fmInitialized())
            exitRawMode();
        if (err_msg->length)
            fprintf(stderr, "%s", err_msg->ptr);
        w3m_exit(2);
    }

    if (err_msg->length)
        disp_message_nsec(err_msg->ptr, FALSE, 1, TRUE, FALSE);

    // SearchHeader = FALSE;
    getRuntime()->DefaultType = NULL;
    getRuntime()->UseContentCharset = TRUE;
    WcOption.auto_detect = auto_detect;

    Currentbuf = Firstbuf;
    screen_from_lines(&Currentbuf->doc, baseURL(Currentbuf));
    if (line_str) {
        doc_goLine(&Currentbuf->doc, line_str);
    }

    return true;
}

static void
dump_source(struct Buffer* buf)
{
    FILE* f;
    int c;
    if (buf->content->sourcefile == NULL)
        return;
    f = fopen(buf->content->sourcefile, "r");
    if (f == NULL)
        return;
    while ((c = fgetc(f)) != EOF) {
        putchar(c);
    }
    fclose(f);
}

void tmpClearBuffer(struct Buffer* buf)
{
    if (writeBufferCache(buf) == 0) {
        buf->doc.firstLine = NULL;
        buf->doc.topLine = NULL;
        buf->doc.currentLine = NULL;
        buf->doc.lastLine = NULL;
    }
}

static Str currentURL(void);

static void
repBuffer(struct Buffer* oldbuf, struct Buffer* buf)
{
    Firstbuf = replaceBuffer(Firstbuf, oldbuf, buf);
    Currentbuf = buf;
}

/*
 * Command functions: These functions are called with a keystroke.
 */

static void
cmd_loadURL(const char* url, struct FormList* request, struct LoadOption option)
{
    if (handleMailto(url))
        return;

    struct Content *content = get_content_cache(url, request, option);
    struct Buffer* buf = newBuffer(INIT_BUFFER_WIDTH);
    buf->content = content;
    if (buf == NULL) {
        char* emsg = Sprintf("Can't load %s", conv_from_system(url))->ptr;
        disp_err_message(emsg, FALSE);
    } else {
        tab_push_buffer(getRuntime()->CurrentTab, buf);
        // if (getRuntime()->RenderFrame && Currentbuf->doc.frameset != NULL)
        //     rFrame(ctx);
    }
}

/* go to specified URL */
static void
goURL0(const char* prompt, int relative)
{
    const char *url, *referer;
    struct Url p_url, *current;
    struct Buffer* cur_buf = Currentbuf;
    const int* no_referer_ptr;

    url = searchKeyData();
    if (url == NULL) {
        struct Hist* hist = copyHist(getRuntime()->URLHist);
        struct Anchor* a;

        current = baseURL(Currentbuf);
        if (current) {
            char* c_url = parsedURL2Str(current)->ptr;
            if (getRuntime()->DefaultURLString == DEFAULT_URL_CURRENT)
                url = url_decode2(c_url, NULL);
            else
                pushHist(hist, c_url);
        }
        a = doc_retrieveCurrentAnchor(&Currentbuf->doc);
        if (a) {
            char* a_url;
            parseURL2(a->url, &p_url, current);
            a_url = parsedURL2Str(&p_url)->ptr;
            if (getRuntime()->DefaultURLString == DEFAULT_URL_LINK)
                url = url_decode2(a_url, Currentbuf);
            else
                pushHist(hist, a_url);
        }
        url = inputLineHist(prompt, url, IN_URL, hist);
        if (url != NULL)
            url = skip_blanks(url);
    }
    if (relative) {
        no_referer_ptr = query_SCONF_NO_REFERER_FROM(&Currentbuf->content->url);
        current = baseURL(Currentbuf);
        if ((no_referer_ptr && *no_referer_ptr) || current == NULL || current->scheme == SCM_LOCAL || current->scheme == SCM_LOCAL_CGI)
            referer = NO_REFERER;
        else
            referer = parsedURL2RefererStr(&Currentbuf->content->url)->ptr;
        url = url_encode(url, current, Currentbuf->doc.charset);
    } else {
        current = NULL;
        referer = NULL;
        url = url_encode(url, NULL, 0);
    }
    if (url == NULL || *url == '\0') {
        return;
    }
    if (*url == '#') {
        gotoLabel(Currentbuf, url + 1);
        return;
    }
    parseURL2(url, &p_url, current);
    pushHashHist(getRuntime()->URLHist, parsedURL2Str(&p_url)->ptr);
    cmd_loadURL(url, NULL, (struct LoadOption) { .base_url = current, .referer = referer });
    if (Currentbuf != cur_buf) /* success */
        pushHashHist(getRuntime()->URLHist, parsedURL2Str(&Currentbuf->content->url)->ptr);
}

DEFUN(goURL, GOTO, "Open specified document in a new buffer")
{
    goURL0("Goto URL: ", FALSE);
}

DEFUN(goHome, GOTO_HOME, "Open home page in a new buffer")
{
    const char* url;
    if ((url = getenv("HTTP_HOME")) != NULL || (url = getenv("WWW_HOME")) != NULL) {
        struct Url p_url;
        struct Buffer* cur_buf = Currentbuf;
        url = skip_blanks(url);
        url = url_encode(url, NULL, 0);
        parseURL2(url, &p_url, NULL);
        pushHashHist(getRuntime()->URLHist, parsedURL2Str(&p_url)->ptr);
        cmd_loadURL(url, NULL, (struct LoadOption) { .base_url = NULL, .referer = NULL });
        if (Currentbuf != cur_buf) /* success */
            pushHashHist(getRuntime()->URLHist, parsedURL2Str(&Currentbuf->content->url)->ptr);
    }
}

DEFUN(gorURL, GOTO_RELATIVE, "Go to relative address")
{
    goURL0("Goto relative URL: ", TRUE);
}

/* load bookmark */
DEFUN(ldBmark, BOOKMARK VIEW_BOOKMARK, "View bookmarks")
{
    cmd_loadURL(getRuntime()->BookmarkFile, NULL,
        (struct LoadOption) { .base_url = NULL, .referer = NO_REFERER });
}

/* Add current to bookmark */
DEFUN(adBmark, ADD_BOOKMARK, "Add current page to bookmarks")
{
    Str tmp;
    struct FormList* request;

    tmp = Sprintf("mode=panel&cookie=%s&bmark=%s&url=%s&title=%s"
                  "&charset=%s",
        (Str_form_quote(localCookie()))->ptr,
        (Str_form_quote(Strnew_charp(getRuntime()->BookmarkFile)))->ptr,
        (Str_form_quote(parsedURL2Str(&Currentbuf->content->url)))->ptr,

        (Str_form_quote(wc_conv_strict(Currentbuf->doc.title,
             getRuntime()->InnerCharset,
             getRuntime()->BookmarkCharset)))
            ->ptr,
        wc_ces_to_charset(getRuntime()->BookmarkCharset));

    request = newFormList(NULL, "post", NULL, NULL, NULL, NULL, NULL);
    request->body = tmp->ptr;
    request->length = tmp->length;
    cmd_loadURL("file:///$LIB/" W3MBOOKMARK_CMDNAME, request,
        (struct LoadOption) { .base_url = NULL, .referer = NO_REFERER });
}

/* option setting */
DEFUN(ldOpt, OPTIONS, "Display options setting panel")
{
    cmd_loadBuffer(load_option_panel(), BP_NO_URL, LB_NOLINK);
}

/* set an option */
DEFUN(setOpt, SET_OPTION, "Set option")
{
    getRuntime()->CurrentKeyData = NULL; /* not allowed in w3m-control: */
    char* opt = searchKeyData();
    if (opt == NULL || *opt == '\0' || strchr(opt, '=') == NULL) {
        if (opt != NULL && *opt != '\0') {
            char* v = get_param_option(opt);
            opt = Sprintf("%s=%s", opt, v ? v : "")->ptr;
        }
        opt = inputStrHist("Set option: ", opt, getRuntime()->TextHist);
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
    struct Buffer* buf;

    if ((buf = Currentbuf->linkBuffer[LB_N_INFO]) != NULL) {
        Currentbuf = buf;
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
    int x, y;
    struct Url p_url;

    struct Anchor* an = doc_retrieveCurrentImg(&Currentbuf->doc);
    x = Currentbuf->doc.cursorX + Currentbuf->doc.rootX;
    y = Currentbuf->doc.cursorY + Currentbuf->doc.rootY;
    struct MapArea* a = follow_map_menu(Currentbuf, name, an, x, y);
    if (a == NULL || a->url == NULL || *(a->url) == '\0') {
        return;
    }
    if (*(a->url) == '#') {
        gotoLabel(Currentbuf, a->url + 1);
        return;
    }
    parseURL2(a->url, &p_url, baseURL(Currentbuf));
    pushHashHist(getRuntime()->URLHist, parsedURL2Str(&p_url)->ptr);
    if (getRuntime()->check_target
        && getRuntime()->open_tab_blank
        && a->target
        && (!strcasecmp(a->target, "_new") || !strcasecmp(a->target, "_blank"))) {
        _newT();
        struct Buffer* buf = Currentbuf;
        cmd_loadURL(a->url, NULL,
            (struct LoadOption) {
                .base_url = baseURL(Currentbuf),
                .referer = parsedURL2Str(&Currentbuf->content->url)->ptr });
        if (buf != Currentbuf)
            delBuffer(buf);
        else
            deleteTab(CurrentTab());
        return;
    }
    cmd_loadURL(a->url, NULL,
        (struct LoadOption) {
            .base_url = baseURL(Currentbuf),
            .referer = parsedURL2Str(&Currentbuf->content->url)->ptr });
}

/* link menu */
DEFUN(linkMn, LINK_MENU, "Pop up link element menu")
{
    struct LinkList* l = link_menu(Currentbuf);
    struct Url p_url;

    if (!l || !l->url)
        return;
    if (*(l->url) == '#') {
        gotoLabel(Currentbuf, l->url + 1);
        return;
    }
    parseURL2(l->url, &p_url, baseURL(Currentbuf));
    pushHashHist(getRuntime()->URLHist, parsedURL2Str(&p_url)->ptr);
    cmd_loadURL(l->url, NULL,
        (struct LoadOption) {
            .base_url = baseURL(Currentbuf),
            .referer = parsedURL2Str(&Currentbuf->content->url)->ptr });
}

static void
anchorMn(BufferMenuFunc menu_func, bool go)
{
    if (!Currentbuf->doc.href || !Currentbuf->doc.hmarklist)
        return;

    struct Anchor* a = menu_func(Currentbuf);
    if (!a || a->hseq < 0)
        return;

    struct BufferPoint* po = &Currentbuf->doc.hmarklist->marks[a->hseq];
    doc_gotoLine(&Currentbuf->doc, po->line);
    Currentbuf->doc.pos = po->pos;
    doc_arrangeCursor(&Currentbuf->doc);
    if (go)
        followA((struct DefunContext) { 0 });
}

/* accesskey */
DEFUN(accessKey, ACCESSKEY, "Pop up accesskey menu")
{
    anchorMn(accesskey_menu, TRUE);
}

/* list menu */
DEFUN(listMn, LIST_MENU, "Pop up menu for hyperlinks to browse to")
{
    anchorMn(list_menu, TRUE);
}

DEFUN(movlistMn, MOVE_LIST_MENU, "Pop up menu to navigate between hyperlinks")
{
    anchorMn(list_menu, FALSE);
}

/* link,anchor,image list */
DEFUN(linkLst, LIST, "Show all URLs referenced")
{
    struct Buffer* buf;

    buf = link_list_panel(Currentbuf);
    if (buf != NULL) {
        buf->doc.charset = Currentbuf->doc.charset;
        cmd_loadBuffer(buf, BP_NORMAL, LB_NOLINK);
    }
}

/* cookie list */
DEFUN(cooLst, COOKIE, "View cookie list")
{
    struct Buffer* buf;

    buf = cookie_list_panel();
    if (buf != NULL)
        cmd_loadBuffer(buf, BP_NO_URL, LB_NOLINK);
}

/* History page */
DEFUN(ldHist, HISTORY, "Show browsing history")
{
    cmd_loadBuffer(historyBuffer(getRuntime()->URLHist), BP_NO_URL, LB_NOLINK);
}

/* download HREF link */
DEFUN(svA, SAVE_LINK, "Save hyperlink target")
{
    getRuntime()->CurrentKeyData = NULL; /* not allowed in w3m-control: */
    _followA(Currentbuf, (struct FollowOption) { .on_target = true, .do_download = false });
}

/* save buffer */
DEFUN(svBuf, PRINT SAVE_SCREEN, "Save rendered document")
{
    getRuntime()->CurrentKeyData = NULL; /* not allowed in w3m-control: */

    char* file = searchKeyData();
    char* qfile = NULL;
    if (file == NULL || *file == '\0') {
        /* FIXME: gettextize? */
        qfile = inputLineHist("Save buffer to: ", NULL, IN_COMMAND, getRuntime()->SaveHist);
        if (qfile == NULL || *qfile == '\0') {
            return;
        }
    }
    file = conv_to_system(qfile ? qfile : file);

    FILE* f;
    bool is_pipe;
    if (*file == '|') {
        is_pipe = TRUE;
        f = popen(file + 1, "w");
    } else {
        if (qfile) {
            file = unescape_spaces(Strnew_charp(qfile))->ptr;
            file = conv_to_system(file);
        }
        file = expandPath(file);
        if (checkOverWrite(file) < 0) {
            return;
        }
        f = fopen(file, "w");
        is_pipe = FALSE;
    }
    if (f == NULL) {
        /* FIXME: gettextize? */
        char* emsg = Sprintf("Can't open %s", conv_from_system(file))->ptr;
        disp_err_message(emsg, TRUE);
        return;
    }
    saveBuffer(Currentbuf, f, TRUE);
    if (is_pipe)
        pclose(f);
    else
        fclose(f);
}

/* save source */
DEFUN(svSrc, DOWNLOAD SAVE, "Save document source")
{
    if (Currentbuf->content->sourcefile == NULL)
        return;
    getRuntime()->CurrentKeyData = NULL; /* not allowed in w3m-control: */
    getRuntime()->PermitSaveToPipe = TRUE;
    const char* file;
    if (Currentbuf->content->url.scheme == SCM_LOCAL)
        file = conv_from_system(guess_save_name(NULL,
            Currentbuf->content->url.real_file));
    else
        file = guess_save_name(Currentbuf->content, Currentbuf->content->url.file);
    doFileCopy(Currentbuf->content->sourcefile, file);
    getRuntime()->PermitSaveToPipe = FALSE;
}

static void
_peekURL(int only_img)
{

    struct Anchor* a;
    struct Url pu;
    static Str s = NULL;
    static Lineprop* p = NULL;
    Lineprop* pp;

    static int offset = 0, n;

    if (Currentbuf->doc.firstLine == NULL)
        return;

    if (getRuntime()->CurrentKey == getRuntime()->prev_key && s != NULL) {
        if (s->length - offset >= TTY_COLS())
            offset++;
        else if (s->length <= offset) /* bug ? */
            offset = 0;
        goto disp;
    } else {
        offset = 0;
    }
    s = NULL;
    a = (only_img ? NULL : doc_retrieveCurrentAnchor(&Currentbuf->doc));
    if (a == NULL) {
        a = (only_img ? NULL : doc_retrieveCurrentForm(&Currentbuf->doc));
        if (a == NULL) {
            a = doc_retrieveCurrentImg(&Currentbuf->doc);
            if (a == NULL)
                return;
        } else
            s = Strnew_charp(form2str((struct FormItemList*)a->url));
    }
    if (s == NULL) {
        parseURL2(a->url, &pu, baseURL(Currentbuf));
        s = parsedURL2Str(&pu);
    }
    if (getRuntime()->DecodeURL)
        s = Strnew_charp(url_decode2(s->ptr, Currentbuf));
    s = checkType(s, &pp, NULL);
    p = NewAtom_N(Lineprop, s->length);
    bcopy((void*)pp, (void*)p, s->length * sizeof(Lineprop));
disp:
    n = searchKeyNum();
    if (n > 1 && s->length > (n - 1) * (TTY_COLS() - 1))
        offset = (n - 1) * (TTY_COLS() - 1);
    while (offset < s->length && p[offset] & PC_WCHAR2)
        offset++;
    disp_message(&s->ptr[offset], TRUE);
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
    return parsedURL2Str(&Currentbuf->content->url);
}

DEFUN(curURL, PEEK, "Show current address")
{
    static Str s = NULL;
    static Lineprop* p = NULL;
    Lineprop* pp;

    static int offset = 0, n;

    if (Currentbuf->bufferprop & BP_INTERNAL)
        return;
    if (getRuntime()->CurrentKey == getRuntime()->prev_key && s != NULL) {
        if (s->length - offset >= TTY_COLS())
            offset++;
        else if (s->length <= offset) /* bug ? */
            offset = 0;
    } else {
        offset = 0;
        s = currentURL();
        if (getRuntime()->DecodeURL)
            s = Strnew_charp(url_decode2(s->ptr, NULL));
        s = checkType(s, &pp, NULL);
        p = NewAtom_N(Lineprop, s->length);
        bcopy((void*)pp, (void*)p, s->length * sizeof(Lineprop));
    }
    n = searchKeyNum();
    if (n > 1 && s->length > (n - 1) * (TTY_COLS() - 1))
        offset = (n - 1) * (TTY_COLS() - 1);
    while (offset < s->length && p[offset] & PC_WCHAR2)
        offset++;
    disp_message(&s->ptr[offset], TRUE);
}
/* view HTML source */

DEFUN(vwSrc, SOURCE VIEW, "Toggle between HTML shown or processed")
{
    struct Buffer* buf;

    if (Currentbuf->content->content_type == NULL || Currentbuf->bufferprop & BP_FRAME)
        return;
    if ((buf = Currentbuf->linkBuffer[LB_SOURCE]) != NULL || (buf = Currentbuf->linkBuffer[LB_N_SOURCE]) != NULL) {
        Currentbuf = buf;
        return;
    }
    if (Currentbuf->content->sourcefile == NULL) {
        // if (Currentbuf->pagerSource && !strcasecmp(Currentbuf->type, "text/plain")) {
        //     wc_ces old_charset;
        //     wc_bool old_fix_width_conv;
        //
        //     FILE* f;
        //     Str tmpf = tmpfname(TMPF_SRC, NULL);
        //     f = fopen(tmpf->ptr, "w");
        //     if (f == NULL)
        //         return;
        //
        //     old_charset = getRuntime()->DisplayCharset;
        //     old_fix_width_conv = WcOption.fix_width_conv;
        //     getRuntime()->DisplayCharset = (Currentbuf->document_charset != WC_CES_US_ASCII)
        //         ? Currentbuf->document_charset
        //         : 0;
        //     WcOption.fix_width_conv = WC_FALSE;
        //
        //     saveBufferBody(Currentbuf, f, TRUE);
        //
        //     getRuntime()->DisplayCharset = old_charset;
        //     WcOption.fix_width_conv = old_fix_width_conv;
        //
        //     fclose(f);
        //     Currentbuf->sourcefile = tmpf->ptr;
        // }
        // else
        {
            return;
        }
    }

    buf = newBuffer(INIT_BUFFER_WIDTH);

    if (is_html_type(Currentbuf->content->content_type)) {
        buf->content->content_type = "text/plain";
        if (Currentbuf->content->content_type && is_html_type(Currentbuf->content->content_type))
            buf->content->content_type = "text/plain";
        else
            buf->content->content_type = Currentbuf->content->content_type;
        buf->doc.title = Sprintf("source of %s", Currentbuf->doc.title)->ptr;
        buf->linkBuffer[LB_N_SOURCE] = Currentbuf;
        Currentbuf->linkBuffer[LB_SOURCE] = buf;
    } else if (!strcasecmp(Currentbuf->content->content_type, "text/plain")) {
        buf->content->content_type = "text/html";
        if (Currentbuf->content->content_type && !strcasecmp(Currentbuf->content->content_type, "text/plain"))
            buf->content->content_type = "text/html";
        else
            buf->content->content_type = Currentbuf->content->content_type;
        buf->doc.title = Sprintf("HTML view of %s",
            Currentbuf->doc.title)
                             ->ptr;
        buf->linkBuffer[LB_SOURCE] = Currentbuf;
        Currentbuf->linkBuffer[LB_N_SOURCE] = buf;
    } else {
        return;
    }
    buf->content->url = Currentbuf->content->url;
    buf->content->filename = Currentbuf->content->filename;
    buf->content->sourcefile = Currentbuf->content->sourcefile;
    buf->content->header_source = Currentbuf->content->header_source;
    // buf->search_header = Currentbuf->search_header;
    buf->doc.charset = Currentbuf->doc.charset;
    buf->clone = Currentbuf->clone;
    (*buf->clone)++;
    reshapeBuffer(buf);
    tab_push_buffer(getRuntime()->CurrentTab, buf);
}

/* reload */
DEFUN(reload, RELOAD, "Load current document anew")
{
    struct Buffer *buf, *fbuf = NULL;
    enum wc_ces old_charset;
    Str url;
    struct FormList* request;
    int multipart;

    if (Currentbuf->bufferprop & BP_INTERNAL) {
        disp_err_message("Can't reload...", TRUE);
        return;
    }
    if (Currentbuf->content->url.scheme == SCM_LOCAL && !strcmp(Currentbuf->content->url.file, "-")) {
        /* file is std input */
        /* FIXME: gettextize? */
        disp_err_message("Can't reload stdin", TRUE);
        return;
    }
    struct Buffer sbuf;
    copyBuffer(&sbuf, Currentbuf);
    if (Currentbuf->bufferprop & BP_FRAME && (fbuf = Currentbuf->linkBuffer[LB_N_FRAME])) {
        if (fmInitialized()) {
            message("Rendering frame");
        }
        if (!(buf = renderFrame(fbuf, 1))) {
            return;
        }
        if (fbuf->linkBuffer[LB_FRAME]) {
            if (buf->content->sourcefile
                && fbuf->linkBuffer[LB_FRAME]->content->sourcefile
                && !strcmp(buf->content->sourcefile, fbuf->linkBuffer[LB_FRAME]->content->sourcefile))
                fbuf->linkBuffer[LB_FRAME]->content->sourcefile = NULL;
            delBuffer(fbuf->linkBuffer[LB_FRAME]);
        }
        fbuf->linkBuffer[LB_FRAME] = buf;
        buf->linkBuffer[LB_N_FRAME] = fbuf;
        tab_push_buffer(getRuntime()->CurrentTab, buf);
        Currentbuf = buf;
        if (Currentbuf->doc.firstLine) {
            COPY_BUFROOT(&ctx.buf->doc, &sbuf.doc);
            doc_restorePosition(&Currentbuf->doc, &sbuf.doc);
        }
        return;
    } else if (Currentbuf->doc.frameset != NULL)
        fbuf = Currentbuf->linkBuffer[LB_FRAME];
    multipart = 0;
    if (Currentbuf->doc.form_submit) {
        request = Currentbuf->doc.form_submit->parent;
        if (request->method == FORM_METHOD_POST
            && request->enctype == FORM_ENCTYPE_MULTIPART) {
            struct stat st;
            multipart = 1;
            query_from_followform(Currentbuf, Currentbuf->doc.form_submit, multipart);
            stat(request->body, &st);
            request->length = st.st_size;
        }
    } else {
        request = NULL;
    }
    url = parsedURL2Str(&Currentbuf->content->url);
    message("Reloading...");
    old_charset = getRuntime()->DocumentCharset;
    if (Currentbuf->doc.charset != WC_CES_US_ASCII)
        getRuntime()->DocumentCharset = Currentbuf->doc.charset;
    // SearchHeader = Currentbuf->search_header;
    getRuntime()->DefaultType = Currentbuf->content->content_type;
    struct Content *content = get_content_cache(url->ptr, request,
        (struct LoadOption) { .base_url = NULL, .referer = NO_REFERER, .flag = RG_NOCACHE });
    buf = newBuffer(INIT_BUFFER_WIDTH);
    buf->content = content;
    getRuntime()->DocumentCharset = old_charset;
    // SearchHeader = FALSE;
    getRuntime()->DefaultType = NULL;

    if (multipart)
        unlink(request->body);
    if (buf == NULL) {
        /* FIXME: gettextize? */
        disp_err_message("Can't reload...", TRUE);
        return;
    }
    if (fbuf != NULL)
        Firstbuf = deleteBuffer(Firstbuf, fbuf);
    repBuffer(Currentbuf, buf);
    if ((buf->content->content_type != NULL) && (sbuf.content->content_type != NULL) && ((!strcasecmp(buf->content->content_type, "text/plain") && is_html_type(sbuf.content->content_type)) || (is_html_type(buf->content->content_type) && !strcasecmp(sbuf.content->content_type, "text/plain")))) {
        vwSrc(ctx);
        if (Currentbuf != buf)
            Firstbuf = deleteBuffer(Firstbuf, buf);
    }
    // Currentbuf->search_header = sbuf.search_header;
    Currentbuf->doc.form_submit = sbuf.doc.form_submit;
    if (Currentbuf->doc.firstLine) {
        COPY_BUFROOT(&ctx.buf->doc, &sbuf.doc);
        doc_restorePosition(&Currentbuf->doc, &sbuf.doc);
    }
}

/* reshape */
DEFUN(reshape, RESHAPE, "Re-render document")
{
    reshapeBuffer(Currentbuf);
}

static void
_docCSet(enum wc_ces charset)
{
    if (Currentbuf->bufferprop & BP_INTERNAL)
        return;
    if (Currentbuf->content->sourcefile == NULL) {
        disp_message("Can't reload...", FALSE);
        return;
    }
    Currentbuf->doc.charset = charset;
}

void change_charset(struct parsed_tagarg* arg)
{
    struct Buffer* buf = Currentbuf->linkBuffer[LB_N_INFO];
    enum wc_ces charset;

    if (buf == NULL)
        return;
    delBuffer(Currentbuf);
    Currentbuf = buf;
    if (Currentbuf->bufferprop & BP_INTERNAL)
        return;
    charset = Currentbuf->doc.charset;
    for (; arg; arg = arg->next) {
        if (!strcmp(arg->arg, "charset"))
            charset = atoi(arg->value);
    }
    _docCSet(charset);
}

DEFUN(docCSet, CHARSET, "Change the character encoding for the current document")
{
    char* cs = searchKeyData();
    if (cs == NULL || *cs == '\0')
        /* FIXME: gettextize? */
        cs = inputStr("Document charset: ",
            wc_ces_to_charset(Currentbuf->doc.charset));

    enum wc_ces charset = wc_guess_charset_short(cs, 0);
    if (charset == 0) {
        return;
    }
    _docCSet(charset);
}

DEFUN(defCSet, DEFAULT_CHARSET, "Change the default character encoding")
{
    char* cs = searchKeyData();
    if (cs == NULL || *cs == '\0')
        /* FIXME: gettextize? */
        cs = inputStr("Default document charset: ",
            wc_ces_to_charset(getRuntime()->DocumentCharset));
    enum wc_ces charset = wc_guess_charset_short(cs, 0);
    if (charset != 0)
        getRuntime()->DocumentCharset = charset;
}

/* mark URL-like patterns as anchors */
void chkURLBuffer(struct Buffer* buf)
{
    static char* url_like_pat[] = {
        "https?://[a-zA-Z0-9][a-zA-Z0-9:%\\-\\./?=~_\\&+@#,\\$;]*[a-zA-Z0-9_/=\\-]",
        "file:/[a-zA-Z0-9:%\\-\\./=_\\+@#,\\$;]*",
        "ftp://[a-zA-Z0-9][a-zA-Z0-9:%\\-\\./=_+@#,\\$]*[a-zA-Z0-9_/]",
        "https?://[a-zA-Z0-9:%\\-\\./_@]*\\[[a-fA-F0-9:][a-fA-F0-9:\\.]*\\][a-zA-Z0-9:%\\-\\./?=~_\\&+@#,\\$;]*",
        "ftp://[a-zA-Z0-9:%\\-\\./_@]*\\[[a-fA-F0-9:][a-fA-F0-9:\\.]*\\][a-zA-Z0-9:%\\-\\./=_+@#,\\$]*",
        NULL
    };
    int i;
    for (i = 0; url_like_pat[i]; i++) {
        reAnchor(buf, url_like_pat[i]);
    }
    chkExternalURIBuffer(buf);
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
    reAnchorWord(Currentbuf, Currentbuf->doc.currentLine, spos, epos);
}

/* render frames */
DEFUN(rFrame, FRAME, "Toggle rendering HTML frames")
{
    struct Buffer* buf;

    if ((buf = Currentbuf->linkBuffer[LB_FRAME]) != NULL) {
        Currentbuf = buf;
        return;
    }
    if (Currentbuf->doc.frameset == NULL) {
        if ((buf = Currentbuf->linkBuffer[LB_N_FRAME]) != NULL) {
            Currentbuf = buf;
        }
        return;
    }
    if (fmInitialized()) {
        message("Rendering frame");
    }
    buf = renderFrame(Currentbuf, 0);
    if (buf == NULL) {
        return;
    }
    buf->linkBuffer[LB_N_FRAME] = Currentbuf;
    Currentbuf->linkBuffer[LB_FRAME] = buf;
    tab_push_buffer(getRuntime()->CurrentTab, buf);
}

/* spawn external browser */
static void
invoke_browser(char* url)
{
    getRuntime()->CurrentKeyData = NULL; /* not allowed in w3m-control: */
    char* browser = searchKeyData();
    if (browser == NULL || *browser == '\0') {
        switch (getRuntime()->prec_num) {
        case 0:
        case 1:
            browser = getRuntime()->ExtBrowser;
            break;
        case 2:
            browser = getRuntime()->ExtBrowser2;
            break;
        case 3:
            browser = getRuntime()->ExtBrowser3;
            break;
        case 4:
            browser = getRuntime()->ExtBrowser4;
            break;
        case 5:
            browser = getRuntime()->ExtBrowser5;
            break;
        case 6:
            browser = getRuntime()->ExtBrowser6;
            break;
        case 7:
            browser = getRuntime()->ExtBrowser7;
            break;
        case 8:
            browser = getRuntime()->ExtBrowser8;
            break;
        case 9:
            browser = getRuntime()->ExtBrowser9;
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
        return;
    }

    int bg = 0, len;
    if ((len = strlen(browser)) >= 2 && browser[len - 1] == '&' && browser[len - 2] != '\\') {
        browser = allocStr(browser, len - 2);
        bg = 1;
    }
    Str cmd = myExtCommand(browser, shell_quote(url), FALSE);
    Strremovetrailingspaces(cmd);
    exitRawMode();
    mySystem(cmd->ptr, bg);
    enterRawMode();
}

DEFUN(extbrz, EXTERN, "Display using an external browser")
{
    if (Currentbuf->bufferprop & BP_INTERNAL) {
        /* FIXME: gettextize? */
        disp_err_message("Can't browse...", TRUE);
        return;
    }
    if (Currentbuf->content->url.scheme == SCM_LOCAL && !strcmp(Currentbuf->content->url.file, "-")) {
        /* file is std input */
        /* FIXME: gettextize? */
        disp_err_message("Can't browse stdin", TRUE);
        return;
    }
    invoke_browser(parsedURL2Str(&Currentbuf->content->url)->ptr);
}

DEFUN(linkbrz, EXTERN_LINK, "Display target using an external browser")
{
    if (ctx.buf->doc.firstLine == NULL)
        return;
    struct Anchor* a = doc_retrieveCurrentAnchor(&ctx.buf->doc);
    if (a == NULL)
        return;
    struct Url pu;
    parseURL2(a->url, &pu, baseURL(ctx.buf));
    invoke_browser(parsedURL2Str(&pu)->ptr);
}

/* show current line number and number of lines in the entire document */
DEFUN(curlno, LINE_INFO, "Display current position in document")
{
    struct Line* l = Currentbuf->doc.currentLine;
    Str tmp;
    int cur = 0, all = 0, col = 0, len = 0;

    if (l != NULL) {
        cur = l->real_linenumber;
        col = l->bwidth + Currentbuf->doc.currentColumn + Currentbuf->doc.cursorX + 1;
        while (l->next && l->next->bpos)
            l = l->next;
        if (l->width < 0)
            l->width = COLPOS(l, l->len);
        len = l->bwidth + l->width;
    }
    if (Currentbuf->doc.lastLine)
        all = Currentbuf->doc.lastLine->real_linenumber;
    // if (Currentbuf->pagerSource && !(Currentbuf->bufferprop & BP_CLOSE))
    //     tmp = Sprintf("line %d col %d/%d", cur, col, len);
    // else
    tmp = Sprintf("line %d/%d (%d%%) col %d/%d", cur, all,
        (int)((double)cur * 100.0 / (double)(all ? all : 1)
            + 0.5),
        col, len);
    Strcat_charp(tmp, "  ");
    Strcat_charp(tmp, wc_ces_to_charset_desc(Currentbuf->doc.charset));

    disp_message(tmp->ptr, FALSE);
}

DEFUN(dispI, DISPLAY_IMAGE, "Restart loading and drawing of images")
{
    if (!getRuntime()->displayImage)
        initImage();
    if (!getRuntime()->activeImage)
        return;
    getRuntime()->displayImage = true;
    /*
     * if (!(Currentbuf->type && is_html_type(Currentbuf->type)))
     * return;
     */
    Currentbuf->doc.image_flag = IMG_FLAG_AUTO;
}

DEFUN(stopI, STOP_IMAGE, "Stop loading and drawing of images")
{
    if (!getRuntime()->activeImage)
        return;
    /*
     * if (!(Currentbuf->type && is_html_type(Currentbuf->type)))
     * return;
     */
    Currentbuf->doc.image_flag = IMG_FLAG_SKIP;
}

DEFUN(dispVer, VERSION, "Display the version of w3m")
{
    disp_message(Sprintf("w3m version %s", w3m_version)->ptr, TRUE);
}

DEFUN(wrapToggle, WRAP_TOGGLE, "Toggle wrapping mode in searches")
{
    if (getRuntime()->WrapSearch) {
        getRuntime()->WrapSearch = FALSE;
        /* FIXME: gettextize? */
        disp_message("Wrap search off", TRUE);
    } else {
        getRuntime()->WrapSearch = TRUE;
        /* FIXME: gettextize? */
        disp_message("Wrap search on", TRUE);
    }
}

static void
execdict(char* word)
{
    if (!getRuntime()->UseDictCommand || word == NULL || *word == '\0') {
        return;
    }
    char* w = conv_to_system(word);
    if (*w == '\0') {
        return;
    }

    char* dictcmd = Sprintf("%s?%s", getRuntime()->DictCommand, Str_form_quote(Strnew_charp(w))->ptr)->ptr;

    struct Content *content = get_content_cache(dictcmd, NULL,
        (struct LoadOption) { .base_url = NULL, .referer = NO_REFERER, .flag = 0 });
    struct Buffer* buf = newBuffer(INIT_BUFFER_WIDTH);
    buf->content = content;
    if (buf == NULL) {
        disp_message("Execution failed", TRUE);
        return;
    } else {
        buf->content->filename = w;
        buf->doc.title = Sprintf("%s %s", DICTBUFFERNAME, word)->ptr;
        if (buf->content->content_type == NULL)
            buf->content->content_type = "text/plain";
        tab_push_buffer(getRuntime()->CurrentTab, buf);
    }
}

DEFUN(dictword, DICT_WORD, "Execute dictionary command (see README.dict)")
{
    execdict(inputStr("(dictionary)!", ""));
}

DEFUN(dictwordat, DICT_WORD_AT,
    "Execute dictionary command for word at cursor")
{
    execdict(GetWord(Currentbuf));
}

char* searchKeyData(void)
{
    const char* data = NULL;
    if (getRuntime()->CurrentKeyData != NULL && *getRuntime()->CurrentKeyData != '\0')
        data = getRuntime()->CurrentKeyData;
    else if (getRuntime()->CurrentCmdData != NULL && *getRuntime()->CurrentCmdData != '\0')
        data = getRuntime()->CurrentCmdData;
    else if (getRuntime()->CurrentKey >= 0)
        data = getKeyData(getRuntime()->CurrentKey);
    getRuntime()->CurrentKeyData = NULL;
    getRuntime()->CurrentCmdData = NULL;
    if (data == NULL || *data == '\0')
        return NULL;
    return allocStr(data, -1);
}

void deleteFiles()
{
    struct Buffer* buf;
    char* f;

    for (struct TabBuffer* CurrentTab = FirstTab(); CurrentTab; CurrentTab = CurrentTab->nextTab) {
        while (Firstbuf) {
            buf = Firstbuf->nextBuffer;
            discardBuffer(Firstbuf);
            Firstbuf = buf;
        }
    }
    while ((f = popText(getRuntime()->fileToDelete)) != NULL) {
        unlink(f);
        if (getRuntime()->enable_inline_image == INLINE_IMG_SIXEL && strcmp(f + strlen(f) - 4, ".gif") == 0) {
            Str firstframe = Strnew_charp(f);
            Strcat_charp(firstframe, "-1");
            unlink(firstframe->ptr);
        }
    }
}

void w3m_exit(int i)
{
    deleteFiles();
    free_ssl_ctx();
    disconnectFTP();
    if (getRuntime()->mkd_tmp_dir)
        if (rmdir(getRuntime()->mkd_tmp_dir) != 0) {
            fprintf(stderr, "Can't remove temporary directory (%s)!\n", getRuntime()->mkd_tmp_dir);
            exit(1);
        }
    exit(i);
}

DEFUN(execCmd, COMMAND, "Invoke w3m function(s)")
{
    getRuntime()->CurrentKeyData = NULL; /* not allowed in w3m-control: */
    const char* data = searchKeyData();
    if (data == NULL || *data == '\0') {
        data = inputStrHist("command [; ...]: ", "", getRuntime()->TextHist);
        if (data == NULL) {
            return;
        }
    }
    /* data: FUNC [DATA] [; FUNC [DATA] ...] */
    while (*data) {
        data = skip_blanks(data);
        if (*data == ';') {
            data++;
            continue;
        }
        char* p = getWord(&data);
        int cmd = getFuncList(p);
        if (cmd < 0)
            break;
        p = getQWord(&data);
        getRuntime()->CurrentKey = -1;
        getRuntime()->CurrentKeyData = NULL;
        getRuntime()->CurrentCmdData = *p ? p : NULL;
        w3mFuncList[cmd].func(ctx);
        getRuntime()->CurrentCmdData = NULL;
    }
}

// static MySignalHandler
// SigAlarm(SIGNAL_ARG)
// {
//     char* data;
//
//     if (CurrentAlarm->sec > 0) {
//         getRuntime()->CurrentKey = -1;
//         getRuntime()->CurrentKeyData = NULL;
//         getRuntime()->CurrentCmdData = data = (char*)CurrentAlarm->data;
//         w3mFuncList[CurrentAlarm->cmd].func();
//         getRuntime()->CurrentCmdData = NULL;
//         if (CurrentAlarm->status == AL_IMPLICIT_ONCE) {
//             CurrentAlarm->sec = 0;
//             CurrentAlarm->status = AL_UNSET;
//         }
//         if (Currentbuf->event) {
//             if (Currentbuf->event->status != AL_UNSET)
//                 CurrentAlarm = Currentbuf->event;
//             else
//                 Currentbuf->event = NULL;
//         }
//         if (!Currentbuf->event)
//             CurrentAlarm = &DefaultAlarm;
//         if (CurrentAlarm->sec > 0) {
//             mySignal(SIGALRM, SigAlarm);
//             alarm(CurrentAlarm->sec);
//         }
//     }
//     SIGNAL_RETURN;
// }

DEFUN(setAlarm, ALARM, "Set alarm")
{
    getRuntime()->CurrentKeyData = NULL; /* not allowed in w3m-control: */
    const char* data = searchKeyData();
    if (data == NULL || *data == '\0') {
        data = inputStrHist("(Alarm)sec command: ", "", getRuntime()->TextHist);
        if (data == NULL) {
            return;
        }
    }
    set_alarm(data);
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
        initKeymap(TRUE);
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

    if (!strcasecmp(resource, "URIMETHODS")) {
        initURIMethods();
        return;
    }

    disp_err_message(Sprintf("Don't know how to reinitialize '%s'", resource)->ptr, FALSE);
}

DEFUN(defKey, DEFINE_KEY, "Define a binding between a key stroke combination and a command")
{
    getRuntime()->CurrentKeyData = NULL; /* not allowed in w3m-control: */
    char* data = searchKeyData();
    if (data == NULL || *data == '\0') {
        data = inputStrHist("Key definition: ", "", getRuntime()->TextHist);
        if (data == NULL || *data == '\0') {
            return;
        }
    }
    setKeymap(allocStr(data, -1), -1, TRUE);
}

DEFUN(newT, NEW_TAB, "Open a new tab (with current document)")
{
    _newT();
}

static struct TabBuffer*
numTab(int n)
{
    struct TabBuffer* tab;
    int i;

    if (n == 0)
        return CurrentTab();
    if (n == 1)
        return FirstTab();
    if (nTab() <= 1)
        return NULL;
    for (tab = FirstTab(), i = 1; tab && i < n; tab = tab->nextTab, i++)
        ;
    return tab;
}

struct TabBuffer*
deleteTab(struct TabBuffer* tab)
{
    struct Buffer *buf, *next;

    if (nTab() <= 1)
        return FirstTab();
    if (tab->prevTab) {
        if (tab->nextTab)
            tab->nextTab->prevTab = tab->prevTab;
        else
            getRuntime()->LastTab = tab->prevTab;
        tab->prevTab->nextTab = tab->nextTab;
        if (tab == CurrentTab())
            getRuntime()->CurrentTab = tab->prevTab;
    } else { /* tab == FirstTab */
        tab->nextTab->prevTab = NULL;
        getRuntime()->FirstTab = tab->nextTab;
        if (tab == CurrentTab())
            getRuntime()->CurrentTab = tab->nextTab;
    }
    getRuntime()->nTab--;
    buf = tab->firstBuffer;
    while (buf) {
        next = buf->nextBuffer;
        discardBuffer(buf);
        buf = next;
    }
    return FirstTab();
}

DEFUN(closeT, CLOSE_TAB, "Close tab")
{
    struct TabBuffer* tab;

    if (nTab() <= 1)
        return;
    if (getRuntime()->prec_num)
        tab = numTab(PREC_NUM);
    else
        tab = CurrentTab();
    if (tab)
        deleteTab(tab);
}

DEFUN(nextT, NEXT_TAB, "Switch to the next tab")
{
    int i;

    if (nTab() <= 1)
        return;
    for (i = 0; i < PREC_NUM; i++) {
        if (CurrentTab()->nextTab)
            getRuntime()->CurrentTab = CurrentTab()->nextTab;
        else
            getRuntime()->CurrentTab = FirstTab();
    }
}

DEFUN(prevT, PREV_TAB, "Switch to the previous tab")
{
    int i;

    if (nTab() <= 1)
        return;
    for (i = 0; i < PREC_NUM; i++) {
        if (CurrentTab()->prevTab)
            getRuntime()->CurrentTab = CurrentTab()->prevTab;
        else
            getRuntime()->CurrentTab = LastTab();
    }
}

static void
followTab(struct TabBuffer* tab)
{
    struct Anchor* a = doc_retrieveCurrentImg(&Currentbuf->doc);
    if (!(a && a->image && a->image->map))
        a = doc_retrieveCurrentAnchor(&Currentbuf->doc);
    if (a == NULL)
        return;

    if (tab == CurrentTab()) {
        getRuntime()->check_target = FALSE;
        followA((struct DefunContext) { 0 });
        getRuntime()->check_target = TRUE;
        return;
    }

    _newT();
    struct Buffer* buf = Currentbuf;
    getRuntime()->check_target = FALSE;
    followA((struct DefunContext) { 0 });
    getRuntime()->check_target = TRUE;
    if (tab == NULL) {
        if (buf != Currentbuf)
            delBuffer(buf);
        else
            deleteTab(CurrentTab());
    } else if (buf != Currentbuf) {
        /* buf <- p <- ... <- Currentbuf = c */
        struct Buffer *c, *p;

        c = Currentbuf;
        if ((p = prevBuffer(c, buf)))
            p->nextBuffer = NULL;
        Firstbuf = buf;
        deleteTab(CurrentTab());
        getRuntime()->CurrentTab = tab;
        for (buf = p; buf; buf = p) {
            p = prevBuffer(c, buf);
            tab_push_buffer(tab, buf);
        }
    }
}

DEFUN(tabA, TAB_LINK, "Follow current hyperlink in a new tab")
{
    followTab(getRuntime()->prec_num ? numTab(PREC_NUM) : NULL);
}

static void
tabURL0(struct TabBuffer* tab, char* prompt, int relative)
{
    struct Buffer* buf;

    if (tab == CurrentTab()) {
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
            deleteTab(CurrentTab());
    } else if (buf != Currentbuf) {
        /* buf <- p <- ... <- Currentbuf = c */
        struct Buffer *c, *p;

        c = Currentbuf;
        if ((p = prevBuffer(c, buf)))
            p->nextBuffer = NULL;
        Firstbuf = buf;
        deleteTab(CurrentTab());
        getRuntime()->CurrentTab = tab;
        for (buf = p; buf; buf = p) {
            p = prevBuffer(c, buf);
            tab_push_buffer(tab, buf);
        }
    }
}

DEFUN(tabURL, TAB_GOTO, "Open specified document in a new tab")
{
    tabURL0(getRuntime()->prec_num ? numTab(PREC_NUM) : NULL,
        "Goto URL on new tab: ", FALSE);
}

DEFUN(tabrURL, TAB_GOTO_RELATIVE, "Open relative address in a new tab")
{
    tabURL0(getRuntime()->prec_num ? numTab(PREC_NUM) : NULL,
        "Goto relative URL on new tab: ", TRUE);
}

static void
moveTab(struct TabBuffer* t, struct TabBuffer* t2, int right)
{
    if (t2 == NO_TABBUFFER)
        t2 = FirstTab();
    if (!t || !t2 || t == t2 || t == NO_TABBUFFER)
        return;
    if (t->prevTab) {
        if (t->nextTab)
            t->nextTab->prevTab = t->prevTab;
        else
            getRuntime()->LastTab = t->prevTab;
        t->prevTab->nextTab = t->nextTab;
    } else {
        t->nextTab->prevTab = NULL;
        getRuntime()->FirstTab = t->nextTab;
    }
    if (right) {
        t->nextTab = t2->nextTab;
        t->prevTab = t2;
        if (t2->nextTab)
            t2->nextTab->prevTab = t;
        else
            getRuntime()->LastTab = t;
        t2->nextTab = t;
    } else {
        t->prevTab = t2->prevTab;
        t->nextTab = t2;
        if (t2->prevTab)
            t2->prevTab->nextTab = t;
        else
            getRuntime()->FirstTab = t;
        t2->prevTab = t;
    }
}

DEFUN(tabR, TAB_RIGHT, "Move right along the tab bar")
{
    int i = 0;
    struct TabBuffer* tab = CurrentTab();
    for (; tab && i < PREC_NUM; tab = tab->nextTab, i++)
        ;
    moveTab(CurrentTab(), tab ? tab : LastTab(), TRUE);
}

DEFUN(tabL, TAB_LEFT, "Move left along the tab bar")
{
    struct TabBuffer* tab = CurrentTab();
    int i = 0;
    for (; tab && i < PREC_NUM;
        tab = tab->prevTab, i++)
        ;
    moveTab(CurrentTab(), tab ? tab : FirstTab(), FALSE);
}

/* download panel */
DEFUN(ldDL, DOWNLOAD_LIST, "Display downloads panel")
{
    assert(false);
    // download_panel();
}

DEFUN(undoPos, UNDO, "Cancel the last cursor movement")
{
    if (!Currentbuf->doc.firstLine)
        return;
    struct DocumentPos* pos = ctx.buf->doc.undo;
    if (!pos || !pos->prev)
        return;
    for (int i = 0; i < PREC_NUM && pos->prev; i++, pos = pos->prev)
        ;
    doc_resetPos(&ctx.buf->doc, pos);
}

DEFUN(redoPos, REDO, "Cancel the last undo")
{
    if (!Currentbuf->doc.firstLine)
        return;
    struct DocumentPos* pos = ctx.buf->doc.undo;
    if (!pos || !pos->next)
        return;
    for (int i = 0; i < PREC_NUM && pos->next; i++, pos = pos->next)
        ;
    doc_resetPos(&ctx.buf->doc, pos);
}

DEFUN(cursorTop, CURSOR_TOP, "Move cursor to the top of the screen")
{
    if (Currentbuf->doc.firstLine == NULL)
        return;
    Currentbuf->doc.currentLine = doc_lineSkip(&Currentbuf->doc, Currentbuf->doc.topLine, 0);
    doc_arrangeLine(&Currentbuf->doc);
}

DEFUN(cursorMiddle, CURSOR_MIDDLE, "Move cursor to the middle of the screen")
{
    if (Currentbuf->doc.firstLine == NULL)
        return;
    int offsety = (Currentbuf->doc.LINES - 1) / 2;
    Currentbuf->doc.currentLine = currentLineSkip(Currentbuf->doc.topLine, offsety);
    doc_arrangeLine(&Currentbuf->doc);
}

DEFUN(cursorBottom, CURSOR_BOTTOM, "Move cursor to the bottom of the screen")
{
    if (Currentbuf->doc.firstLine == NULL)
        return;
    int offsety = Currentbuf->doc.LINES - 1;
    Currentbuf->doc.currentLine = currentLineSkip(Currentbuf->doc.topLine, offsety);
    doc_arrangeLine(&Currentbuf->doc);
}
