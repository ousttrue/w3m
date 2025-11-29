#pragma once
#include <gcstr.h>
#include <sys/types.h>
#include <stdbool.h>

struct DownloadList {
    pid_t pid;
    const char* url;
    const char* save;
    const char* lock;
    long long size;
    time_t time;
    int running;
    int err;
    struct DownloadList* next;
    struct DownloadList* prev;
};
#define DOWNLOAD_LIST_TITLE "Download List Panel"

extern struct DownloadList* FirstDL;
extern bool add_download_list;

void dl_update();
void dl_add(pid_t pid, const char* url, const char* save, const char* lock, size_t size);
void dl_stop();
bool dl_has_active();
struct KeyValueList;
void dl_action(struct KeyValueList* arg);
Str DownloadListBuffer(int COLS);
void dl_exit_status(pid_t pid, int p_stat);
