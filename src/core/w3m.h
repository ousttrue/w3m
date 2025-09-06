#pragma once
#include <stdbool.h>
#include <sys/types.h>

enum DefaultUrlType {
    DEFAULT_URL_EMPTY = 0,
    DEFAULT_URL_CURRENT = 1,
    DEFAULT_URL_LINK = 2,
};
extern int DefaultURLString;
extern int UseDictCommand;
extern char* DictCommand;
extern int use_mark;

extern int clear_buffer;
extern char* config_file;
extern char FollowLocale;
extern int confirm_on_quit;

extern int CurrentKey;
extern char* CurrentKeyData;
extern char* CurrentCmdData;

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
void w3m_exit(int i);
