#pragma once
#include <stdint.h>

#ifdef _WIN32
#include <process.h>
#endif

#define DOWNLOAD_LIST_TITLE "Download List Panel"

void download_update();
void addDownloadList(pid_t pid, char *url, char *save, char *lock,
                     int64_t size);

bool do_add_download_list();
void stopDownload();
bool checkDownloadList();
struct parsed_tagarg;
void download_action(struct parsed_tagarg *arg);
