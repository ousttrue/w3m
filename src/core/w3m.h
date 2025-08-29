#pragma once
#include <stdbool.h>

void initialize();
void fmInit();
void fmTerm();

int main_loop(const char* line_str);
void _goLine(const char* l);

struct _Buffer;
void delBuffer(struct _Buffer* buf);

bool onFrame();
void onKeyInput(char c);

void pushEvent(int cmd, void* data);
void chkURLBuffer(struct _Buffer* buf);
struct _AlarmEvent* setAlarmEvent(struct _AlarmEvent* event, int sec, short status, int cmd, void* data);
