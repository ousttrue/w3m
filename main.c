#include "alloc.h"
#include "buffer.h"
#include "content.h"
#include "cookie.h"
#include "display.h"
#include "file.h"
#include "history.h"
#include "html_form.h"
#include "image.h"
#include "indep.h"
#include "local_cgi.h"
#include "message.h"
#include "myctype.h"
#include "tab.h"
#include "tab_list.h"
#include "textlist.h"
#include "w3m_rc.h"
#include <libwc/wtf.h>
#include <libwc/ucs.h>
#include <libwc/charset.h>
#include <libwc/status.h>

#include <gc/gc.h>
#include <assert.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int show_params_p = 0;

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
            struct Content* content = get_content_cache(getRuntime()->BookmarkFile, NULL,
                (struct LoadOption) { .base_url = NULL, .referer = NO_REFERER, .flag = 0 });
            if (!content)
                Strcat_charp(err_msg, "w3m: Can't load bookmark.\n");
            newbuf = buf_new(content);
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
            struct Content* content = get_content_cache(p, NULL,
                (struct LoadOption) { .base_url = NULL, .referer = NO_REFERER, .flag = 0 });
            newbuf = buf_new(content);
            if (!content)
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
                struct Content* content = get_content_cache(url, request,
                    (struct LoadOption) { .base_url = NULL, .referer = NO_REFERER, .flag = 0 });
                newbuf = buf_new(content);
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
            tabs_append(newbuf);
            // getRuntime()->FirstTab = getRuntime()->LastTab = getRuntime()->CurrentTab = tab_new();
            // if (!FirstTab()) {
            //     fprintf(stderr, "%s\n", "Can't allocated memory");
            //     exit(1);
            // }
            // getRuntime()->nTab = 1;
            // Firstbuf = Currentbuf = newbuf;
        } else if (open_new_tab) {
            tabs_append(newbuf);
        } else {
            tab_push_buffer(CurrentTab(), newbuf);
        }
        assert(Currentbuf);
        assert(Firstbuf);

        Currentbuf = newbuf;
    }

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
    screen_from_lines(Currentbuf->doc, baseURL(Currentbuf));
    if (line_str) {
        doc_goLine(Currentbuf->doc, line_str);
    }

    return true;
}
