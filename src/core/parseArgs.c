#include "parseArgs.h"
#include "history.h"
#include "term_renderer.h"
#include "etc.h"
#include "screen.h"
#include "graphicchar.h"
#include "fm.h"
#include "rc.h"
#include "tty.h"
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

extern MySignalHandler
    resize_hook(SIGNAL_ARG);

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
        ",color"
        ",ansi-color"
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
    fprintf(f, "    -M               monochrome display\n");
    fprintf(f, "    -H               use high-intensity colors\n");
    fprintf(f,
        "    -N               open URL of command line on each new tab\n");
    fprintf(f, "    -ppc count       specify the number of pixels per character (4.0...32.0)\n");
    fprintf(f, "    -ppl count       specify the number of pixels per line (4.0...64.0)\n");
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
    fprintf(f, "    -reqlog          write request logfile\n");
    fprintf(f, "    -help            print this usage message\n");
    fprintf(f, "    -version         print w3m version\n");
    if (show_params_p)
        show_params(f);
    exit(err);
}

void parseArgs(int argc, char** argv)
{
    char* url = argv[1];
    if (getURLScheme(&url) == SCM_MISSING && !ArgvIsURL)
    retry_as_local_file:
        url = file_to_url(argv[1]);
    else
        url = url_encode(conv_from_system(argv[1]), NULL, 0);

    Buffer* newbuf = loadGeneralFile(url, NULL, NO_REFERER, 0, NULL);
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
    Firstbuf = Currentbuf = newbuf;
    saveBufferInfo();
}
