#pragma once
#include <stdbool.h>
#include <sys/types.h>

bool hasDownloadList();
void download_update();
bool download_checkList(void);
void download_panel(void);
void addDownloadList(pid_t pid, char* url, char* save, char* lock, size_t size);
