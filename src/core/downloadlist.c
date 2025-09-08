#include "downloadlist.h"
#include "expandpath.h"
#include "indep.h"
#include "ui.h"
#include <Str.h>
#include <alloc.h>
#include <time.h>
#include <stdbool.h>
#include <sys/stat.h>

#include "defun.h"

DownloadList* FirstDL = 0;
DownloadList* LastDL = 0;
static bool add_download_list = false;

void updateDownload()
{
    if (add_download_list) {
        add_download_list = false;
        ldDL();
    }
}

void addDownloadList(pid_t pid, char* url, char* save, char* lock, long long size)
{
    DownloadList* d = New(DownloadList);
    d->pid = pid;
    d->url = url;
    if (save[0] != '/' && save[0] != '~')
        save = Strnew_m_charp(CurrentDir, "/", save, NULL)->ptr;
    d->save = expandPath(save);
    d->lock = lock;
    d->size = size;
    d->time = time(0);
    d->running = true;
    d->err = 0;
    d->next = NULL;
    d->prev = LastDL;
    if (LastDL)
        LastDL->next = d;
    else
        FirstDL = d;
    LastDL = d;
    add_download_list = true;
}

int checkDownloadList(void)
{
    DownloadList* d;
    struct stat st;

    if (!FirstDL)
        return false;
    for (d = FirstDL; d != NULL; d = d->next) {
        if (d->running && !lstat(d->lock, &st))
            return true;
    }
    return false;
}
