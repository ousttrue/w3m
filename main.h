#pragma once
#include <w3m.h>
#include "Str.h"
#include <stdint.h>

#define FRAME_WIDTH 2

typedef struct _AlarmEvent {
    int sec;
    short status;
    const char* cmd;
    const void* data;
} AlarmEvent;

extern AlarmEvent DefaultAlarm;

extern int check_target;
#define PREC_NUM (prec_num ? prec_num : 1)
extern int prev_key;
extern int display_ok;

#define nextChar(s, l) \
    do {               \
        (s)++;         \
    } while ((s) < (l)->len && (l)->propBuf[s] & PC_WCHAR2)

#define prevChar(s, l) \
    do {               \
        (s)--;         \
    } while ((s) > 0 && (l)->propBuf[s] & PC_WCHAR2)

void escKeyProc(struct CmdArgs* args, int esc, const char* map[128]);
void nscroll(struct CmdArgs* args, int n, int mode);
int searchKeyNum(void);
struct Buffer;
void shiftvisualpos(struct Buffer* buf, int shift);
void pushBuffer(struct CmdArgs* args, struct Buffer* buf);
void cmd_loadfile(struct CmdArgs* args, const char* fn);
struct Url;
struct Form;
void cmd_loadURL(struct CmdArgs* args, const char* url, struct Url* current, char* referer, struct Form* request);
void _movL(struct CmdArgs* args, int n);
void _movD(struct CmdArgs* args, int n);
void _movU(struct CmdArgs* args, int n);
void _movR(struct CmdArgs* args, int n);
struct Line;
int prev_nonnull_line(struct Line* line);
int is_wordchar(uint32_t c);
uint32_t getChar(char* p);
int next_nonnull_line(struct Line* line);
void _quitfm(struct CmdArgs* args, int confirm);
void delBuffer(struct Buffer* buf);
void _goLine(struct CmdArgs* args, const char* l);
int cur_real_linenumber(struct Buffer* buf);
void _followForm(struct CmdArgs* args, int submit);
void gotoLabel(struct CmdArgs* args, const char* label);
int handleMailto(struct CmdArgs* args, const char* url);
void _newT(void);
struct Buffer* loadLink(struct CmdArgs* args, const char* url, const char* target, const char* referer, struct Form* request);
void _nextA(struct CmdArgs* args, bool visited);
void _prevA(struct CmdArgs* args, bool visited);
void nextX(struct CmdArgs* args, int d, int dy);
void nextY(struct CmdArgs* args, int d);
int checkBackBuffer(struct Buffer* buf);
void goURL0(struct CmdArgs* args, char* prompt, int relative);
void cmd_loadBuffer(struct CmdArgs* args, struct Buffer* buf, int prop, int linkid);
struct Anchor;
typedef struct Anchor* (*AnchorFunc)(struct CmdArgs* args, struct Buffer*);
void anchorMn(struct CmdArgs* args, AnchorFunc menu_func, int go);
void _peekURL(struct CmdArgs* args, int only_img);
Str currentURL(void);
struct FormItem;
void query_from_followform(Str* query, struct FormItem* fi, int multipart);
void repBuffer(struct Buffer* oldbuf, struct Buffer* buf);
void _docCSet(struct CmdArgs* args, uint32_t charset);
char* getCurWord(struct Buffer* buf, int* spos, int* epos);
void invoke_browser(struct CmdArgs* args, const char* url);
void process_mouse(int btn, int x, int y);
struct _TabBuffer* posTab(int x, int y);
void execdict(struct CmdArgs* args, const char* word);
char* GetWord(struct Buffer* buf);
struct _TabBuffer* numTab(int n);
void followTab(struct CmdArgs* args, struct _TabBuffer* tab);
void tabURL0(struct CmdArgs* args, struct _TabBuffer* tab, char* prompt, int relative);
void moveTab(struct CmdArgs* args, struct _TabBuffer* t, struct _TabBuffer* t2, int right);
struct Buffer* DownloadListBuffer(void);
struct _BufferPos;
void resetPos(struct CmdArgs* args, struct _BufferPos* b);
void w3m_exit(int i);
const char* searchKeyData(void);

void dump_extra(struct Buffer* buf);
void dump_head(struct Buffer* buf);
void dump_source(struct Buffer* buf);
void escdmap(struct CmdArgs* args);
void pushEvent(const char* cmd, void* data);

void tty_init(void);
void tty_deinit(void);
int initscr(void);
void tty_reset(void);
