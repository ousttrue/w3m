#pragma once
#include <stdint.h>
#include "Str.h"

#define FRAME_WIDTH 2

typedef struct _AlarmEvent {
    int sec;
    short status;
    int cmd;
    void* data;
} AlarmEvent;

extern AlarmEvent DefaultAlarm;

extern char* MarkString;
extern int check_target;
extern int prec_num;
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

extern int prec_num;

void escKeyProc(int c, int esc, unsigned char* map);
void nscroll(int n, int mode);
int searchKeyNum(void);
typedef struct _Buffer Buffer;
typedef int (*SrchFunc)(Buffer*, char*);
void srch(SrchFunc func, char* prompt);
void isrch(SrchFunc func, char* prompt);
void srch_nxtprv(int reverse);
void shiftvisualpos(Buffer* buf, int shift);
void pushBuffer(Buffer* buf);
void cmd_loadfile(char* fn);
struct _ParsedURL;
struct form_list;
void cmd_loadURL(char* url, struct _ParsedURL* current, char* referer, struct form_list* request);
void _movL(int n);
void _movD(int n);
void _movU(int n);
void _movR(int n);
struct _Line;
int prev_nonnull_line(struct _Line* line);
int is_wordchar(uint32_t c);
uint32_t getChar(char* p);
int next_nonnull_line(struct _Line* line);
void _quitfm(int confirm);
void delBuffer(Buffer* buf);
void _goLine(char* l);
int cur_real_linenumber(Buffer* buf);
void _followForm(int submit);
void gotoLabel(char* label);
int handleMailto(char* url);
void _newT(void);
Buffer* loadLink(char* url, char* target, char* referer, struct form_list* request);
void _nextA(int visited);
void _prevA(int visited);
void nextX(int d, int dy);
void nextY(int d);
int checkBackBuffer(Buffer* buf);
void goURL0(char* prompt, int relative);
void cmd_loadBuffer(Buffer* buf, int prop, int linkid);
struct _anchor;
void anchorMn(struct _anchor* (*menu_func)(Buffer*), int go);
void _peekURL(int only_img);
Str currentURL(void);
struct form_item_list;
void query_from_followform(Str* query, struct form_item_list* fi, int multipart);
void repBuffer(Buffer* oldbuf, Buffer* buf);
void _docCSet(uint32_t charset);
char* getCurWord(Buffer* buf, int* spos, int* epos);
void invoke_browser(char* url);
void process_mouse(int btn, int x, int y);
struct _TabBuffer* posTab(int x, int y);
void execdict(char* word);
char* GetWord(Buffer* buf);
struct _TabBuffer* numTab(int n);
void followTab(struct _TabBuffer* tab);
void tabURL0(struct _TabBuffer* tab, char* prompt, int relative);
void moveTab(struct _TabBuffer* t, struct _TabBuffer* t2, int right);
Buffer* DownloadListBuffer(void);
struct _BufferPos;
void resetPos(struct _BufferPos* b);
