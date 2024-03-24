#pragma once
#include <time.h>
#include <string>
#include <list>

struct DownloadList {
  int pid;
  const char *url;
  std::string save;
  const char *lock;
  size_t size;
  time_t time;
  int running;
  int err;
  DownloadList *next;
  DownloadList *prev;
};
#define DOWNLOAD_LIST_TITLE "Download List Panel"

void addDownloadList(int pid, const char *url, const char *save,
                     const char *lock, long long size);
void stopDownload(void);
int checkDownloadList(void);
bool popAddDownloadList();
std::string DownloadListBuffer(void);
void download_action(const std::list<std::pair<std::string, std::string>> &);
