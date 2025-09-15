#pragma once
#include "geometry.h"
#include <stdbool.h>
#include <sys/types.h>
#include <Str.h>

extern char* mkd_tmp_dir;

extern int UseDictCommand;
extern char* DictCommand;
extern int use_mark;

extern const char* config_file;
extern char FollowLocale;
extern int confirm_on_quit;

extern int CurrentKey;
extern const char* CurrentKeyData;
extern const char* CurrentCmdData;

struct KeyValue;
struct Buffer;
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
void _goLine(struct UI ui, const char* l);

bool onFrame();
void onKeyInput(unsigned char c);

void pushEvent(int cmd, void* data);
void chkURLBuffer(struct Buffer* buf);
struct _AlarmEvent* setAlarmEvent(struct _AlarmEvent* event, int sec, short status, int cmd, void* data);

void w3m_exit(int i);
Str myEditor(const char* cmd, const char* file, int line);
void _quitfm(bool confirm);
