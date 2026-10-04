#include "w3m.h"
#include "url.h"
#include "version.h"
#include "http_request.h"
// #include "terms.h"
#include "alloc.h"
#include "form.h"
#include "str_gc.h"
#include "str_const.h"
#include "libwc/status.h"
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

struct wc_option WcOption;

struct W3mEventCallback {
    enum W3M_EVENT event;
    EventCallbackFunc callback;
};
static struct W3mEventCallback event_callbacks[256] = { 0 };
static int event_callback_count = 0;

void w3m_register_callback(enum W3M_EVENT event, EventCallbackFunc callback)
{
    event_callbacks[event_callback_count++] = (struct W3mEventCallback) {
        .event = event,
        .callback = callback,
    };
}

void w3m_init()
{
    WcOption = makeDefaultOption();
}

void w3m_exit(int i)
{
    for (int i = 0;
        i < sizeof(event_callbacks) / sizeof(struct W3mEventCallback); ++i) {
        if (event_callbacks[i].event == W3M_EVENT_ON_EXIT) {
            event_callbacks[i].callback();
        }
    }
    exit(i);
}

const char* CurrentDir;
int CurrentPid;
bool fmInitialized = false;
bool QuietMessage = false;
bool TrapSignal = true;
const char* cgi_bin = NULL;
const char* document_root = NULL;

void set_environ(const char* var, const char* value)
{
    if (var && value)
        setenv(var, value, 1);
}

const char* currentdir(void)
{
    char* path;
#ifdef MAXPATHLEN
    path = NewAtom_N(char, MAXPATHLEN);
    getcwd(path, MAXPATHLEN);
#else
    path = getcwd(NULL, 0);
#endif
    return path;
}

SigActionFunc mySignal(int signal_number, SigActionFunc action)
{
    struct sigaction new_action = {
        .sa_handler = action,
        .sa_flags = (signal_number == SIGALRM) ? 0
                                               : SA_RESTART,
    };
    sigemptyset(&new_action.sa_mask);

    struct sigaction old_action;
    sigaction(signal_number, &new_action, &old_action);

    return old_action.sa_handler;
}

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

void setup_child(bool child, int i, int f)
{
    reset_signals();
    mySignal(SIGINT, SIG_IGN);
#ifndef __MINGW32_VERSION
    if (!child)
        setpgid(0, 0);
#endif /* __MINGW32_VERSION */
    close_all_fds_except(i, f);
    QuietMessage = true;
    fmInitialized = false;
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

void myExec(const char* command)
{
    mySignal(SIGINT, SIG_DFL);
    execl("/bin/sh", "sh", "-c", command, NULL);
    exit(127);
}

int mySystem(const char* command, bool background)
{
    if (background) {
        // flush_tty();
        if (!fork()) {
            setup_child(false, 0, -1);
            myExec(command);
        }
        return 0;
    } else
        return system(command);
}

enum CGIFN_TYPE {
    CGIFN_NORMAL,
    CGIFN_LIBDIR,
    CGIFN_CGIBIN,
};

struct CgiInfo {
    enum CGIFN_TYPE type;
    const char* expanded;
    const char* uri;
    // const char* relative_path;
};

static pStr
checkPath(const char* fn, const char* path)
{
    while (*path) {
        const char* p = strchr(path, ':');
        pStr tmp = expandPath(p ? allocStr_n(path, p - path).ptr : path);
        if (Strlastchar(tmp) != '/')
            Strcat_char(tmp, '/');
        Strcat_charp(tmp, fn);
        struct stat st;
        if (stat(tmp->ptr, &st) == 0)
            return tmp;
        if (!p)
            break;
        path = p + 1;
        while (*path == ':')
            path++;
    }
    return NULL;
}

static struct CgiInfo
cgi_filename(const char* uri)
{
    struct CgiInfo info = {
        .type = 0,
        .expanded = uri,
        .uri = uri,
        // .relative_path = NULL,
    };

    if (cgi_bin && strncmp(uri, "/cgi-bin/", 9) == 0) {
        int offset = 9;
        // if ((info.relative_path = strchr(uri + offset, '/')))
        //     info.uri = allocStr_n(uri, info.relative_path - uri).ptr;
        pStr tmp = checkPath(info.uri + offset, cgi_bin);
        if (!tmp) {
            info.type = CGIFN_NORMAL;
            return info;
        }
        info.expanded = tmp->ptr;
        info.type = CGIFN_CGIBIN;
        return info;
    }

    pStr tmp = Strnew_charp(w3m_lib_dir());
    if (Strlastchar(tmp) != '/')
        Strcat_char(tmp, '/');
    int offset;
    if (strncmp(uri, "/$LIB/", 6) == 0)
        offset = 6;
    else if (strncmp(uri, tmp->ptr, tmp->len) == 0)
        offset = tmp->len;
    else if (*uri == '/' && document_root) {
        pStr tmp2 = Strnew_charp(document_root);
        if (Strlastchar(tmp2) != '/')
            Strcat_char(tmp2, '/');
        Strcat_charp(tmp2, uri + 1);
        if (strncmp(tmp2->ptr, tmp->ptr, tmp->len) != 0) {
            info.type = CGIFN_NORMAL;
            return info;
        }
        uri = tmp2->ptr;
        info.uri = uri;
        offset = tmp->len;
    } else {
        info.type = CGIFN_NORMAL;
        return info;
    }
    // if ((info.relative_path = strchr(uri + offset, '/')))
    //     info.uri = allocStr_n(uri, info.relative_path - uri).ptr;
    Strcat_charp(tmp, info.uri + offset);
    info.expanded = tmp->ptr;
    info.type = CGIFN_LIBDIR;
    return info;
}

static bool
check_local_cgi(const char* file, enum CGIFN_TYPE status)
{
    struct stat st;

    if (status != CGIFN_LIBDIR && status != CGIFN_CGIBIN)
        return false;
    if (stat(file, &st) < 0)
        return false;
    if (S_ISDIR(st.st_mode))
        return false;
    if (st.st_uid == geteuid() ? (st.st_mode & S_IXUSR) : st.st_gid == getegid() ? (st.st_mode & S_IXGRP)
                                                                                 : (st.st_mode & S_IXOTH)) /* executable */
        return true;
    return false;
}

static char* Local_cookie_file = NULL;
static pStr Local_cookie = NULL;

/* setup cookie for local CGI */
pStr localCookie(void)
{
    if (Local_cookie)
        return Local_cookie;
    srand48((long)New(char) + (long)time(NULL));
    Local_cookie = Sprintf("%ld@%s", lrand48(), HostName ? HostName : "localhost");
    return Local_cookie;
}

static void
writeLocalCookie(void)
{
    if (Local_cookie_file)
        return;
    Local_cookie_file = tmpfname(CurrentPid, TMPF_COOKIE, NULL)->ptr;
    set_environ("LOCAL_COOKIE_FILE", Local_cookie_file);
    FILE* f;
    f = fopen(Local_cookie_file, "wb");
    if (!f)
        return;
    localCookie();
    fwrite(Local_cookie->ptr, sizeof(char), Local_cookie->len, f);
    fclose(f);
    chmod(Local_cookie_file, S_IRUSR | S_IWUSR);
}

static void
set_cgi_environ(const char* name, const char* fn, const char* req_uri)
{
    set_environ("SERVER_SOFTWARE", W3M_VERSION);
    set_environ("SERVER_PROTOCOL", "HTTP/1.0");
    set_environ("SERVER_NAME", "localhost");
    set_environ("SERVER_PORT", "80"); /* dummy */
    set_environ("REMOTE_HOST", "localhost");
    set_environ("REMOTE_ADDR", "127.0.0.1");
    set_environ("GATEWAY_INTERFACE", "CGI/1.1");

    set_environ("SCRIPT_NAME", name);
    set_environ("SCRIPT_FILENAME", fn);
    set_environ("REQUEST_URI", req_uri);
}

FILE* localcgi_post(const char* uri,
    const char* qstr, FormList* request, const char* referer)
{
    struct CgiInfo cgi = cgi_filename(uri);
    if (!check_local_cgi(cgi.expanded, cgi.type))
        return NULL;

    writeLocalCookie();
    FILE* fw = NULL;
    const char* tmpf = NULL;
    if (request && request->enctype != FORM_ENCTYPE_MULTIPART) {
        tmpf = tmpfname(CurrentPid, TMPF_DFL, NULL)->ptr;
        fw = fopen(tmpf, "w");
        if (!fw)
            return NULL;
    }
    if (qstr)
        uri = Strnew_m_charp(uri, "?", qstr, NULL)->ptr;
    const char* cgi_dir = mydirname(cgi.expanded)->ptr;
    const char* cgi_basename = mybasename(cgi.expanded);
    FILE* fr = NULL;
    pid_t pid = open_pipe_rw(&fr, NULL); /* open_pipe_rw() forks */
    /* Don't invoke gc after here, or the program might crash in some platforms */
    if (pid < 0) {
        if (fw)
            fclose(fw);
        return NULL;
    } else if (pid) {
        /* parent */
        if (fw)
            fclose(fw);
        return fr;
    }
    /* child */
    setup_child(true, 2, fw ? fileno(fw) : -1);

    set_cgi_environ(cgi.uri, cgi.expanded, uri);
    // if (cgi.relative_path)
    //     set_environ("PATH_INFO", cgi.relative_path);
    if (referer && referer != NO_REFERER)
        set_environ("HTTP_REFERER", referer);
    if (request) {
        set_environ("REQUEST_METHOD", "POST");
        if (qstr)
            set_environ("QUERY_STRING", qstr);
        set_environ("CONTENT_LENGTH", Sprintf("%lu", request->length)->ptr);
        if (request->enctype == FORM_ENCTYPE_MULTIPART) {
            set_environ("CONTENT_TYPE",
                Sprintf("multipart/form-data; boundary=%s",
                    request->boundary)
                    ->ptr);
            if (!freopen(request->body, "r", stdin))
                return NULL;
        } else {
            set_environ("CONTENT_TYPE", "application/x-www-form-urlencoded");
            fwrite(request->body, sizeof(char), request->length, fw);
            fclose(fw);
            if (!freopen(tmpf, "r", stdin))
                return NULL;
        }
    } else {
        set_environ("REQUEST_METHOD", "GET");
        set_environ("QUERY_STRING", qstr ? qstr : "");
        if (!freopen(DEV_NULL_PATH, "r", stdin))
            return NULL;
    }

    if (chdir(cgi_dir) == -1) {
        fprintf(stderr, "failed to chdir to %s: %s\n",
            cgi_dir, strerror(errno));
        exit(1);
    }
    execl(cgi.expanded, cgi_basename, NULL);
    fprintf(stderr, "execl(\"%s\", \"%s\", NULL): %s\n",
        cgi.expanded, cgi_basename, strerror(errno));
    exit(1);

    /*
     * Suppress compiler warning: function might return no value
     * This code is never reached.
     */
    return NULL;
}
