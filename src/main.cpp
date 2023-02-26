/* $Id: main.c,v 1.270 2010/08/24 10:11:51 htrb Exp $ */
#define MAINPROGRAM
#include "term_screen.h"
#include "tty.h"
#include "App.h"
#include "Args.h"

#include "core.h"
#if defined(HAVE_WAITPID) || defined(HAVE_WAIT3)
#include <sys/wait.h>
#endif

extern "C" int _main(int argc, char **argv);

Hist *LoadHist;
Hist *SaveHist;
Hist *URLHist;
Hist *ShellHist;
Hist *TextHist;

static GC_warn_proc orig_GC_warn_proc = NULL;
#define GC_WARN_KEEP_MAX (20)

static void wrap_GC_warn_proc(char *msg, GC_word arg)
{
    if (fmInitialized)
    {
        /* *INDENT-OFF* */
        static struct
        {
            char *msg;
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

        if (!lock)
        {
            lock = 1;

            for (; n > 0; --n, ++i)
            {
                i %= sizeof(msg_ring) / sizeof(msg_ring[0]);

                printf(msg_ring[i].msg, (unsigned long)msg_ring[i].arg);
                tty::sleep_till_anykey(1, 1);
            }

            lock = 0;
        }
    }
    else if (orig_GC_warn_proc)
        orig_GC_warn_proc(msg, arg);
    else
        fprintf(stderr, msg, (unsigned long)arg);
}

static void sig_chld(int signo)
{
    int p_stat;
    pid_t pid;

#ifdef HAVE_WAITPID
    while ((pid = waitpid(-1, &p_stat, WNOHANG)) > 0)
#elif HAVE_WAIT3
    while ((pid = wait3(&p_stat, WNOHANG, NULL)) > 0)
#else
    if ((pid = wait(&p_stat)) > 0)
#endif
    {
        DownloadList *d;

        if (WIFEXITED(p_stat))
        {
            for (d = FirstDL; d != NULL; d = d->next)
            {
                if (d->pid == pid)
                {
                    d->err = WEXITSTATUS(p_stat);
                    break;
                }
            }
        }
    }
    mySignal(SIGCHLD, sig_chld);
    return;
}

static void *die_oom(size_t bytes)
{
    fprintf(stderr, "Out of memory: %lu bytes unavailable!\n",
            (unsigned long)bytes);
    exit(1);
}

static void initialize()
{
    if (!getenv("GC_LARGE_ALLOC_WARN_INTERVAL"))
        set_environ("GC_LARGE_ALLOC_WARN_INTERVAL", "30000");
    GC_INIT();
#if (GC_VERSION_MAJOR > 7) || \
    ((GC_VERSION_MAJOR == 7) && (GC_VERSION_MINOR >= 2))
    GC_set_oom_fn(die_oom);
#else
    GC_oom_fn = die_oom;
#endif
#if defined(ENABLE_NLS) || (defined(USE_M17N) && defined(HAVE_LANGINFO_CODESET))
    setlocale(LC_ALL, "");
#endif
#ifdef ENABLE_NLS
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);
#endif

    NO_proxy_domains = newTextList();
    fileToDelete = newTextList();

    CurrentDir = currentdir();
    CurrentPid = (int)getpid();
    BookmarkFile = NULL;
    config_file = NULL;

    {
        char hostname[HOST_NAME_MAX + 2];
        if (gethostname(hostname, HOST_NAME_MAX + 2) == 0)
        {
            size_t hostname_len;
            /* Don't use hostname if it is truncated.  */
            hostname[HOST_NAME_MAX + 1] = '\0';
            hostname_len = strlen(hostname);
            if (hostname_len <= HOST_NAME_MAX && hostname_len < STR_SIZE_MAX)
                HostName = allocStr(hostname, (int)hostname_len);
        }
    }
}

int main(int argc, char **argv)
{
    initialize();
    help_or_exit(argc, argv);

    char *Locale = NULL;
    if (non_null(Locale = getenv("LC_ALL")) ||
        non_null(Locale = getenv("LC_CTYPE")) ||
        non_null(Locale = getenv("LANG")))
    {
        DisplayCharset = wc_guess_locale_charset(Locale, DisplayCharset);
        DocumentCharset = wc_guess_locale_charset(Locale, DocumentCharset);
        SystemCharset = wc_guess_locale_charset(Locale, SystemCharset);
    }

    // initializations
    init_rc();

    LoadHist = newHist();
    SaveHist = newHist();
    ShellHist = newHist();
    TextHist = newHist();
    URLHist = newHist();

    if (FollowLocale && Locale)
    {
        DisplayCharset = wc_guess_locale_charset(Locale, DisplayCharset);
        SystemCharset = wc_guess_locale_charset(Locale, SystemCharset);
    }

    BookmarkCharset = DocumentCharset;

    char *p;
    if (!non_null(HTTP_proxy) &&
        ((p = getenv("HTTP_PROXY")) || (p = getenv("http_proxy")) ||
         (p = getenv("HTTP_proxy"))))
        HTTP_proxy = p;
    if (!non_null(HTTPS_proxy) &&
        ((p = getenv("HTTPS_PROXY")) || (p = getenv("https_proxy")) ||
         (p = getenv("HTTPS_proxy"))))
        HTTPS_proxy = p;
    if (HTTPS_proxy == NULL && non_null(HTTP_proxy))
        HTTPS_proxy = HTTP_proxy;
    if (!non_null(FTP_proxy) &&
        ((p = getenv("FTP_PROXY")) || (p = getenv("ftp_proxy")) ||
         (p = getenv("FTP_proxy"))))
        FTP_proxy = p;
    if (!non_null(NO_proxy) &&
        ((p = getenv("NO_PROXY")) || (p = getenv("no_proxy")) ||
         (p = getenv("NO_proxy"))))
        NO_proxy = p;

    if (!non_null(Editor) && (p = getenv("EDITOR")) != NULL)
        Editor = p;
    if (!non_null(Mailer) && (p = getenv("MAILER")) != NULL)
        Mailer = p;

    // argument search 2
    Args argument;
    argument.parse(argc, argv);

    FirstTab = NULL;
    LastTab = NULL;
    nTab = 0;
    CurrentTab = NULL;

    CurrentKey = -1;
    if (BookmarkFile == NULL)
        BookmarkFile = rcFile(BOOKMARK);

    if (!isatty(1) && !w3m_dump)
    {
        /* redirected output */
        w3m_dump = DUMP_BUFFER;
    }
    if (w3m_dump)
    {
        if (COLS == 0)
            COLS = DEFAULT_COLS;
    }

    if (!w3m_dump && !w3m_backend)
    {
        fmInit();
    }
    else if (w3m_halfdump && displayImage)
        activeImage = TRUE;

    sync_with_option();
    initCookie();
    if (UseHistory)
        loadHistory(URLHist);

    wtf_init(DocumentCharset, DisplayCharset);
    /*  if (w3m_dump)
     *    WcOption.pre_conv = WC_TRUE;
     */

    if (w3m_backend)
        backend();

    if (w3m_dump)
        mySignal(SIGINT, SIG_IGN);
    mySignal(SIGCHLD, sig_chld);
    mySignal(SIGPIPE, SigPipe);

#if (GC_VERSION_MAJOR > 7) || \
    ((GC_VERSION_MAJOR == 7) && (GC_VERSION_MINOR >= 2))
    orig_GC_warn_proc = GC_get_warn_proc();
    GC_set_warn_proc(wrap_GC_warn_proc);
#else
    orig_GC_warn_proc = GC_set_warn_proc(wrap_GC_warn_proc);
#endif

    App::instance().run(argument);
}
