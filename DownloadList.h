#pragma once
#include <sys/types.h>

struct DownloadList {
    pid_t pid;
    char* url;
    char* save;
    char* lock;
    long long size;
    time_t time;
    int running;
    int err;
    struct DownloadList* next;
    struct DownloadList* prev;
};
#define DOWNLOAD_LIST_TITLE "Download List Panel"

extern struct DownloadList* FirstDL;
extern struct DownloadList* LastDL;
extern int add_download_list;

void addDownloadList(pid_t pid, char* url, char* save, char* lock, long long size);
void stopDownload();
int checkDownloadList();
struct KeyValueList;
void download_action(struct KeyValueList* arg);
struct Buffer;
struct Buffer* DownloadListBuffer();
