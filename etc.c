#include "etc.h"
#include "signal_jmp.h"
#include "tui.h"
#include "fm.h"
#include "terms.h"
#include "w3m_runtime.h"
#include "buffer.h"
#include "indep.h"
#include <pwd.h>
#include <gcstr/gcstr.h>
#include "html.h"
#include "local_cgi.h"

#include <fcntl.h>
#include <sys/types.h>
#include <time.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>

TextList* fileToDelete = 0;

int columnSkip(struct Buffer* buf, int offset)
{
    int i, maxColumn;
    int column = buf->currentColumn + offset;
    int nlines = buf->LINES + 1;
    struct Line* l;

    maxColumn = 0;
    for (i = 0, l = buf->topLine; i < nlines && l != NULL; i++, l = l->next) {
        if (l->width < 0)
            l->width = COLPOS(l, l->len);
        if (l->width - 1 > maxColumn)
            maxColumn = l->width - 1;
    }
    maxColumn -= buf->COLS - 1;
    if (column < maxColumn)
        maxColumn = column;
    if (maxColumn < 0)
        maxColumn = 0;

    if (buf->currentColumn == maxColumn)
        return 0;
    buf->currentColumn = maxColumn;
    return 1;
}

struct Line* lineSkip(struct Buffer* buf, struct Line* line, int offset, int last)
{
    int i;
    struct Line* l;

    l = currentLineSkip(buf, line, offset, last);
    if (!nextpage_topline)
        for (i = buf->LINES - 1 - (buf->lastLine->linenumber - l->linenumber);
            i > 0 && l->prev != NULL; i--, l = l->prev)
            ;
    return l;
}

struct Line* currentLineSkip(struct Buffer* buf, struct Line* line, int offset, int last)
{
    int i, n;
    struct Line* l = line;

    if (buf->pagerSource && !(buf->bufferprop & BP_CLOSE)) {
        n = line->linenumber + offset + buf->LINES;
        if (buf->lastLine->linenumber < n)
            getNextPage(buf, n - buf->lastLine->linenumber);
        while ((last || (buf->lastLine->linenumber < n)) && (getNextPage(buf, 1) != NULL))
            ;
        if (last)
            l = buf->lastLine;
    }

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



#ifndef HAVE_STRERROR
char* strerror(int errno)
{
    extern char* sys_errlist[];
    return sys_errlist[errno];
}
#endif /* not HAVE_STRERROR */

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

/* get last modified time */
char* last_modified(struct Buffer* buf)
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







pid_t open_pipe_rw(FILE** fr, FILE** fw)
{
    int fdr[2];
    int fdw[2];
    pid_t pid;

    if (fr && pipe(fdr) < 0)
        goto err0;
    if (fw && pipe(fdw) < 0)
        goto err1;

    tty_flush();
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

Str myExtCommand(char* cmd, char* arg, int redirect)
{
    Str tmp = NULL;
    char* p;
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

Str myEditor(char* cmd, char* file, int line)
{
    Str tmp = NULL;
    char* p;
    int set_file = FALSE, set_line = FALSE;

    for (p = cmd; *p; p++) {
        if (*p == '%' && *(p + 1) == 's' && !set_file) {
            if (tmp == NULL)
                tmp = Strnew_charp_n(cmd, (int)(p - cmd));
            Strcat_charp(tmp, file);
            set_file = TRUE;
            p++;
        } else if (*p == '%' && *(p + 1) == 'd' && !set_line && line > 0) {
            if (tmp == NULL)
                tmp = Strnew_charp_n(cmd, (int)(p - cmd));
            Strcat(tmp, Sprintf("%d", line));
            set_line = TRUE;
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

int is_localhost(const char* host)
{
    if (!host || !strcasecmp(host, "localhost") || !strcmp(host, "127.0.0.1") || (HostName && !strcasecmp(host, HostName)) || !strcmp(host, "[::1]"))
        return TRUE;
    return FALSE;
}

char* file_to_url(char* file)
{
    Str tmp;
    char* drive = NULL;

    if (!(file = expandPath(file)))
        return NULL;
    if (IS_ALPHA(file[0]) && file[1] == ':') {
        drive = allocStr(file, 2);
        file += 2;
    } else
        if (file[0] != '/') {
        tmp = Strnew_charp(CurrentDir);
        if (Strlastchar(tmp) != '/')
            Strcat_char(tmp, '/');
        Strcat_charp(tmp, file);
        file = tmp->ptr;
    }
    tmp = Strnew_charp("file://");
    if (drive)
        Strcat_charp(tmp, drive);
    Strcat_charp(tmp, file_quote(cleanupName(file)));
    return tmp->ptr;
}

static char* tmpf_base[MAX_TMPF_TYPE] = {
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
    char* dir;

    switch (type) {
    case TMPF_HIST:
        dir = rc_dir;
        break;
    case TMPF_DFL:
    case TMPF_COOKIE:
    case TMPF_SRC:
    case TMPF_FRAME:
    case TMPF_CACHE:
    default:
        dir = tmp_dir;
    }

    tmpf = Sprintf("%s/w3m%s%d-%d%s",
        dir,
        tmpf_base[type],
        CurrentPid, tmpf_seq[type]++, (ext) ? ext : "");
    pushText(fileToDelete, tmpf->ptr);
    return tmpf;
}

static char* monthtbl[] = {
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
mymktime(const char* timestr)
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


