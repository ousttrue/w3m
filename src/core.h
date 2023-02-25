#pragma once
#include "fm.h"
#include <ucs.h>
#include "proto.h"
#include <setjmp.h>
#include <signal.h>

extern JMP_BUF IntReturn;
extern int prec_num;
#define PREC_NUM (prec_num ? prec_num : 1)
extern int prev_key;
extern int on_target;

extern char *SearchString;
extern int (*searchRoutine)(Buffer *, char *);
extern int searchKeyNum(void);

extern int check_target;
extern int display_ok;

extern MySignalHandler SigPipe(SIGNAL_ARG);
extern void delBuffer(Buffer *buf);
extern void cmd_loadfile(char *path);
extern void cmd_loadBuffer(Buffer *buf, int prop, int linkid);
extern void keyPressEventProc(int c);

extern void _newT(void);
extern char *getCurWord(Buffer *buf, int *spos, int *epos);
extern void cmd_loadURL(char *url, ParsedURL *current, char *referer,
                        FormList *request);
extern void do_dump(Buffer *);
extern void set_buffer_environ(Buffer *);
extern void save_buffer_position(Buffer *buf);
extern void _followForm(int);
extern void _goLine(char *);
extern void followTab(TabBuffer *tab);
extern void moveTab(TabBuffer *t, TabBuffer *t2, int right);
extern void _nextA(int);
extern void _prevA(int);
void query_from_followform(Str *query, FormItemList *fi, int multipart);
Buffer *loadLink(char *url, char *target, char *referer, FormList *request);
FormItemList *save_submit_formlist(FormItemList *src);
char *searchKeyData(void);
void resetPos(BufferPos *b);
void pushBuffer(Buffer *buf);
TabBuffer *numTab(int n);
void escKeyProc(int c, int esc, unsigned char *map);
void nscroll(int n, int mode);
void srch(int (*func)(Buffer *, char *), char *prompt);
void isrch(int (*func)(Buffer *, char *), char *prompt);
void srch_nxtprv(int reverse);
void _movL(int n);
void _movD(int n);
void _movR(int n);
int prev_nonnull_line(Line *line);

#define nextChar(s, l)                                                         \
  do {                                                                         \
    (s)++;                                                                     \
  } while ((s) < (l)->len && (l)->propBuf[s] & PC_WCHAR2)
#define prevChar(s, l)                                                         \
  do {                                                                         \
    (s)--;                                                                     \
  } while ((s) > 0 && (l)->propBuf[s] & PC_WCHAR2)

wc_uint32 getChar(char *p);

int is_wordchar(wc_uint32 c);

void _quitfm(int confirm);
int next_nonnull_line(Line *line);
int cur_real_linenumber(Buffer *buf);
void gotoLabel(char *label);
void nextX(int d, int dy);
void nextY(int d);
void goURL0(char *prompt, int relative);
void anchorMn(Anchor *(*menu_func)(Buffer *), int go);
void _peekURL(int only_img);
void invoke_browser(char *url);
void execdict(char *word);
void tabURL0(TabBuffer *tab, char *prompt, int relative);
int handleMailto(char *url);
int checkBackBuffer(Buffer *buf);
Str currentURL(void);
void repBuffer(Buffer *oldbuf, Buffer *buf);
void _docCSet(wc_ces charset);
char *GetWord(Buffer *buf);
Buffer *DownloadListBuffer(void);

void addDownloadList(pid_t pid, char *url, char *save, char *lock, clen_t size);
