#include "Args.h"
#include "term_screen.h"
#include "fm.h"

#include <stdio.h>

static void fversion(FILE *f)
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
            ",external-uri-loader"
#ifdef INET6
            ",ipv6"
#endif
            ",alarm");
}

// rc.c
void show_params(FILE *fp);

static int show_params_p = 0;
static void fusage(FILE *f, int err)
{
    fversion(f);
    /* FIXME: gettextize? */
    fprintf(f, "usage: w3m [options] [URL or filename]\noptions:\n");
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
    fprintf(f, "    -N               open URL of command line on each new tab\n");
    fprintf(f, "    -F               automatically render frames\n");
    fprintf(f, "    -cols width      specify column width (used with -dump)\n");
    fprintf(f, "    -ppc count       specify the number of pixels per character "
               "(4.0...32.0)\n");
    fprintf(f, "    -ppl count       specify the number of pixels per line "
               "(4.0...64.0)\n");
    fprintf(f, "    -dump            dump formatted page into stdout\n");
    fprintf(f,
            "    -dump_head       dump response of HEAD request into stdout\n");
    fprintf(f, "    -dump_source     dump page source into stdout\n");
    fprintf(f, "    -dump_both       dump HEAD and source into stdout\n");
    fprintf(f, "    -dump_extra      dump HEAD, source, and extra information "
               "into stdout\n");
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
    fprintf(f, "    -graph           use DEC special graphics for border of "
               "table and menu\n");
    fprintf(f, "    -no-graph        use ASCII character for border of table and "
               "menu\n");
    fprintf(f, "    -s               squeeze multiple blank lines\n");
    fprintf(f, "    -W               toggle search wrap mode\n");
    fprintf(f, "    -X               don't use termcap init/deinit\n");
    fprintf(f, "    -title[=TERM]    set buffer name to terminal title string\n");
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

void help()
{
    fusage(stdout, 0);
}
void usage()
{
    fusage(stderr, 1);
}

void help_or_exit(int argc, char **argv)
{
    /* argument search 1 */
    for (int i = 1; i < argc; i++)
    {
        if (*argv[i] == '-')
        {
            if (!strcmp("-config", argv[i]))
            {
                argv[i] = "-dummy";
                if (++i >= argc)
                    usage();
                config_file = argv[i];
                argv[i] = "-dummy";
            }
            else if (!strcmp("-h", argv[i]) || !strcmp("-help", argv[i]))
                help();
            else if (!strcmp("-V", argv[i]) || !strcmp("-version", argv[i]))
            {
                fversion(stdout);
                exit(0);
            }
        }
    }
}

static Str make_optional_header_string(char *s)
{
    char *p;
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
    if (*(++p))
    {                   /* not null header */
        SKIP_BLANKS(p); /* skip white spaces */
        Strcat_charp(hs, p);
    }
    Strcat_charp(hs, "\r\n");
    return hs;
}

void Args::parse(int argc, char **argv)
{
    i = 1;
    while (i < argc)
    {
        if (*argv[i] == '-')
        {
            if (!strcmp("-t", argv[i]))
            {
                if (++i >= argc)
                    usage();
                if (atoi(argv[i]) > 0)
                    Tabstop = atoi(argv[i]);
            }
            else if (!strcmp("-r", argv[i]))
                ShowEffect = false;
            else if (!strcmp("-l", argv[i]))
            {
                if (++i >= argc)
                    usage();
                if (atoi(argv[i]) > 0)
                    PagerMax = atoi(argv[i]);
            }
            else if (!strncmp("-I", argv[i], 2))
            {
                char *p;
                if (argv[i][2] != '\0')
                    p = argv[i] + 2;
                else
                {
                    if (++i >= argc)
                        usage();
                    p = argv[i];
                }
                DocumentCharset = wc_guess_charset_short(p, DocumentCharset);
                WcOption.auto_detect = WC_OPT_DETECT_OFF;
                UseContentCharset = false;
            }
            else if (!strncmp("-O", argv[i], 2))
            {
                char *p;
                if (argv[i][2] != '\0')
                    p = argv[i] + 2;
                else
                {
                    if (++i >= argc)
                        usage();
                    p = argv[i];
                }
                DisplayCharset = wc_guess_charset_short(p, DisplayCharset);
            }
            else if (!strcmp("-graph", argv[i]))
                UseGraphicChar = GRAPHIC_CHAR_DEC;
            else if (!strcmp("-no-graph", argv[i]))
                UseGraphicChar = GRAPHIC_CHAR_ASCII;
            else if (!strcmp("-T", argv[i]))
            {
                if (++i >= argc)
                    usage();
                DefaultType = default_type = argv[i];
            }
            else if (!strcmp("-m", argv[i]))
                SearchHeader = search_header = true;
            else if (!strcmp("-v", argv[i]))
                visual_start = true;
            else if (!strcmp("-N", argv[i]))
                open_new_tab = true;
            else if (!strcmp("-M", argv[i]))
                useColor = false;
            else if (!strcmp("-B", argv[i]))
                load_bookmark = true;
            else if (!strcmp("-bookmark", argv[i]))
            {
                if (++i >= argc)
                    usage();
                BookmarkFile = argv[i];
                if (BookmarkFile[0] != '~' && BookmarkFile[0] != '/')
                {
                    Str tmp = Strnew_charp(CurrentDir);
                    if (Strlastchar(tmp) != '/')
                        Strcat_char(tmp, '/');
                    Strcat_charp(tmp, BookmarkFile);
                    BookmarkFile = cleanupName(tmp->ptr);
                }
            }
            else if (!strcmp("-F", argv[i]))
                RenderFrame = true;
            else if (!strcmp("-W", argv[i]))
            {
                if (WrapDefault)
                    WrapDefault = false;
                else
                    WrapDefault = true;
            }
            else if (!strcmp("-dump", argv[i]))
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
            else if (!strcmp("-halfload", argv[i]))
            {
                w3m_dump = 0;
                w3m_halfload = true;
                DefaultType = default_type = "text/html";
            }
            else if (!strcmp("-backend", argv[i]))
            {
                w3m_backend = true;
            }
            else if (!strcmp("-backend_batch", argv[i]))
            {
                w3m_backend = true;
                if (++i >= argc)
                    usage();
                if (!backend_batch_commands)
                    backend_batch_commands = newTextList();
                pushText(backend_batch_commands, argv[i]);
            }
            else if (!strcmp("-cols", argv[i]))
            {
                if (++i >= argc)
                    usage();
                COLS = atoi(argv[i]);
                if (COLS > MAXIMUM_COLS)
                {
                    COLS = MAXIMUM_COLS;
                }
            }
            else if (!strcmp("-ppc", argv[i]))
            {
                double ppc;
                if (++i >= argc)
                    usage();
                ppc = atof(argv[i]);
                if (ppc >= MINIMUM_PIXEL_PER_CHAR && ppc <= MAXIMUM_PIXEL_PER_CHAR)
                {
                    pixel_per_char = ppc;
                    set_pixel_per_char = true;
                }
            }
            else if (!strcmp("-ppl", argv[i]))
            {
                double ppc;
                if (++i >= argc)
                    usage();
                ppc = atof(argv[i]);
                if (ppc >= MINIMUM_PIXEL_PER_CHAR &&
                    ppc <= MAXIMUM_PIXEL_PER_CHAR * 2)
                {
                    pixel_per_line = ppc;
                    set_pixel_per_line = true;
                }
            }
            else if (!strcmp("-ri", argv[i]))
            {
                enable_inline_image = INLINE_IMG_OSC5379;
            }
            else if (!strcmp("-sixel", argv[i]))
            {
                enable_inline_image = INLINE_IMG_SIXEL;
            }
            else if (!strcmp("-num", argv[i]))
                showLineNum = true;
            else if (!strcmp("-no-proxy", argv[i]))
                use_proxy = false;
#ifdef INET6
            else if (!strcmp("-4", argv[i]) || !strcmp("-6", argv[i]))
                set_param_option(Sprintf("dns_order=%c", argv[i][1])->ptr);
#endif
            else if (!strcmp("-post", argv[i]))
            {
                if (++i >= argc)
                    usage();
                post_file = argv[i];
            }
            else if (!strcmp("-header", argv[i]))
            {
                Str hs;
                if (++i >= argc)
                    usage();
                if ((hs = make_optional_header_string(argv[i])) != nullptr)
                {
                    if (header_string == nullptr)
                        header_string = hs;
                    else
                        Strcat(header_string, hs);
                }
                while (argv[i][0])
                {
                    argv[i][0] = '\0';
                    argv[i]++;
                }
            }
            else if (!strcmp("-no-cookie", argv[i]))
            {
                use_cookie = false;
                accept_cookie = false;
            }
            else if (!strcmp("-cookie", argv[i]))
            {
                use_cookie = true;
                accept_cookie = true;
            }
            else if (!strcmp("-s", argv[i]))
                squeezeBlankLine = true;
            else if (!strcmp("-title", argv[i]))
                displayTitleTerm = getenv("TERM");
            else if (!strncmp("-title=", argv[i], 7))
                displayTitleTerm = argv[i] + 7;
            else if (!strcmp("-insecure", argv[i]))
            {
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
            }
            else if (!strcmp("-o", argv[i]) || !strcmp("-show-option", argv[i]))
            {
                if (!strcmp("-show-option", argv[i]) || ++i >= argc ||
                    !strcmp(argv[i], "?"))
                {
                    show_params(stdout);
                    exit(0);
                }
                if (!set_param_option(argv[i]))
                {
                    /* option set failed */
                    /* FIXME: gettextize? */
                    fprintf(stderr, "%s: bad option\n", argv[i]);
                    show_params_p = 1;
                    usage();
                }
            }
            else if (!strcmp("-", argv[i]) || !strcmp("-dummy", argv[i]))
            {
                /* do nothing */
            }
            else if (!strcmp("-debug", argv[i]))
            {
                w3m_debug = true;
            }
            else if (!strcmp("-reqlog", argv[i]))
            {
                w3m_reqlog = rcFile("request.log");
            }
            else
            {
                usage();
            }
        }
        else if (*argv[i] == '+')
        {
            line_str = argv[i] + 1;
        }
        else
        {
            load_argv[load_argc++] = argv[i];
        }
        i++;
    }
}
