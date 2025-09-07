#pragma once
#include <sys/types.h>

typedef struct _DownloadList {
    pid_t pid;
    const char* url;
    const char* save;
    const char* lock;
    long long size;
    time_t time;
    int running;
    int err;
    struct _DownloadList* next;
    struct _DownloadList* prev;
} DownloadList;
#define DOWNLOAD_LIST_TITLE "Download List Panel"

extern DownloadList* FirstDL;
extern DownloadList* LastDL;

struct KeyValue;

void updateDownload();
void addDownloadList(pid_t pid, char* url, char* save, char* lock, long long size);
void stopDownload(void);
int checkDownloadList(void);
void download_action(struct KeyValue* arg);
