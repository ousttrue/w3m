#include "download.h"
#include "indep.h"
#include "file.h"
#include "buffer.h"
#include "display.h"
#include "w3m_rc.h"
#include "tab.h"
#include "buffer.h"
#include "image.h"
#include "fm.h"
#include <signal.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>

struct DownloadList* FirstDL = (NULL);
struct DownloadList* LastDL = (NULL);

void sig_child_downloadlist(pid_t pid, int p_stat)
{
    for (struct DownloadList* d = FirstDL; d != NULL; d = d->next) {
        if (d->pid == pid) {
            d->err = WEXITSTATUS(p_stat);
            break;
        }
    }
}

static bool add_download_list = false;

void download_update()
{
    if (add_download_list) {
        add_download_list = false;
        ldDL();
    }
}

bool download_checkList(void)
{
    if (!FirstDL)
        return false;
    for (struct DownloadList* d = FirstDL; d != NULL; d = d->next) {
        struct stat st;
        if (d->running && !lstat(d->lock, &st))
            return true;
    }
    return false;
}

static const char* convert_size3(size_t size)
{
    Str tmp = Strnew();
    do {
        int n = size % 1000;
        size /= 1000;
        tmp = Sprintf(size ? ",%.3d%s" : "%d%s", n, tmp->ptr);
    } while (size);
    return tmp->ptr;
}

static struct Buffer* DownloadListBuffer(void)
{
    struct DownloadList* d;
    Str src = NULL;
    struct stat st;
    int duration, rate, eta;
    size_t size;

    if (!FirstDL)
        return NULL;

    time_t cur_time = time(0);
    /* FIXME: gettextize? */
    src = Strnew_charp("<html><head><title>" DOWNLOAD_LIST_TITLE
                       "</title></head>\n<body><h1 align=center>" DOWNLOAD_LIST_TITLE "</h1>\n"
                       "<form method=internal action=download><hr>\n");
    for (d = LastDL; d != NULL; d = d->prev) {
        if (lstat(d->lock, &st))
            d->running = FALSE;
        Strcat_charp(src, "<pre>\n");
        Strcat(src, Sprintf("%s\n  --&gt; %s\n  ", html_quote(d->url), html_quote(conv_from_system(d->save))));
        duration = cur_time - d->time;
        if (!stat(d->save, &st)) {
            size = st.st_size;
            if (!d->running) {
                if (!d->err)
                    d->size = size;
                duration = st.st_mtime - d->time;
            }
        } else
            size = 0;
        if (d->size) {
            int i, l = TTY_COLS() - 6;
            if (size < d->size)
                i = 1.0 * l * size / d->size;
            else
                i = l;
            l -= i;
            while (i-- > 0)
                Strcat_char(src, '#');
            while (l-- > 0)
                Strcat_char(src, '_');
            Strcat_char(src, '\n');
        }
        if ((d->running || d->err) && size < d->size)
            Strcat(src, Sprintf("  %s / %s bytes (%d%%)", convert_size3(size), convert_size3(d->size), (int)(100.0 * size / d->size)));
        else
            Strcat(src, Sprintf("  %s bytes loaded", convert_size3(size)));
        if (duration > 0) {
            rate = size / duration;
            Strcat(src, Sprintf("  %02d:%02d:%02d  rate %s/sec", duration / (60 * 60), (duration / 60) % 60, duration % 60, convert_size(rate, 1)));
            if (d->running && size < d->size && rate) {
                eta = (d->size - size) / rate;
                Strcat(src, Sprintf("  eta %02d:%02d:%02d", eta / (60 * 60), (eta / 60) % 60, eta % 60));
            }
        }
        Strcat_char(src, '\n');
        if (!d->running) {
            Strcat(src, Sprintf("<input type=submit name=ok%d value=OK>", d->pid));
            switch (d->err) {
            case 0:
                if (size < d->size)
                    Strcat_charp(src, " Download ended but probably not complete");
                else
                    Strcat_charp(src, " Download complete");
                break;
            case 1:
                Strcat_charp(src, " Error: could not open destination file");
                break;
            case 2:
                Strcat_charp(src, " Error: could not write to file (disk full)");
                break;
            default:
                Strcat_charp(src, " Error: unknown reason");
            }
        } else
            Strcat(src, Sprintf("<input type=submit name=stop%d value=STOP>", d->pid));
        Strcat_charp(src, "\n</pre><hr>\n");
    }
    Strcat_charp(src, "</form></body></html>");
    return loadHTMLString(src);
}

void download_panel()
{
    bool replace = false;
    if (Currentbuf->bufferprop & BP_INTERNAL && !strcmp(Currentbuf->buffername, DOWNLOAD_LIST_TITLE))
        replace = true;
    if (!FirstDL) {
        if (replace) {
            if (Currentbuf == Firstbuf && Currentbuf->nextBuffer == NULL) {
                if (nTab() > 1)
                    deleteTab(CurrentTab());
            } else
                delBuffer(Currentbuf);
        }
        return;
    }
    bool reload = download_checkList();
    struct Buffer* buf = DownloadListBuffer();
    if (!buf) {
        return;
    }
    buf->bufferprop |= (BP_INTERNAL | BP_NO_URL);
    if (replace) {
        COPY_BUFROOT(buf, Currentbuf);
        restorePosition(buf, Currentbuf);
    }
    bool new_tab = false;
    if (!replace && getRuntime()->open_tab_dl_list) {
        _newT();
        new_tab = true;
    }
    pushBuffer(buf);
    if (replace || new_tab)
        deletePrevBuf();
    if (reload)
        Currentbuf->event = setAlarmEvent(Currentbuf->event, 1, AL_IMPLICIT,
            FUNCNAME_reload, NULL);
}

void addDownloadList(pid_t pid,
    const char* url, const char* save, const char* lock, size_t size)
{
    struct DownloadList* d;

    d = New(struct DownloadList);
    d->pid = pid;
    d->url = url;
    if (save[0] != '/' && save[0] != '~')
        save = Strnew_m_charp(CurrentDir, "/", save, NULL)->ptr;
    d->save = expandPath(save);
    d->lock = lock;
    d->size = size;
    d->time = time(0);
    d->running = TRUE;
    d->err = 0;
    d->next = NULL;
    d->prev = LastDL;
    if (LastDL)
        LastDL->next = d;
    else
        FirstDL = d;
    LastDL = d;
    add_download_list = TRUE;
}

bool hasDownloadList()
{
    if (add_download_list) {
        add_download_list = FALSE;

        tabs_prepare();

        if (!Firstbuf || Firstbuf == NO_BUFFER) {
            Firstbuf = Currentbuf = newBuffer(INIT_BUFFER_WIDTH);
            Currentbuf->bufferprop = BP_INTERNAL | BP_NO_URL;
            Currentbuf->buffername = DOWNLOAD_LIST_TITLE;
        } else
            Currentbuf = Firstbuf;
        ldDL();
        return true;
    } else {
        return false;
    }
}

void download_action(struct parsed_tagarg* arg)
{
    struct DownloadList* d;
    pid_t pid;

    for (; arg; arg = arg->next) {
        if (!strncmp(arg->arg, "stop", 4)) {
            pid = (pid_t)atoi(&arg->arg[4]);
            kill(pid, SIGKILL);
        } else if (!strncmp(arg->arg, "ok", 2))
            pid = (pid_t)atoi(&arg->arg[2]);
        else
            continue;
        for (d = FirstDL; d; d = d->next) {
            if (d->pid == pid) {
                unlink(d->lock);
                if (d->prev)
                    d->prev->next = d->next;
                else
                    FirstDL = d->next;
                if (d->next)
                    d->next->prev = d->prev;
                else
                    LastDL = d->prev;
                break;
            }
        }
    }
    ldDL();
}

void stopDownload(void)
{
    struct DownloadList* d;

    if (!FirstDL)
        return;
    for (d = FirstDL; d != NULL; d = d->next) {
        if (!d->running)
            continue;
#ifndef __MINGW32_VERSION
        kill(d->pid, SIGKILL);
#endif
        unlink(d->lock);
    }
}
