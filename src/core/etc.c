#include "alloc.h"
#define _GNU_SOURCE
#include "etc.h"
#include "convertline.h"
#include "indep.h"
#include "istream.h"
#include "readbuffer.h"
#include "auth.h"
#include "quote.h"
#include "local.h"
#include "display.h"
#include "buffer.h"
#include "mysignal.h"
#include "ui.h"
#include "rc.h"
#include "ctrlcode.h"
#include "myctype.h"
#include "HtmlTag.h"
#include "hash.h"
#include "screen.h"
#include <pwd.h>
#include <stdlib.h>
#include <strings.h>
#include <wtf.h>
#include <fcntl.h>
#include <sys/types.h>
#include <time.h>
#if defined(HAVE_WAITPID) || defined(HAVE_WAIT3)
#include <sys/wait.h>
#endif
#include <signal.h>
#include <unistd.h>

int nextpage_topline = (false);
int disable_secret_security_check = (false);

int columnSkip(Buffer* buf, int offset)
{
    int i, maxColumn;
    int column = buf->currentColumn + offset;
    int nlines = getScreen()->ROWS + 1;
    Line* l;

    maxColumn = 0;
    for (i = 0, l = buf->topLine; i < nlines && l != NULL; i++, l = l->next) {
        if (l->width < 0)
            l->width = COLPOS(l, l->len);
        if (l->width - 1 > maxColumn)
            maxColumn = l->width - 1;
    }
    maxColumn -= getScreen()->COLS - 1;
    if (column < maxColumn)
        maxColumn = column;
    if (maxColumn < 0)
        maxColumn = 0;

    if (buf->currentColumn == maxColumn)
        return 0;
    buf->currentColumn = maxColumn;
    return 1;
}

int columnPos(Line* line, int column)
{
    int i;

    for (i = 1; i < line->len; i++) {
        if (COLPOS(line, i) > column)
            break;
    }
    for (i--; i > 0 && line->propBuf[i] & PC_WCHAR2; i--)
        ;
    return i;
}

Line* lineSkip(Buffer* buf, Line* line, int offset, int last)
{
    int i;
    Line* l;

    l = currentLineSkip(buf, line, offset, last);
    if (!nextpage_topline)
        for (i = getScreen()->ROWS - 1 - (buf->lastLine->linenumber - l->linenumber);
            i > 0 && l->prev != NULL; i--, l = l->prev)
            ;
    return l;
}

Line* currentLineSkip(Buffer* buf, Line* line, int offset, int last)
{
    int i, n;
    Line* l = line;

    if (offset == 0)
        return l;
    if (offset > 0)
        for (i = 0; i < offset && l->next != NULL; i++, l = l->next)
            ;
    else
        for (i = 0; i < -offset && l->prev != NULL; i++, l = l->prev)
            ;
    return l;
}

#define MAX_CMD_LEN 128

int gethtmlcmd(char** s)
{
    extern Hash_si tagtable;
    char cmdstr[MAX_CMD_LEN];
    char* p = cmdstr;
    char* save = *s;
    int cmd;

    (*s)++;
    /* first character */
    if (IS_ALNUM(**s) || **s == '_' || **s == '/') {
        *(p++) = TOLOWER(**s);
        (*s)++;
    } else
        return HTML_UNKNOWN;
    if (p[-1] == '/')
        SKIP_BLANKS(*s);
    while ((IS_ALNUM(**s) || **s == '_') && p - cmdstr < MAX_CMD_LEN) {
        *(p++) = TOLOWER(**s);
        (*s)++;
    }
    if (p - cmdstr == MAX_CMD_LEN) {
        /* buffer overflow: perhaps caused by bad HTML source */
        *s = save + 1;
        return HTML_UNKNOWN;
    }
    *p = '\0';

    /* hash search */
    cmd = getHash_si(&tagtable, cmdstr, HTML_UNKNOWN);
    while (**s && **s != '>')
        (*s)++;
    if (**s == '>')
        (*s)++;
    return cmd;
}

char* lastFileName(char* path)
{
    char *p, *q;

    p = q = path;
    while (*p != '\0') {
        if (*p == '/')
            q = p + 1;
        p++;
    }

    return allocStr(q, -1);
}

#ifdef USE_INCLUDED_SRAND48
static unsigned long R1 = 0x1234abcd;
static unsigned long R2 = 0x330e;
#define A1 0x5deec
#define A2 0xe66d
#define C 0xb

void srand48(long seed)
{
    R1 = (unsigned long)seed;
    R2 = 0x330e;
}

long lrand48(void)
{
    R1 = (A1 * R1 << 16) + A1 * R2 + A2 * R1 + ((A2 * R2 + C) >> 16);
    R2 = (A2 * R2 + C) & 0xffff;
    return (long)(R1 >> 1);
}
#endif

char* mydirname(char* s)
{
    char* p = s;
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

int next_status(char c, int* status)
{
    switch (*status) {
    case R_ST_NORMAL:
        if (c == '<') {
            *status = R_ST_TAG0;
            return 0;
        } else if (c == '&') {
            *status = R_ST_AMP;
            return 1;
        } else
            return 1;
        break;
    case R_ST_TAG0:
        if (c == '!') {
            *status = R_ST_CMNT1;
            return 0;
        }
        *status = R_ST_TAG;
        /* continues to next case */
    case R_ST_TAG:
        if (c == '>')
            *status = R_ST_NORMAL;
        else if (c == '=')
            *status = R_ST_EQL;
        return 0;
    case R_ST_EQL:
        if (c == '"')
            *status = R_ST_DQUOTE;
        else if (c == '\'')
            *status = R_ST_QUOTE;
        else if (IS_SPACE(c))
            *status = R_ST_EQL;
        else if (c == '>')
            *status = R_ST_NORMAL;
        else
            *status = R_ST_VALUE;
        return 0;
    case R_ST_QUOTE:
        if (c == '\'')
            *status = R_ST_TAG;
        return 0;
    case R_ST_DQUOTE:
        if (c == '"')
            *status = R_ST_TAG;
        return 0;
    case R_ST_VALUE:
        if (c == '>')
            *status = R_ST_NORMAL;
        else if (IS_SPACE(c))
            *status = R_ST_TAG;
        return 0;
    case R_ST_AMP:
        if (c == ';') {
            *status = R_ST_NORMAL;
            return 0;
        } else if (c != '#' && !IS_ALNUM(c) && c != '_') {
            /* something's wrong! */
            *status = R_ST_NORMAL;
            return 0;
        } else
            return 0;
    case R_ST_CMNT1:
        switch (c) {
        case '-':
            *status = R_ST_CMNT2;
            break;
        case '>':
            *status = R_ST_NORMAL;
            break;
        case 'D':
        case 'd':
            /* could be a !doctype */
            *status = R_ST_TAG;
            break;
        default:
            *status = R_ST_IRRTAG;
        }
        return 0;
    case R_ST_CMNT2:
        switch (c) {
        case '-':
            *status = R_ST_CMNT;
            break;
        case '>':
            *status = R_ST_NORMAL;
            break;
        default:
            *status = R_ST_IRRTAG;
        }
        return 0;
    case R_ST_CMNT:
        if (c == '-')
            *status = R_ST_NCMNT1;
        return 0;
    case R_ST_NCMNT1:
        if (c == '-')
            *status = R_ST_NCMNT2;
        else
            *status = R_ST_CMNT;
        return 0;
    case R_ST_NCMNT2:
        switch (c) {
        case '>':
            *status = R_ST_NORMAL;
            break;
        case '-':
            *status = R_ST_NCMNT2;
            break;
        default:
            if (IS_SPACE(c))
                *status = R_ST_NCMNT3;
            else
                *status = R_ST_CMNT;
            break;
        }
        break;
    case R_ST_NCMNT3:
        switch (c) {
        case '>':
            *status = R_ST_NORMAL;
            break;
        case '-':
            *status = R_ST_NCMNT1;
            break;
        default:
            if (IS_SPACE(c))
                *status = R_ST_NCMNT3;
            else
                *status = R_ST_CMNT;
            break;
        }
        return 0;
    case R_ST_IRRTAG:
        if (c == '>')
            *status = R_ST_NORMAL;
        return 0;
    }
    /* notreached */
    return 0;
}

int read_token(Str buf, char** instr, int* status, int pre, int append)
{
    char* p;
    int prev_status;

    if (!append)
        Strclear(buf);
    if (**instr == '\0')
        return 0;
    for (p = *instr; *p; p++) {
        /* Drop Unicode soft hyphen */
        if (*(unsigned char*)p == 0210
            && *(unsigned char*)(p + 1) == 0200
            && *(unsigned char*)(p + 2) == 0201
            && *(unsigned char*)(p + 3) == 0255) {
            p += 3;
            continue;
        }

        prev_status = *status;
        next_status(*p, status);
        switch (*status) {
        case R_ST_NORMAL:
            if (prev_status == R_ST_AMP && *p != ';') {
                p--;
                break;
            }
            if (prev_status == R_ST_NCMNT2 || prev_status == R_ST_NCMNT3 || prev_status == R_ST_IRRTAG || prev_status == R_ST_CMNT1) {
                if (prev_status == R_ST_CMNT1 && !append && !pre)
                    Strclear(buf);
                if (pre)
                    Strcat_char(buf, *p);
                p++;
                goto proc_end;
            }
            Strcat_char(buf, (!pre && IS_SPACE(*p)) ? ' ' : *p);
            if (ST_IS_REAL_TAG(prev_status)) {
                *instr = p + 1;
                if (buf->length < 2 || buf->ptr[buf->length - 2] != '<' || buf->ptr[buf->length - 1] != '>')
                    return 1;
                Strshrink(buf, 2);
            }
            break;
        case R_ST_TAG0:
        case R_ST_TAG:
            if (prev_status == R_ST_NORMAL && p != *instr) {
                *instr = p;
                *status = prev_status;
                return 1;
            }
            if (*status == R_ST_TAG0 && !REALLY_THE_BEGINNING_OF_A_TAG(p)) {
                /* it seems that this '<' is not a beginning of a tag */
                /*
                 * Strcat_charp(buf, "&lt;");
                 */
                Strcat_char(buf, '<');
                *status = R_ST_NORMAL;
            } else
                Strcat_char(buf, *p);
            break;
        case R_ST_EQL:
        case R_ST_QUOTE:
        case R_ST_DQUOTE:
        case R_ST_VALUE:
        case R_ST_AMP:
            Strcat_char(buf, *p);
            break;
        case R_ST_CMNT:
        case R_ST_IRRTAG:
            if (pre)
                Strcat_char(buf, *p);
            else if (!append)
                Strclear(buf);
            break;
        case R_ST_CMNT1:
        case R_ST_CMNT2:
        case R_ST_NCMNT1:
        case R_ST_NCMNT2:
        case R_ST_NCMNT3:
            /* do nothing */
            if (pre)
                Strcat_char(buf, *p);
            break;
        }
    }
proc_end:
    *instr = p;
    return 1;
}

Str correct_irrtag(int status)
{
    char c;
    Str tmp = Strnew();

    while (status != R_ST_NORMAL) {
        switch (status) {
        case R_ST_CMNT: /* required "-->" */
        case R_ST_NCMNT1: /* required "->" */
            c = '-';
            break;
        case R_ST_NCMNT2:
        case R_ST_NCMNT3:
        case R_ST_IRRTAG:
        case R_ST_CMNT1:
        case R_ST_CMNT2:
        case R_ST_TAG:
        case R_ST_TAG0:
        case R_ST_EQL: /* required ">" */
        case R_ST_VALUE:
            c = '>';
            break;
        case R_ST_QUOTE:
            c = '\'';
            break;
        case R_ST_DQUOTE:
            c = '"';
            break;
        case R_ST_AMP:
            c = ';';
            break;
        default:
            return tmp;
        }
        next_status(c, &status);
        Strcat_char(tmp, c);
    }
    return tmp;
}

/* FIXME: gettextize? */
#define FILE_IS_READABLE_MSG "SECURITY NOTE: file %s must not be accessible by others"

FILE* openSecretFile(char* fname)
{
    if (fname == NULL)
        return NULL;

    const char* efname = expandPath(fname);
    struct stat st;
    if (stat(efname, &st) < 0)
        return NULL;

    /* check permissions, if group or others readable or writable,
     * refuse it, because it's insecure.
     *
     * XXX: disable_secret_security_check will introduce some
     *    security issues, but on some platform such as Windows
     *    it's not possible (or feasible) to disable group|other
     *    readable and writable.
     *   [w3m-dev 03368][w3m-dev 03369][w3m-dev 03370]
     */
    if (disable_secret_security_check)
        /* do nothing */;
    else if ((st.st_mode & (S_IRWXG | S_IRWXO)) != 0) {
        message(getUI(), MSG_INFO, Sprintf(FILE_IS_READABLE_MSG, fname)->ptr);
        // refresh(ttyWriter());
        sleep(2);
        return NULL;
    }

    return fopen(efname, "r");
}

void loadPasswd(void)
{
    FILE* fp = openSecretFile(passwd_file);
    if (fp != NULL) {
        parsePasswd(fp, 0);
        fclose(fp);
    }

    /* for FTP */
    fp = openSecretFile("~/.netrc");
    if (fp != NULL) {
        parsePasswd(fp, 1);
        fclose(fp);
    }
    return;
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

#define SETPGRP_VOID 1
#ifdef SETPGRP_VOID
#define SETPGRP() setpgrp()
#else
#define SETPGRP() setpgrp(0, 0)
#endif
void setup_child(int child, int i, int f)
{
    reset_signals();
    mySignal(SIGINT, SIG_IGN);
    if (!child)
        SETPGRP();
    /*
     * I don't know why but close_tty() sometimes interrupts loadGeneralFile() in loadImage()
     * and corrupt image data can be cached in ~/.w3m.
     */
    close_all_fds_except(i, f);
    QuietMessage = true;
    TrapSignal = false;
}

pid_t open_pipe_rw(FILE** fr, FILE** fw)
{
    int fdr[2];
    int fdw[2];
    pid_t pid;

    if (fr && pipe(fdr) < 0)
        goto err0;
    if (fw && pipe(fdw) < 0)
        goto err1;

    // flush_tty();
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
    } else {
        if (fr) {
            close(fdr[1]);
            if (*fr == stdin)
                dup2(fdr[0], 0);
            else
                *fr = fdopen(fdr[0], "r");
        }
        if (fw) {
            close(fdw[0]);
            if (*fw == stdout)
                dup2(fdw[1], 1);
            else
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

void myExec(char* command)
{
    mySignal(SIGINT, SIG_DFL);
    execl("/bin/sh", "sh", "-c", command, NULL);
    exit(127);
}

void mySystem(char* command, int background)
{
    if (background) {
        // flush_tty();
        if (!fork()) {
            setup_child(false, 0, -1);
            myExec(command);
        }
    } else
        system(command);
}

Str myExtCommand(char* cmd, char* arg, int redirect)
{
    Str tmp = NULL;
    char* p;
    int set_arg = false;

    for (p = cmd; *p; p++) {
        if (*p == '%' && *(p + 1) == 's' && !set_arg) {
            if (tmp == NULL)
                tmp = Strnew_charp_n(cmd, (int)(p - cmd));
            Strcat_charp(tmp, arg);
            set_arg = true;
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

Str myEditor(char* cmd, char* file, int line)
{
    Str tmp = NULL;
    char* p;
    int set_file = false, set_line = false;

    for (p = cmd; *p; p++) {
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

const char* expandName(const char* name)
{
    if (name == NULL)
        return NULL;

    struct passwd* passent;
    Str extpath = NULL;

    const char* p = name;
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

char* url_unquote_conv(char* url, wc_ces charset)
{
    wc_uint8 old_auto_detect = WcOption.auto_detect;
    Str tmp;
    tmp = Str_url_unquote(Strnew_charp(url), false, true);
    if (!charset || charset == WC_CES_US_ASCII)
        charset = SystemCharset;
    WcOption.auto_detect = WC_OPT_DETECT_ON;
    tmp = convertLine(NULL, tmp, RAW_MODE, &charset, charset, InnerCharset);
    WcOption.auto_detect = old_auto_detect;
    return tmp->ptr;
}

static char* monthtbl[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

static int
get_day(const char** s)
{
    Str tmp = Strnew();
    int day;
    const char* ss = *s;

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
get_month(const char** s)
{
    Str tmp = Strnew();
    int mon;
    const char* ss = *s;

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
get_year(const char** s)
{
    Str tmp = Strnew();
    int year;
    const char* ss = *s;

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
get_time(const char** s, int* hour, int* min, int* sec)
{
    Str tmp = Strnew();
    const char* ss = *s;

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
get_zone(const char** s, int* z_hour, int* z_min)
{
    Str tmp = Strnew();
    int zone;
    const char* ss = *s;

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
mymktime(const char* timestr)
{
    if (!(timestr && *timestr))
        return -1;

    const char* s = timestr;
    while (*s && IS_ALPHA(*s))
        s++;
    while (*s && !IS_ALNUM(*s))
        s++;

    int day, mon, year, hour, min, sec, z_hour = 0, z_min = 0;
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
