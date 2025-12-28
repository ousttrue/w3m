#pragma once
#include <stdbool.h>
#include <sys/types.h>

struct DownloadList {
    pid_t pid;
    const char* url;
    const char* save;
    const char* lock;
    size_t size;
    time_t time;
    int running;
    int err;
    struct DownloadList* next;
    struct DownloadList* prev;
};
#define DOWNLOAD_LIST_TITLE "Download List Panel"

bool hasDownloadList();
void download_update();
bool download_checkList(void);
void download_panel(void);
void addDownloadList(pid_t pid,
    const char* url, const char* save, const char* lock, size_t size);
void stopDownload(void);
struct parsed_tagarg;
void download_action(struct parsed_tagarg* arg);
