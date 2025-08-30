#pragma once
#include <stdbool.h>
#include <sys/types.h>

struct parsed_tagarg;
struct _Buffer;
struct Frame;

void initialize();
void fmInit();
void fmTerm();

int main_loop(const char* line_str);
void _goLine(const char* l);

void delBuffer(struct _Buffer* buf);

bool onFrame();
void onKeyInput(char c);

void pushEvent(int cmd, void* data);
void chkURLBuffer(struct _Buffer* buf);
struct _AlarmEvent* setAlarmEvent(struct _AlarmEvent* event, int sec, short status, int cmd, void* data);
void tmpClearBuffer(struct _Buffer* buf);

void addDownloadList(pid_t pid, char* url, char* save, char* lock, long long size);
void stopDownload(void);
int checkDownloadList(void);
void download_action(struct parsed_tagarg* arg);

void change_charset(struct parsed_tagarg* arg);
void saveBufferInfo(void);
