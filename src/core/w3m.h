#pragma once
#include <stdbool.h>
#include <sys/types.h>

struct KeyValue;
struct _Buffer;
struct Frame;

enum AlarmStatus {
    AL_UNSET = 0,
    AL_EXPLICIT = 1,
    AL_IMPLICIT = 2,
    AL_IMPLICIT_ONCE = 3,
};

typedef struct _AlarmEvent {
    int sec;
    enum AlarmStatus status;
    int cmd;
    void* data;
} AlarmEvent;

void initialize();
void fmInit();
void fmTerm();

int main_loop(const char* line_str);
void _goLine(const char* l);

void delBuffer(struct _Buffer* buf);

bool onFrame();
void onKeyInput(unsigned char c);

void pushEvent(int cmd, void* data);
void chkURLBuffer(struct _Buffer* buf);
struct _AlarmEvent* setAlarmEvent(struct _AlarmEvent* event, int sec, short status, int cmd, void* data);
void tmpClearBuffer(struct _Buffer* buf);

void change_charset(struct KeyValue* arg);
void saveBufferInfo(void);
