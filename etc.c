/* vi: set sw=4 ts=8 ai sm noet : */
#include "etc.h"

#include "charset.h"
#include "config.h"
#include "display.h"
#include "fm.h"
#include "proto.h"
#include "hash.h"
#include "html.h"
#include "linein.h"
#include "myctype.h"
#include "rc.h"
#include "symbol.h"
#include "terms.h"

#include <fcntl.h>
#include <libgen.h>
#include <pwd.h>
#include <time.h>
#include <signal.h>
#include <strings.h>

TextList* fileToDelete;

char* mydirname(const char* s)
{
    const char* p = s;
    while (*p)
        p++;
    if (s != p)
        p--;
    while (s != p && *p == '/')
        p--;
    while (s != p && *p != '/')
        p--;
    if (*p != '/')
        return ".";
    while (s != p && *p == '/')
        p--;
    return allocStr(s, strlen(s) - strlen(p) + 1);
}

/* get last modified time */
char* last_modified(Buffer* buf)
{
    TextListItem* ti;
    struct stat st;

    if (buf->document_header) {
        for (ti = buf->document_header->first; ti; ti = ti->next) {
            if (strncasecmp(ti->ptr, "Last-modified: ", 15) == 0) {
                return ti->ptr + 15;
            }
        }
        return "unknown";
    } else if (buf->currentURL.scheme == SCM_LOCAL) {
        if (stat(buf->currentURL.file, &st) < 0)
            return "unknown";
        return ctime(&st.st_mtime);
    }
    return "unknown";
}

static char roman_num1[] = {
    'i',
    'x',
    'c',
    'm',
    '*',
};
static char roman_num5[] = {
    'v',
    'l',
    'd',
    '*',
};

static Str
romanNum2(int l, int n)
{
    Str s = Strnew();

    switch (n) {
    case 1:
    case 2:
    case 3:
        for (; n > 0; n--)
            Strcat_char(s, roman_num1[l]);
        break;
    case 4:
        Strcat_char(s, roman_num1[l]);
        Strcat_char(s, roman_num5[l]);
        break;
    case 5:
    case 6:
    case 7:
    case 8:
        Strcat_char(s, roman_num5[l]);
        for (n -= 5; n > 0; n--)
            Strcat_char(s, roman_num1[l]);
        break;
    case 9:
        Strcat_char(s, roman_num1[l]);
        Strcat_char(s, roman_num1[l + 1]);
        break;
    }
    return s;
}

Str romanNumeral(int n)
{
    Str r = Strnew();

    if (n <= 0)
        return r;
    if (n >= 4000) {
        Strcat_charp(r, "**");
        return r;
    }
    Strcat(r, romanNum2(3, n / 1000));
    Strcat(r, romanNum2(2, (n % 1000) / 100));
    Strcat(r, romanNum2(1, (n % 100) / 10));
    Strcat(r, romanNum2(0, n % 10));

    return r;
}

Str romanAlphabet(int n)
{
    Str r = Strnew();
    int l;
    char buf[14];

    if (n <= 0)
        return r;

    l = 0;
    while (n) {
        buf[l++] = 'a' + (n - 1) % 26;
        n = (n - 1) / 26;
    }
    l--;
    for (; l >= 0; l--)
        Strcat_char(r, buf[l]);

    return r;
}

#ifndef SIGIOT
#define SIGIOT SIGABRT
#endif /* not SIGIOT */

static void
reset_signals(void)
{
#ifdef SIGHUP
    mySignal(SIGHUP, SIG_DFL); /* terminate process */
#endif
    mySignal(SIGINT, SIG_DFL); /* terminate process */
#ifdef SIGQUIT
    mySignal(SIGQUIT, SIG_DFL); /* terminate process */
#endif
    mySignal(SIGTERM, SIG_DFL); /* terminate process */
    mySignal(SIGILL, SIG_DFL); /* create core image */
    mySignal(SIGIOT, SIG_DFL); /* create core image */
    mySignal(SIGFPE, SIG_DFL); /* create core image */
#ifdef SIGBUS
    mySignal(SIGBUS, SIG_DFL); /* create core image */
#endif /* SIGBUS */
#ifdef SIGCHLD
    mySignal(SIGCHLD, SIG_IGN);
#endif
#ifdef SIGPIPE
    mySignal(SIGPIPE, SIG_IGN);
#endif
}

#ifndef FOPEN_MAX
#define FOPEN_MAX 1024 /* XXX */
#endif

static void
close_all_fds_except(int i, int f)
{
    switch (i) { /* fall through */
    case 0:
        dup2(open(DEV_NULL_PATH, O_RDONLY), 0);
    case 1:
        dup2(open(DEV_NULL_PATH, O_WRONLY), 1);
    case 2:
        dup2(open(DEV_NULL_PATH, O_WRONLY), 2);
    }
    /* close all other file descriptors (socket, ...) */
    for (i = 3; i < FOPEN_MAX; i++) {
        if (i != f)
            close(i);
    }
}

void setup_child(int child, int i, int f)
{
    reset_signals();
    mySignal(SIGINT, SIG_IGN);
#ifndef __MINGW32_VERSION
    if (!child)
        setpgid(0, 0);
#endif /* __MINGW32_VERSION */
    close_all_fds_except(i, f);
    QuietMessage = TRUE;
    fmInitialized = FALSE;
    TrapSignal = FALSE;
}

#ifndef __MINGW32_VERSION
pid_t open_pipe_rw(FILE** fr, FILE** fw)
{
    int fdr[2];
    int fdw[2];
    pid_t pid;

    if (fr && pipe(fdr) < 0)
        goto err0;
    if (fw && pipe(fdw) < 0)
        goto err1;

    flush_tty();
    pid = fork();
    if (pid < 0)
        goto err2;
    if (pid == 0) {
        /* child */
        if (fr) {
            close(fdr[0]);
            dup2(fdr[1], 1);
        }
        if (fw) {
            close(fdw[1]);
            dup2(fdw[0], 0);
        }
        return pid;
    }

    /* parent */
    if (fr) {
        close(fdr[1]);
        if (*fr == stdin) {
            dup2(fdr[0], 0);
            close(fdr[0]);
        } else {
            *fr = fdopen(fdr[0], "r");
        }
    }
    if (fw) {
        close(fdw[0]);
        if (*fw == stdout) {
            dup2(fdw[1], 1);
            close(fdw[1]);
        } else {
            *fw = fdopen(fdw[1], "w");
        }
    }
    return pid;

err2:
    if (fw) {
        close(fdw[0]);
        close(fdw[1]);
    }
err1:
    if (fr) {
        close(fdr[0]);
        close(fdr[1]);
    }
err0:
    return (pid_t)-1;
}
#endif /* __MINGW32_VERSION */

void myExec(char* command)
{
    mySignal(SIGINT, SIG_DFL);
    execl("/bin/sh", "sh", "-c", command, NULL);
    exit(127);
}

int mySystem(char* command, int background)
{
#ifndef __MINGW32_VERSION
    if (background) {
#ifndef __EMX__
        flush_tty();
        if (!fork()) {
            setup_child(FALSE, 0, -1);
            myExec(command);
        }
        return 0;
#else
        Str cmd = Strnew_charp("start /f ");
        Strcat_charp(cmd, command);
        return system(cmd->ptr);
#endif
    } else
#endif /* __MINGW32_VERSION */
        return system(command);
}

Str myExtCommand(const char* cmd, const char* arg, int redirect)
{
    Str tmp = NULL;
    const char* p;
    int set_arg = FALSE;

    for (p = cmd; *p; p++) {
        if (*p == '%' && *(p + 1) == 's' && !set_arg) {
            if (tmp == NULL)
                tmp = Strnew_charp_n(cmd, (int)(p - cmd));
            Strcat_charp(tmp, arg);
            set_arg = TRUE;
            p++;
        } else {
            if (tmp)
                Strcat_char(tmp, *p);
        }
    }
    if (!set_arg) {
        if (redirect)
            tmp = Strnew_m_charp("(", cmd, ") < ", arg, NULL);
        else
            tmp = Strnew_m_charp(cmd, " ", arg, NULL);
    }
    return tmp;
}

Str editor_cmd(const char* file, int line)
{
    Str tmp;
    const char* p;
    int n, file_set = FALSE, line_set = FALSE;

    static const char* eds[] = {
        "emacs",
        "hx", /* helix */
        "micro",
        "nano",
        "nvi",
        "nvim",
        "vi",
        "vim",
        NULL
    };

    tmp = Strnew();
    if (!*Editor)
        return tmp;

    for (p = Editor; *p; p++) {
        if (*p == '%' && p[1] == 's') {
            Strcat_charp(tmp, file);
            file_set = TRUE;
            p++;
        } else if (*p == '%' && p[1] == 'd') {
            Strcat(tmp, Sprintf("%d", line));
            line_set = TRUE;
            p++;
        } else
            Strcat_char(tmp, *p);
    }

    if (file_set)
        return tmp;

    n = strcspn(Editor, " ");
    if (!(p = basename(Strnew_charp_n(Editor, n)->ptr)))
        p = Editor;

    if (!line_set && line > 0)
        for (const char** e = eds; *e; e++)
            if (!strcmp(p, *e)) {
                Strcat(tmp, Sprintf(" +%d", line));
                break;
            }

    Strcat_m_charp(tmp, " ", file, NULL);
    return tmp;
}

#ifdef __MINGW32_VERSION
char* expandName(char* name)
{
    return getenv("HOME");
}
#else
char* expandName(char* name)
{
    char* p;
    struct passwd* passent;
    Str extpath = NULL;

    if (name == NULL)
        return NULL;
    p = name;
    if (*p == '/') {
        if ((*(p + 1) == '~' && IS_ALPHA(*(p + 2)))
            && personal_document_root) {
            char* q;
            p += 2;
            q = strchr(p, '/');
            if (q) { /* /~user/dir... */
                passent = getpwnam(allocStr(p, q - p));
                p = q;
            } else { /* /~user */
                passent = getpwnam(p);
                p = "";
            }
            if (!passent)
                goto rest;
            extpath = Strnew_m_charp(passent->pw_dir, "/",
                personal_document_root, NULL);
            if (*personal_document_root == '\0' && *p == '/')
                p++;
        } else
            goto rest;
        if (Strcmp_charp(extpath, "/") == 0 && *p == '/')
            p++;
        Strcat_charp(extpath, p);
        return extpath->ptr;
    } else
        return expandPath(p);
rest:
    return name;
}
#endif

int is_localhost(const char* host)
{
    if (!host || !strcasecmp(host, "localhost") || !strcmp(host, "127.0.0.1") || (HostName && !strcasecmp(host, HostName)) || !strcmp(host, "[::1]"))
        return TRUE;
    return FALSE;
}

char* file_to_url(char* file)
{
    Str tmp;
#ifdef SUPPORT_DOS_DRIVE_PREFIX
    char* drive = NULL;
#endif
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
#ifdef SUPPORT_DOS_DRIVE_PREFIX
    if (IS_ALPHA(file[0]) && file[1] == ':') {
        drive = allocStr(file, 2);
        file += 2;
    } else
#endif
        if (file[0] != '/') {
        tmp = Strnew_charp(CurrentDir);
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
#ifdef SUPPORT_DOS_DRIVE_PREFIX
    if (drive)
        Strcat_charp(tmp, drive);
#endif
    Strcat_charp(tmp, file_quote(cleanupName(file)));
    return tmp->ptr;
}

#ifdef USE_M17N
char* url_unquote_conv(const char* url, wc_ces charset)
#else
char* url_unquote_conv0(const char* url)
#endif
{
#ifdef USE_M17N
    wc_uint8 old_auto_detect = WcOption.auto_detect;
#endif
    Str tmp;
    tmp = Str_url_unquote(Strnew_charp(url), FALSE, TRUE);
#ifdef USE_M17N
    if (!charset || charset == WC_CES_US_ASCII)
        charset = SystemCharset;
    WcOption.auto_detect = WC_OPT_DETECT_ON;
    tmp = convertLine(NULL, tmp, RAW_MODE, &charset, charset);
    WcOption.auto_detect = old_auto_detect;
#endif
    return tmp->ptr;
}

static const char* tmpf_base[MAX_TMPF_TYPE] = {
    "tmp",
    "src",
    "frame",
    "cache",
    "cookie",
    "hist",
};
static unsigned int tmpf_seq[MAX_TMPF_TYPE];

Str tmpfname(int type, const char* ext)
{
    Str tmpf;

    tmpf = Sprintf("%s/w3m%s%d-%d%s",
        type == TMPF_HIST ? rc_dir : tmp_dir,
        tmpf_base[type],
        CurrentPid, tmpf_seq[type]++, (ext) ? ext : "");
    pushText(fileToDelete, tmpf->ptr);
    return tmpf;
}

static const char* monthtbl[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

static int
get_day(char** s)
{
    Str tmp = Strnew();
    int day;
    char* ss = *s;

    if (!**s)
        return -1;

    while (**s && IS_DIGIT(**s))
        Strcat_char(tmp, *((*s)++));

    day = atoi(tmp->ptr);

    if (day < 1 || day > 31) {
        *s = ss;
        return -1;
    }
    return day;
}

static int
get_month(char** s)
{
    Str tmp = Strnew();
    int mon;
    char* ss = *s;

    if (!**s)
        return -1;

    while (**s && IS_DIGIT(**s))
        Strcat_char(tmp, *((*s)++));
    if (tmp->length > 0) {
        mon = atoi(tmp->ptr);
    } else {
        while (**s && IS_ALPHA(**s))
            Strcat_char(tmp, *((*s)++));
        for (mon = 1; mon <= 12; mon++) {
            if (strncmp(tmp->ptr, monthtbl[mon - 1], 3) == 0)
                break;
        }
    }
    if (mon < 1 || mon > 12) {
        *s = ss;
        return -1;
    }
    return mon;
}

static int
get_year(char** s)
{
    Str tmp = Strnew();
    int year;
    char* ss = *s;

    if (!**s)
        return -1;

    while (**s && IS_DIGIT(**s))
        Strcat_char(tmp, *((*s)++));
    if (tmp->length != 2 && tmp->length != 4) {
        *s = ss;
        return -1;
    }

    year = atoi(tmp->ptr);
    if (tmp->length == 2) {
        if (year >= 70)
            year += 1900;
        else
            year += 2000;
    }
    return year;
}

static int
get_time(char** s, int* hour, int* min, int* sec)
{
    Str tmp = Strnew();
    char* ss = *s;

    if (!**s)
        return -1;

    while (**s && IS_DIGIT(**s))
        Strcat_char(tmp, *((*s)++));
    if (**s != ':') {
        *s = ss;
        return -1;
    }
    *hour = atoi(tmp->ptr);

    (*s)++;
    Strclear(tmp);
    while (**s && IS_DIGIT(**s))
        Strcat_char(tmp, *((*s)++));
    if (**s != ':') {
        *s = ss;
        return -1;
    }
    *min = atoi(tmp->ptr);

    (*s)++;
    Strclear(tmp);
    while (**s && IS_DIGIT(**s))
        Strcat_char(tmp, *((*s)++));
    *sec = atoi(tmp->ptr);

    if (*hour < 0 || *hour >= 24 || *min < 0 || *min >= 60 || *sec < 0 || *sec >= 60) {
        *s = ss;
        return -1;
    }
    return 0;
}

static int
get_zone(char** s, int* z_hour, int* z_min)
{
    Str tmp = Strnew();
    int zone;
    char* ss = *s;

    if (!**s)
        return -1;

    if (**s == '+' || **s == '-')
        Strcat_char(tmp, *((*s)++));
    while (**s && IS_DIGIT(**s))
        Strcat_char(tmp, *((*s)++));
    if (!(tmp->length == 4 && IS_DIGIT(*ss)) && !(tmp->length == 5 && (*ss == '+' || *ss == '-'))) {
        *s = ss;
        return -1;
    }

    zone = atoi(tmp->ptr);
    *z_hour = zone / 100;
    *z_min = zone - (zone / 100) * 100;
    return 0;
}

/* RFC 1123 or RFC 850 or ANSI C asctime() format string -> time_t */
time_t
mymktime(char* timestr)
{
    char* s;
    int day, mon, year, hour, min, sec, z_hour = 0, z_min = 0;

    if (!(timestr && *timestr))
        return -1;
    s = timestr;

#ifdef DEBUG
    fprintf(stderr, "mktime: %s\n", timestr);
#endif /* DEBUG */

    while (*s && IS_ALPHA(*s))
        s++;
    while (*s && !IS_ALNUM(*s))
        s++;

    if (IS_DIGIT(*s)) {
        /* RFC 1123 or RFC 850 format */
        if ((day = get_day(&s)) == -1)
            return -1;

        while (*s && !IS_ALNUM(*s))
            s++;
        if ((mon = get_month(&s)) == -1)
            return -1;

        while (*s && !IS_DIGIT(*s))
            s++;
        if ((year = get_year(&s)) == -1)
            return -1;

        while (*s && !IS_DIGIT(*s))
            s++;
        if (!*s) {
            hour = 0;
            min = 0;
            sec = 0;
        } else {
            if (get_time(&s, &hour, &min, &sec) == -1)
                return -1;
            while (*s && !IS_DIGIT(*s) && *s != '+' && *s != '-')
                s++;
            get_zone(&s, &z_hour, &z_min);
        }
    } else {
        /* ANSI C asctime() format. */
        while (*s && !IS_ALNUM(*s))
            s++;
        if ((mon = get_month(&s)) == -1)
            return -1;

        while (*s && !IS_DIGIT(*s))
            s++;
        if ((day = get_day(&s)) == -1)
            return -1;

        while (*s && !IS_DIGIT(*s))
            s++;
        if (get_time(&s, &hour, &min, &sec) == -1)
            return -1;

        while (*s && !IS_DIGIT(*s))
            s++;
        if ((year = get_year(&s)) == -1)
            return -1;
    }
#ifdef DEBUG
    fprintf(stderr,
        "year=%d month=%d day=%d hour:min:sec=%d:%d:%d zone=%d:%d\n", year,
        mon, day, hour, min, sec, z_hour, z_min);
#endif /* DEBUG */

    mon -= 3;
    if (mon < 0) {
        mon += 12;
        year--;
    }
    day += (year - 1968) * 1461 / 4;
    day += ((((mon * 153) + 2) / 5) - 672);
    hour -= z_hour;
    min -= z_min;
    return (time_t)((day * 60 * 60 * 24) + (hour * 60 * 60) + (min * 60) + sec);
}

void (*mySignal(int signal_number, void (*action)(int)))(int)
{
#ifdef SA_RESTART
    struct sigaction new_action, old_action;

    sigemptyset(&new_action.sa_mask);
    new_action.sa_handler = action;
    if (signal_number == SIGALRM)
        new_action.sa_flags = 0;
    else
        new_action.sa_flags = SA_RESTART;
    sigaction(signal_number, &new_action, &old_action);
    return (old_action.sa_handler);
#else
    return (signal(signal_number, action));
#endif
}

static const char Base64Table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

Str base64_encode(const char* src, size_t len)
{
#define Strcatc(x, y) ((x)->ptr[(x)->length++] = (y))
    Str dest;
    const unsigned char *in, *endw, *s;
    unsigned long j;
    size_t k;

    s = (const unsigned char*)src;

    k = len;
    if (k % 3)
        k += 3 - (k % 3);

    k = k / 3 * 4;

    if (!len || k + 1 < len)
        return Strnew();

    dest = Strnew_size(k);
    if (dest->area_size <= k) {
        Strfree(dest);
        return Strnew();
    }

    in = s;

    endw = s + len - 2;

    while (in < endw) {
        j = *in++;
        j = j << 8 | *in++;
        j = j << 8 | *in++;

        Strcatc(dest, Base64Table[(j >> 18) & 0x3f]);
        Strcatc(dest, Base64Table[(j >> 12) & 0x3f]);
        Strcatc(dest, Base64Table[(j >> 6) & 0x3f]);
        Strcatc(dest, Base64Table[j & 0x3f]);
    }

    if (s + len - in) {
        j = *in++;
        if (s + len - in) {
            j = j << 8 | *in++;
            j = j << 8;
            Strcatc(dest, Base64Table[(j >> 18) & 0x3f]);
            Strcatc(dest, Base64Table[(j >> 12) & 0x3f]);
            Strcatc(dest, Base64Table[(j >> 6) & 0x3f]);
        } else {
            j = j << 8;
            j = j << 8;
            Strcatc(dest, Base64Table[(j >> 18) & 0x3f]);
            Strcatc(dest, Base64Table[(j >> 12) & 0x3f]);
            Strcatc(dest, '=');
        }
        Strcatc(dest, '=');
    }
    dest->ptr[dest->length] = '\0';
    return dest;
#undef Strcatc
}
