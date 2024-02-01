#include "downloadlist.h"
#include "etc.h"
#include "w3m.h"
#include "alloc.h"
#include "Str.h"
#include <sys/stat.h>

static bool add_download_list = false;
DownloadList *FirstDL = nullptr;
DownloadList *LastDL = nullptr;

bool popAddDownloadList() {
  auto ret = add_download_list;
  add_download_list = false;
  return ret;
}

void addDownloadList(pid_t pid, const char *url, const char *save,
                     const char *lock, long long size) {

  auto d = (DownloadList *)New(DownloadList);
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

int checkDownloadList() {
  DownloadList *d;
  struct stat st;

  if (!FirstDL)
    return false;
  for (d = FirstDL; d != NULL; d = d->next) {
    if (d->running && !lstat(d->lock, &st))
      return true;
  }
  return false;
}
