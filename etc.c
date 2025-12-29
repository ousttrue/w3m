#include "etc.h"
#include "indep.h"
#include "alloc.h"
#include "w3m_rc.h"
#include "file.h"
#include "symbol.h"
#include "message.h"
#include "ctrlcode.h"
#include "buffer.h"
#include "fm.h"
#include "myctype.h"
#include "html_tag.h"
#include "local_cgi.h"
#include "hash.h"

#include <libwc/ces.h>

#include <pwd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>

struct auth_pass {
    int bad;
    int is_proxy;
    Str host;
    int port;
    /*    Str file; */
    Str realm;
    Str uname;
    Str pwd;
    struct auth_pass* next;
};

struct auth_pass* passwords = NULL;

int columnSkip(struct Buffer* buf, int offset)
{
    int i, maxColumn;
    int column = buf->currentColumn + offset;
    int nlines = buf->LINES + 1;
    struct Line* l;

    maxColumn = 0;
    for (i = 0, l = buf->doc.topLine; i < nlines && l != NULL; i++, l = l->next) {
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
    if (!getRuntime()->nextpage_topline)
        for (i = buf->LINES - 1 - (buf->doc.lastLine->linenumber - l->linenumber);
            i > 0 && l->prev != NULL; i--, l = l->prev)
            ;
    return l;
}

struct Line* currentLineSkip(struct Buffer* buf, struct Line* line, int offset, int last)
{
    int i;
    struct Line* l = line;

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

int gethtmlcmd(const char** s)
{
    extern Hash_si tagtable;
    char cmdstr[MAX_CMD_LEN];
    char* p = cmdstr;
    const char* save = *s;
    int cmd;

    (*s)++;
    /* first character */
    if (IS_ALNUM(**s) || **s == '_' || **s == '/') {
        *(p++) = TOLOWER(**s);
        (*s)++;
    } else
        return HTML_UNKNOWN;
    if (p[-1] == '/')
        *s = skip_blanks(*s);
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

char* lastFileName(const char* path)
{
    const char *p, *q;
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

char* mybasename(const char* s)
{
    const char* p = s;
    while (*p)
        p++;
    while (s <= p && *p != '/')
        p--;
    if (*p == '/')
        p++;
    else
        p = s;
    return allocStr(p, -1);
}

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

#ifndef HAVE_STRERROR
char* strerror(int errno)
{
    extern char* sys_errlist[];
    return sys_errlist[errno];
}
#endif /* not HAVE_STRERROR */

/*
 * RFC2617: 1.2 Access Authentication Framework
 *
 * The realm value (case-sensitive), in combination with the canonical root
 * URL (the absoluteURI for the server whose abs_path is empty; see section
 * 5.1.2 of RFC2616 ) of the server being accessed, defines the protection
 * space. These realms allow the protected resources on a server to be
 * partitioned into a set of protection spaces, each with its own
 * authentication scheme and/or authorization database.
 *
 */
static void
add_auth_pass_entry(const struct auth_pass* ent, int netrc, int override)
{
    if ((ent->host || netrc) /* netrc accept default (host == NULL) */
        && (ent->is_proxy || ent->realm || netrc)
        && ent->uname && ent->pwd) {
        struct auth_pass* newent = New(struct auth_pass);
        memcpy(newent, ent, sizeof(struct auth_pass));
        if (override) {
            newent->next = passwords;
            passwords = newent;
        } else {
            if (passwords == NULL)
                passwords = newent;
            else if (passwords->next == NULL)
                passwords->next = newent;
            else {
                struct auth_pass* ep = passwords;
                for (; ep->next; ep = ep->next)
                    ;
                ep->next = newent;
            }
        }
    }
    /* ignore invalid entries */
}

static struct auth_pass*
find_auth_pass_entry(char* host, int port, char* realm, char* uname,
    int is_proxy)
{
    struct auth_pass* ent;
    for (ent = passwords; ent != NULL; ent = ent->next) {
        if (ent->is_proxy == is_proxy
            && (ent->bad != TRUE)
            && (!ent->host || !Strcasecmp_charp(ent->host, host))
            && (!ent->port || ent->port == port)
            && (!ent->uname || !uname || !Strcmp_charp(ent->uname, uname))
            && (!ent->realm || !realm || !Strcmp_charp(ent->realm, realm)))
            return ent;
    }
    return NULL;
}

int find_auth_user_passwd(struct Url* pu, char* realm,
    Str* uname, Str* pwd, int is_proxy)
{
    struct auth_pass* ent;

    if (pu->user && pu->pass) {
        *uname = Strnew_charp(pu->user);
        *pwd = Strnew_charp(pu->pass);
        return 1;
    }
    ent = find_auth_pass_entry(pu->host, pu->port, realm, pu->user, is_proxy);
    if (ent) {
        *uname = ent->uname;
        *pwd = ent->pwd;
        return 1;
    }
    return 0;
}

void add_auth_user_passwd(struct Url* pu, char* realm, Str uname, Str pwd,
    int is_proxy)
{
    struct auth_pass ent;
    memset(&ent, 0, sizeof(ent));

    ent.is_proxy = is_proxy;
    ent.host = Strnew_charp(pu->host);
    ent.port = pu->port;
    ent.realm = Strnew_charp(realm);
    ent.uname = uname;
    ent.pwd = pwd;
    add_auth_pass_entry(&ent, 0, 1);
}

void invalidate_auth_user_passwd(struct Url* pu, char* realm, Str uname, Str pwd,
    int is_proxy)
{
    struct auth_pass* ent;
    ent = find_auth_pass_entry(pu->host, pu->port, realm, NULL, is_proxy);
    if (ent) {
        ent->bad = TRUE;
    }
    return;
}

/* passwd */
/*
 * machine <host>
 * host <host>
 * port <port>
 * proxy
 * path <file>	; not used
 * realm <realm>
 * login <login>
 * passwd <passwd>
 * password <passwd>
 */

static Str
next_token(Str arg)
{
    if (arg == NULL || arg->length == 0)
        return NULL;
    Str narg = NULL;
    char* p = arg->ptr;
    char* q = p;
    SKIP_NON_BLANKS(q);
    if (*q != '\0') {
        *q++ = '\0';
        q = (char*)skip_blanks(q);
        if (*q != '\0')
            narg = Strnew_charp(q);
    }
    return narg;
}

static void
parsePasswd(FILE* fp, int netrc)
{
    struct auth_pass ent;
    Str line = NULL;

    bzero(&ent, sizeof(struct auth_pass));
    while (1) {
        Str arg = NULL;
        char* p;

        if (line == NULL || line->length == 0)
            line = Strfgets(fp);
        if (line->length == 0)
            break;
        Strchop(line);
        Strremovefirstspaces(line);
        p = line->ptr;
        if (*p == '#' || *p == '\0') {
            line = NULL;
            continue; /* comment or empty line */
        }
        arg = next_token(line);

        if (!strcmp(p, "machine") || !strcmp(p, "host")
            || (netrc && !strcmp(p, "default"))) {
            add_auth_pass_entry(&ent, netrc, 0);
            bzero(&ent, sizeof(struct auth_pass));
            if (netrc)
                ent.port = 21; /* XXX: getservbyname("ftp"); ? */
            if (strcmp(p, "default") != 0) {
                line = next_token(arg);
                ent.host = arg;
            } else {
                line = arg;
            }
        } else if (!netrc && !strcmp(p, "port") && arg) {
            line = next_token(arg);
            ent.port = atoi(arg->ptr);
        } else if (!netrc && !strcmp(p, "proxy")) {
            ent.is_proxy = 1;
            line = arg;
        } else if (!netrc && !strcmp(p, "path")) {
            line = next_token(arg);
            /* ent.file = arg; */
        } else if (!netrc && !strcmp(p, "realm")) {
            /* XXX: rest of line becomes arg for realm */
            line = NULL;
            ent.realm = arg;
        } else if (!strcmp(p, "login")) {
            line = next_token(arg);
            ent.uname = arg;
        } else if (!strcmp(p, "password") || !strcmp(p, "passwd")) {
            line = next_token(arg);
            ent.pwd = arg;
        } else if (netrc && !strcmp(p, "machdef")) {
            while ((line = Strfgets(fp))->length != 0) {
                if (*line->ptr == '\n')
                    break;
            }
            line = NULL;
        } else if (netrc && !strcmp(p, "account")) {
            /* ignore */
            line = next_token(arg);
        } else {
            /* ignore rest of line */
            line = NULL;
        }
    }
    add_auth_pass_entry(&ent, netrc, 0);
}

/* FIXME: gettextize? */
#define FILE_IS_READABLE_MSG "SECURITY NOTE: file %s must not be accessible by others"

FILE* openSecretFile(char* fname)
{
    char* efname;
    struct stat st;

    if (fname == NULL)
        return NULL;
    efname = expandPath(fname);
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
    if (getRuntime()->disable_secret_security_check)
        /* do nothing */;
    else if ((st.st_mode & (S_IRWXG | S_IRWXO)) != 0) {
        if (fmInitialized()) {
            message(Sprintf(FILE_IS_READABLE_MSG, fname)->ptr, 0, 0);
        } else {
            fputs(Sprintf(FILE_IS_READABLE_MSG, fname)->ptr, stderr);
            fputc('\n', stderr);
        }
        sleep(2);
        return NULL;
    }

    return fopen(efname, "r");
}

void loadPasswd(void)
{
    passwords = NULL;
    FILE* fp = openSecretFile(getRuntime()->passwd_file);
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
char* last_modified(struct Buffer* buf)
{
    if (buf->content.document_header) {
        TextListItem* ti;
        for (ti = buf->content.document_header->first; ti; ti = ti->next) {
            if (strncasecmp(ti->ptr, "Last-modified: ", 15) == 0) {
                return ti->ptr + 15;
            }
        }
        return "unknown";
    } else if (buf->currentURL.scheme == SCM_LOCAL) {
        struct stat st;
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
    if (!child)
        SETPGRP();
    close_all_fds_except(i, f);
    getRuntime()->QuietMessage = TRUE;
    // getRuntime()->fmInitialized = FALSE;
    getRuntime()->TrapSignal = FALSE;
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
#ifndef __MINGW32_VERSION
    if (background) {
#ifndef __EMX__
        flush_tty();
        if (!fork()) {
            setup_child(FALSE, 0, -1);
            myExec(command);
        }
#else
        Str cmd = Strnew_charp("start /f ");
        Strcat_charp(cmd, command);
        system(cmd->ptr);
#endif
    } else
#endif /* __MINGW32_VERSION */
        system(command);
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

#ifdef __MINGW32_VERSION
char* expandName(char* name)
{
    return getenv("HOME");
}
#else
char* expandName(char* name)
{
    char* p;
    struct passwd *passent, *getpwnam(const char*);
    Str extpath = NULL;

    if (name == NULL)
        return NULL;
    p = name;
    if (*p == '/') {
        if ((*(p + 1) == '~' && IS_ALPHA(*(p + 2)))
            && getRuntime()->personal_document_root) {
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
                getRuntime()->personal_document_root, NULL);
            if (*getRuntime()->personal_document_root == '\0' && *p == '/')
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
    if (!host || !strcasecmp(host, "localhost") || !strcmp(host, "127.0.0.1") || (getRuntime()->HostName && !strcasecmp(host, getRuntime()->HostName)) || !strcmp(host, "[::1]"))
        return TRUE;
    return FALSE;
}

char* file_to_url(const char* file)
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

char* url_unquote_conv(char* url, wc_ces charset)
{
    wc_uint8 old_auto_detect = WcOption.auto_detect;

    Str tmp;
    tmp = Str_url_unquote(Strnew_charp(url), FALSE, TRUE);

    if (!charset || charset == WC_CES_US_ASCII)
        charset = getRuntime()->SystemCharset;
    WcOption.auto_detect = WC_OPT_DETECT_ON;
    tmp = convertLine(tmp, RAW_MODE, &charset, charset);
    WcOption.auto_detect = old_auto_detect;

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

Str tmpfname(enum TmpFileTypes type, const char* ext)
{
    Str tmpf;
    const char* dir;

    switch (type) {
    case TMPF_HIST:
        dir = getRuntime()->rc_dir;
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
    const char* s;
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

#ifdef USE_COOKIE
#ifdef INET6
#include <sys/socket.h>
#endif /* INET6 */
#ifndef __MINGW32_VERSION
#include <netdb.h>
#else
#include <winsock.h>
#endif
char* FQDN(char* host)
{
    char* p;
#ifndef INET6
    struct hostent* entry;
#else /* INET6 */
    int* af;
#endif /* INET6 */

    if (host == NULL)
        return NULL;

    if (strcasecmp(host, "localhost") == 0)
        return host;

    for (p = host; *p && *p != '.'; p++)
        ;

    if (*p == '.')
        return host;

    for (af = ai_family_order_table[getRuntime()->DNS_order];; af++) {
        int error;
        struct addrinfo hints;
        struct addrinfo *res, *res0;
        char* namebuf;

        memset(&hints, 0, sizeof(hints));
        hints.ai_flags = AI_CANONNAME;
        hints.ai_family = *af;
        hints.ai_socktype = SOCK_STREAM;
        error = getaddrinfo(host, NULL, &hints, &res0);
        if (error) {
            if (*af == PF_UNSPEC) {
                /* all done */
                break;
            }
            /* try next address family */
            continue;
        }
        for (res = res0; res != NULL; res = res->ai_next) {
            if (res->ai_canonname) {
                /* found */
                namebuf = strdup(res->ai_canonname);
                freeaddrinfo(res0);
                return namebuf;
            }
        }
        freeaddrinfo(res0);
        if (*af == PF_UNSPEC) {
            break;
        }
    }
    /* all failed */
    return NULL;
}

#endif /* USE_COOKIE */

void (*mySignal(int signal_number, void (*action)(int)))(int)
{
#ifdef SA_RESTART
    struct sigaction new_action, old_action;

    sigemptyset(&new_action.sa_mask);
    new_action.sa_handler = action;
    if (signal_number == SIGALRM) {
#ifdef SA_INTERRUPT
        new_action.sa_flags = SA_INTERRUPT;
#else
        new_action.sa_flags = 0;
#endif
    } else {
        new_action.sa_flags = SA_RESTART;
    }
    sigaction(signal_number, &new_action, &old_action);
    return (old_action.sa_handler);
#else
    return (signal(signal_number, action));
#endif
}

static char Base64Table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

Str base64_encode(const char* src, size_t len)
{
    Str dest;
    const unsigned char *in, *endw, *s;
    unsigned long j;
    size_t k;

    s = (unsigned char*)src;

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
    Strnulterm(dest);
    return dest;
}

Str unescape_spaces(Str s)
{
    Str tmp = NULL;
    char* p;

    if (s == NULL)
        return s;
    for (p = s->ptr; *p; p++) {
        if (*p == '\\' && (*(p + 1) == ' ' || *(p + 1) == CTRL_I)) {
            if (tmp == NULL)
                tmp = Strnew_charp_n(s->ptr, (int)(p - s->ptr));
        } else {
            if (tmp)
                Strcat_char(tmp, *p);
        }
    }
    if (tmp)
        return tmp;
    return s;
}
