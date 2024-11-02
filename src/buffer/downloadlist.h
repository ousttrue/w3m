#pragma once
#include "current.h"
#include <stdint.h>

// pid_t
#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

#define DOWNLOAD_LIST_TITLE "Download List Panel"

void download_update(struct Current);
void addDownloadList(pid_t pid, char *url, char *save, char *lock,
                     int64_t size);

bool do_add_download_list();
void stopDownload();
bool checkDownloadList();
struct InternalAction;
void download_action(struct InternalAction *arg, struct Current);
void download_exit(pid_t pid, int err);
