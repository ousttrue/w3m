#pragma once
#include <string>
#include <list>

void addDownloadList(int pid, const char *url, const char *save,
                     const char *lock, long long size);
void stopDownload(void);
int checkDownloadList(void);
bool popAddDownloadList();
std::string DownloadListBuffer(void);
void download_action(const std::list<std::pair<std::string, std::string>> &);
