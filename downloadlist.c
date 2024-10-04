#include "downloadlist.h"
#include "text.h"
#include "file.h"
#include "html_text.h"
#include "app.h"
#include "proto.h"
#include "html_readbuffer.h"
#include "alloc.h"
#include "Str.h"
#include "termsize.h"
#include "parsetag.h"
#include "fm.h"
#include <time.h>
#include <sys/stat.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define SIGKILL 9
#else
#include <sys/wait.h>
#endif

struct DownloadList *FirstDL = nullptr;
struct DownloadList *LastDL = nullptr;

struct DownloadList {
  pid_t pid;
  const char *url;
  const char *save;
  const char *lock;
  int64_t size;
  time_t time;
  int running;
  int err;
  struct DownloadList *next;
  struct DownloadList *prev;
};

static bool add_download_list = false;

void download_update() {
  if (add_download_list) {
    add_download_list = false;
    ldDL();
  }
}

bool do_add_download_list() {
  int ret = add_download_list;
  add_download_list = false;
  return ret;
}

void addDownloadList(pid_t pid, char *url, char *save, char *lock,
                     int64_t size) {
  struct DownloadList *d = New(struct DownloadList);
  d->pid = pid;
  d->url = url;
  if (save[0] != '/' && save[0] != '~')
    save = Strnew_m_charp(app_currentdir(), "/", save, NULL)->ptr;
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

#ifdef _WIN32
#define lstat stat
#endif

bool checkDownloadList() {
  if (!FirstDL)
    return false;

  for (auto d = FirstDL; d != NULL; d = d->next) {
    struct stat st;
    if (d->running && !lstat(d->lock, &st))
      return true;
  }
  return false;
}

static char *convert_size3(int64_t size) {
  Str tmp = Strnew();
  int n;

  do {
    n = size % 1000;
    size /= 1000;
    tmp = Sprintf(size ? ",%.3d%s" : "%d%s", n, tmp->ptr);
  } while (size);
  return tmp->ptr;
}

static struct Buffer *DownloadListBuffer(void) {
  struct DownloadList *d;
  Str src = NULL;
  struct stat st;
  time_t cur_time;
  int duration, rate, eta;
  size_t size;

  if (!FirstDL)
    return NULL;
  cur_time = time(0);
  /* FIXME: gettextize? */
  src = Strnew_charp(
      "<html><head><title>" DOWNLOAD_LIST_TITLE
      "</title></head>\n<body><h1 align=center>" DOWNLOAD_LIST_TITLE "</h1>\n"
      "<form method=internal action=download><hr>\n");
  for (d = LastDL; d != NULL; d = d->prev) {
    if (lstat(d->lock, &st))
      d->running = false;
    Strcat_charp(src, "<pre>\n");
    Strcat(src, Sprintf("%s\n  --&gt; %s\n  ", html_quote(d->url),
                        html_quote(d->save)));
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
      int i, l = COLS - 6;
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
      Strcat(src,
             Sprintf("  %s / %s bytes (%d%%)", convert_size3(size),
                     convert_size3(d->size), (int)(100.0 * size / d->size)));
    else
      Strcat(src, Sprintf("  %s bytes loaded", convert_size3(size)));
    if (duration > 0) {
      rate = size / duration;
      Strcat(src, Sprintf("  %02d:%02d:%02d  rate %s/sec", duration / (60 * 60),
                          (duration / 60) % 60, duration % 60,
                          convert_size(rate, 1)));
      if (d->running && size < d->size && rate) {
        eta = (d->size - size) / rate;
        Strcat(src, Sprintf("  eta %02d:%02d:%02d", eta / (60 * 60),
                            (eta / 60) % 60, eta % 60));
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
      Strcat(src,
             Sprintf("<input type=submit name=stop%d value=STOP>", d->pid));
    Strcat_charp(src, "\n</pre><hr>\n");
  }
  Strcat_charp(src, "</form></body></html>");
  return loadHTMLString(src);
}

#ifdef _WIN32
BOOL kill(DWORD dwProcessId, UINT uExitCode) {
  DWORD dwDesiredAccess = PROCESS_TERMINATE;
  BOOL bInheritHandle = false;
  HANDLE hProcess = OpenProcess(dwDesiredAccess, bInheritHandle, dwProcessId);
  if (hProcess == NULL)
    return false;

  BOOL result = TerminateProcess(hProcess, uExitCode);

  CloseHandle(hProcess);

  return result;
}
#endif

void download_action(struct parsed_tagarg *arg) {

  for (; arg; arg = arg->next) {
    pid_t pid;
    if (!strncmp(arg->arg, "stop", 4)) {
      pid = (pid_t)atoi(&arg->arg[4]);
#ifdef _WIN32
#else
      kill(pid, SIGKILL);
#endif
    } else if (!strncmp(arg->arg, "ok", 2)) {
      pid = (pid_t)atoi(&arg->arg[2]);
    } else {
      continue;
    }

    for (auto d = FirstDL; d; d = d->next) {
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

void stopDownload(void) {
  struct DownloadList *d;

  if (!FirstDL)
    return;
  for (d = FirstDL; d != NULL; d = d->next) {
    if (!d->running)
      continue;
    kill(d->pid, SIGKILL);
    unlink(d->lock);
  }
}

void download_exit(pid_t pid, int err) {

  for (auto d = FirstDL; d != NULL; d = d->next) {
    if (d->pid == pid) {
      d->err = err;
      break;
    }
  }
}
