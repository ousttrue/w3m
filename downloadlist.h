#pragma once
#include <unistd.h>
#include <time.h>

struct DownloadList {
  pid_t pid;
  char *url;
  char *save;
  char *lock;
  long long size;
  time_t time;
  int running;
  int err;
  DownloadList *next;
  DownloadList *prev;
};
#define DOWNLOAD_LIST_TITLE "Download List Panel"

extern DownloadList *FirstDL;
extern DownloadList *LastDL;

void addDownloadList(pid_t pid, char *url, char *save, char *lock,
                     long long size);
void stopDownload(void);
int checkDownloadList(void);

bool popAddDownloadList();
