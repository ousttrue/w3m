#include "file_copy.h"
#include "keymap.h"
#include "subprocess.h"
#include "linein.h"
#include "history.h"
#include "indep.h"
#include "str_util.h"
#include "myctype.h"
#include "istream.h"
#include "progress.h"
#include "tmpfile.h"
#include "tty.h"
#include "downloadlist.h"
#include <Str.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utime.h>

bool PermitSaveToPipe = (false);

bool notExistsOrOverWrite(const char* path)
{
    struct stat st;
    if (stat(path, &st) < 0) {
        // not exists
        return true;
    }

    const char* ans = inputAnswer("File exists. Overwrite? (y/n)");
    if (ans && TOLOWER(*ans) == 'y') {
        // can overwrite
        return true;
    }

    return false;
}

static bool canCopyFile(const char* path1, const char* path2)
{
    if (*path2 == '|' && PermitSaveToPipe)
        return true;

    struct stat st1, st2;
    if ((stat(path1, &st1) == 0) && (stat(path2, &st2) == 0))
        if (st1.st_ino == st2.st_ino)
            return false;

    return true;
}

#define SAVE_BUF_SIZE 1536

static int _MoveFile(const char* path1, const char* path2)
{
    InputStream f1;
    FILE* f2;
    int is_pipe;
    long long linelen = 0, trbyte = 0;
    char* buf = NULL;
    int count;

    f1 = openIS(path1);
    if (f1 == NULL)
        return -1;
    if (*path2 == '|' && PermitSaveToPipe) {
        is_pipe = true;
        f2 = popen(path2 + 1, "w");
    } else {
        is_pipe = false;
        f2 = fopen(path2, "wb");
    }
    if (f2 == NULL) {
        ISclose(f1);
        return -1;
    }

    int current_content_length = 0;
    buf = NewWithoutGC_N(char, SAVE_BUF_SIZE);
    while ((count = ISread_n(f1, buf, SAVE_BUF_SIZE)) > 0) {
        fwrite(buf, 1, count, f2);
        linelen += count;
        showProgress(current_content_length, &linelen, &trbyte);
    }
    xfree(buf);
    ISclose(f1);
    if (is_pipe)
        pclose(f2);
    else
        fclose(f2);
    return 0;
}

int setModtime(const char* path, time_t modtime)
{
    struct utimbuf t;
    struct stat st;
    if (stat(path, &st) == 0)
        t.actime = st.st_atime;
    else
        t.actime = time(NULL);
    t.modtime = modtime;
    return utime(path, &t);
}

int _doFileCopy(const char* tmpf, const char* defstr, int download)
{
    Str msg;
    // Str filen;
    char *p, *q = NULL;
    pid_t pid;
    char* lock;
    struct stat st;
    long long size = 0;
    bool is_pipe = false;

    // if (fmInitialized)
    {
        p = searchKeyData();
        if (p == NULL || *p == '\0') {
            /* FIXME: gettextize? */
            q = inputLineHist(getUI(), "(Download)Save file to: ",
                defstr, IN_COMMAND, SaveHist);
            if (q == NULL || *q == '\0')
                return false;
            p = conv_to_system(q);
        }
        if (*p == '|' && PermitSaveToPipe)
            is_pipe = true;
        else {
            if (q) {
                p = unescape_spaces(Strnew_charp(q))->ptr;
                p = conv_to_system(p);
            }
            p = expandPath(p);
            if (!notExistsOrOverWrite(p))
                return -1;
        }
        if (!canCopyFile(tmpf, p)) {
            msg = Sprintf("Can't copy. %s and %s are identical.",
                conv_from_system(tmpf), conv_from_system(p));
            message(getUI(), MSG_ERR, msg->ptr);
            return -1;
        }
        if (!download) {
            if (_MoveFile(tmpf, p) < 0) {
                /* FIXME: gettextize? */
                msg = Sprintf("Can't save to %s", conv_from_system(p));
                message(getUI(), MSG_ERR, msg->ptr);
            }
            return -1;
        }
        lock = tmpfname(TMPF_DFL, ".lock")->ptr;

        symlink(p, lock);

        flush_tty();
        pid = fork();
        if (!pid) {
            setup_child(false, 0, -1);
            if (!_MoveFile(tmpf, p) && PreserveTimestamp && !is_pipe && !stat(tmpf, &st))
                setModtime(p, st.st_mtime);
            unlink(lock);
            exit(0);
        }
        if (!stat(tmpf, &st))
            size = st.st_size;
        addDownloadList(pid, conv_from_system(tmpf), p, lock, size);
    }

    // else {
    //     q = searchKeyData();
    //     if (q == NULL || *q == '\0') {
    //         /* FIXME: gettextize? */
    //         printf("(Download)Save file to: ");
    //         fflush(stdout);
    //         filen = Strfgets(stdin);
    //         if (filen->length == 0)
    //             return -1;
    //         q = filen->ptr;
    //     }
    //     for (p = q + strlen(q) - 1; IS_SPACE(*p); p--)
    //         ;
    //     *(p + 1) = '\0';
    //     if (*q == '\0')
    //         return -1;
    //     p = q;
    //     if (*p == '|' && PermitSaveToPipe)
    //         is_pipe = true;
    //     else {
    //         p = expandPath(p);
    //         if (!notExistsOrOverWrite(p))
    //             return -1;
    //     }
    //     if (checkCopyFile(tmpf, p) < 0) {
    //         /* FIXME: gettextize? */
    //         printf("Can't copy. %s and %s are identical.", tmpf, p);
    //         return -1;
    //     }
    //     if (_MoveFile(tmpf, p) < 0) {
    //         /* FIXME: gettextize? */
    //         printf("Can't save to %s\n", p);
    //         return -1;
    //     }
    //     if (PreserveTimestamp && !is_pipe && !stat(tmpf, &st))
    //         setModtime(p, st.st_mtime);
    // }
    return 0;
}

int doFileMove(const char* tmpf, const char* defstr)
{
    int ret = doFileCopy(tmpf, defstr);
    unlink(tmpf);
    return ret;
}
