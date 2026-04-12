#pragma once
#include <w3m.h>
#include <sys/types.h>

#define DOWNLOAD_LIST_TITLE "Download List Panel"

typedef struct _DownloadList {
    pid_t pid;
    char* url;
    char* save;
    char* lock;
    int64_t size;
    time_t time;
    int running;
    int err;
    struct _DownloadList* next;
    struct _DownloadList* prev;
} DownloadList;

bool checkAddDownloadList();
void addDownloadList(pid_t pid, const char* url, const char* save, const char* lock, int64_t size);
void stopDownload(void);
bool checkDownloadList(void);
struct parsed_tagarg;
void download_action(struct CmdArgs args, struct parsed_tagarg* arg);
void downloadListPanel(struct CmdArgs args);
void exitDownloadList();
