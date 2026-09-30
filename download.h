#ifndef W3M_DOWNLOAD_H
#define W3M_DOWNLOAD_H

#include "parsetag.h"

#include <sys/types.h>

#define DOWNLOAD_LIST_TITLE "Download List Panel"

typedef struct _DownloadList {
    pid_t pid;
    char* url;
    char* save;
    char* lock;
    size_t size;
    time_t time;
    int running;
    int err;
    struct _DownloadList* next;
    struct _DownloadList* prev;
} DownloadList;

extern void addDownloadList(pid_t pid, const char* url,
    const char* save, const char* lock, size_t size);
extern void stopDownload(void);
extern void download_action(struct parsed_tagarg* arg);

extern int do_download;
#endif
