#pragma once
#include "geometry.h"
#include <stdbool.h>
#include <sys/types.h>
#include <Str.h>

extern char* mkd_tmp_dir;
extern char ArgvIsURL;

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

void delBuffer(struct UI ui, struct Buffer* buf);

bool onFrame();
void onKeyInput(unsigned char c);

void pushEvent(int cmd, void* data);
void chkURLBuffer(struct Buffer* buf);
struct _AlarmEvent* setAlarmEvent(struct _AlarmEvent* event, int sec, short status, int cmd, void* data);
void tmpClearBuffer(struct Buffer* buf);

void change_charset(struct UI ui, struct KeyValue* arg);
void saveBufferInfo(struct UI ui);
void w3m_exit(int i);
char* file_to_url(const char* file, const char* currentDir);
Str myEditor(const char* cmd, const char* file, int line);
