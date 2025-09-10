#include "subprocess.h"
#include "mysignal.h"
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

// #define DEV_NULL_PATH "nul"
#define DEV_NULL_PATH "/dev/null"

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

void reset_signals(void)
{
#ifdef WIN32
#else
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
    mySignal(SIGCHLD, SIG_IGN);
    mySignal(SIGPIPE, SIG_IGN);
#endif
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
    // QuietMessage = true;
}

pid_t open_pipe_rw(FILE** fr, FILE** fw)
{
    int fdr[2];
    int fdw[2];
    if (fr && pipe(fdr) < 0)
        goto err0;
    if (fw && pipe(fdw) < 0)
        goto err1;

    // flush_tty();
    pid_t pid = fork();
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

void myExec(const char* command)
{
    mySignal(SIGINT, SIG_DFL);
    execl("/bin/sh", "sh", "-c", command, NULL);
    exit(127);
}

void mySystem(const char* command, bool background)
{
    if (background) {
        // flush_tty();
        if (!fork()) {
            setup_child(false, 0, -1);
            myExec(command);
        }
    } else {
        system(command);
    }
}
