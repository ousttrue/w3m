#include "downloadlist.h"
#include "fm.h"
#include "indep.h"
#include <sys/stat.h>

static bool add_download_list = FALSE;

bool popAddDownloadList() {
  auto ret = add_download_list;
  add_download_list = false;
  return ret;
}

void addDownloadList(pid_t pid, char *url, char *save, char *lock,
                     long long size) {

  auto d = (DownloadList *)New(DownloadList);
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

int checkDownloadList(void) {
  DownloadList *d;
  struct stat st;

  if (!FirstDL)
    return FALSE;
  for (d = FirstDL; d != NULL; d = d->next) {
    if (d->running && !lstat(d->lock, &st))
      return TRUE;
  }
  return FALSE;
}
