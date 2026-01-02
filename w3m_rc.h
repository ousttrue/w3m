#pragma once
/// w3m_rc: w3m run command ?
///
/// config
/// process
/// signal
#include "w3m_types.h"
#include "geometry.h"
#include <libwc/wtf.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef HAVE_SIGSETJMP
#ifdef __MINGW32_VERSION
#define SETJMP(env) setjmp(env)
#define LONGJMP(env, val) longjmp(env, val)
#define JMP_BUF jmp_buf
#else
#define SETJMP(env) sigsetjmp(env, 1)
#define LONGJMP(env, val) siglongjmp(env, val)
#define JMP_BUF sigjmp_buf
#endif /* __MINGW32_VERSION */
#else
#define SETJMP(env) setjmp(env)
#define LONGJMP(env, val) longjmp(env, val)
#define JMP_BUF jmp_buf
#endif

#define DUMP_BUFFER 0x01
#define DUMP_HEAD 0x02
#define DUMP_SOURCE 0x04
#define DUMP_EXTRA 0x08
#define DUMP_HALFDUMP 0x10
#define DUMP_FRAME 0x20
#define w3m_halfdump (getRuntime()->w3m_dump & DUMP_HALFDUMP)

#define RELATIVE_WIDTH(w) (((w) >= 0) ? (int)((w) / getRuntime()->pixel_per_char) : (w))
#define REAL_WIDTH(w, limit) (((w) >= 0) ? (int)((w) / getRuntime()->pixel_per_char) : -(w) * (limit) / 100)

extern char* w3m_version;

#define DEFAULT_COLS 80

#define SAVE_BUF_SIZE 1536

#define nextChar(s, l) \
    do {               \
        (s)++;         \
    } while ((s) < (l)->len && (l)->propBuf[s] & PC_WCHAR2)

#define prevChar(s, l) \
    do {               \
        (s)--;         \
    } while ((s) > 0 && (l)->propBuf[s] & PC_WCHAR2)

#define TRAP_ON                                \
    if (getRuntime()->TrapSignal) {            \
        prevtrap = mySignal(SIGINT, KeyAbort); \
        tty_cbreak(true);                      \
    }
#define TRAP_OFF                        \
    if (getRuntime()->TrapSignal) {     \
        tty_cbreak(false);              \
        if (prevtrap)                   \
            mySignal(SIGINT, prevtrap); \
    }

#define GRAPHIC_CHAR_ASCII 2
#define GRAPHIC_CHAR_DEC 1
#define GRAPHIC_CHAR_CHARSET 0

extern char UseGraphicChar;

struct Event {
    int cmd;
    void* data;
    struct Event* next;
};

struct Runtime* getRuntime(void);
struct TabBuffer* CurrentTab();
struct TabBuffer* FirstTab();
struct TabBuffer* LastTab();
int nTab();

char* conv_from_system(const char* x);
char* conv_to_system(const char* x);
char* url_quote_conv(const char* x, enum wc_ces c);
Str Str_conv_to_halfdump(Str x);
Str Str_conv_to_system(Str x);
Str Str_conv_from_system(Str x);

#define _INIT_BUFFER_WIDTH (TTY_COLS() - (getRuntime()->showLineNum ? 6 : 1))
#define INIT_BUFFER_WIDTH ((_INIT_BUFFER_WIDTH > 0) ? _INIT_BUFFER_WIDTH : 0)
#define FOLD_BUFFER_WIDTH (getRuntime()->FoldLine ? (INIT_BUFFER_WIDTH + 1) : -1)

#define get_strwidth(c) wtf_strwidth((wc_uchar*)(c))
#define get_Str_strwidth(c) wtf_strwidth((wc_uchar*)((c)->ptr))

#define Currentbuf (getRuntime()->CurrentTab->currentBuffer)
#define Firstbuf (getRuntime()->CurrentTab->firstBuffer)

int getOutputHandle();
void reset_error_exit(int);
char graphchar(char c);
void writestr(const char* s);
int write1(int c);
bool fmInitialized(void);
void tty_init_termcap(void);
// input
int getch(void);
// int sleep_till_anykey(int sec, bool purge);
// output
void flush_tty(void);
// void reset_tty(void);
// void ttymode_set(int mode, int imode);
// void ttymode_reset(int mode, int imode);
void tty_add_ISIG();
void tty_remove_ISIG();
void set_cc(int spec, int val);
void w3m_exit(int i);
char* ttyname_tty(void);
void initscr(void);
void tty_MOVE(int line, int column);
void (*mySignal(int signal_number, void (*action)(int)))(int);

void enterRawMode(void);
void exitRawMode(void);
void tty_cbreak(bool);
void setlinescols(void);

inline static int TTY_LINES(void) { return getRuntime()->lines; }
inline static int TTY_COLS(void) { return getRuntime()->cols; }
inline static size_t LASTLINE(void) { return getRuntime()->lines > 0 ? getRuntime()->lines - 1 : 0; }
void tty_set_cols(int cols);
int graph_ok(void);
void tty_clear(void);
void tty_write_screen(void);

// void crmode(void);
// void nocrmode(void);
// void term_echo(void);
// void term_noecho(void);
// void term_raw(void);
// void term_cooked(void);
// void term_cbreak(void);

void term_title(const char* s);
void bell(void);
void quitfm(void);

bool get_pixel_per_cell(int* ppc, int* ppl);

void tabs_prepare();
bool currentBufferSubmit();
void _followForm(bool submit, bool on_target, bool do_download);
struct FormList;
struct Buffer* loadLink(const char* url, const char* target, const char* referer, struct FormList* request, bool on_target, bool do_download);
struct FormItemList;
void query_from_followform(Str* query, struct FormItemList* fi, int multipart);
void pushEvent(int cmd, void* data);
void keyPressEventProc(int c);
void escKeyProc(int c, int esc, unsigned char* map);
bool eventUpdate();
void w3m_end_frame();
void w3m_on_key(uint8_t ch);
char* getCurWord(struct Buffer* buf, int* spos, int* epos);
char* GetWord(struct Buffer* buf);
int is_wordchar(wc_uint32 c);
wc_uint32 getChar(char* p);

void show_params(FILE* fp);
int exec_cmd(const char* cmd);
uint8_t blockChild(const char* cmd);

void showProgress(int64_t* linelen, int64_t* trbyte, size_t current_content_length);
#define AL_UNSET 0
#define AL_EXPLICIT 1
#define AL_IMPLICIT 2
#define AL_IMPLICIT_ONCE 3

typedef struct _AlarmEvent {
    int sec;
    short status;
    int cmd;
    const void* data;
} AlarmEvent;

AlarmEvent* setAlarmEvent(AlarmEvent* event, int sec, short status,
    int cmd, const void* data);

struct parsed_tagarg;
extern void panel_set_option(struct parsed_tagarg*);
char* rcFile(const char* base);
char* etcFile(const char* base);
char* confFile(const char* base);
char* libFile(const char* base);
char* helpFile(const char* base);
void init_rc(void);
extern void change_charset(struct parsed_tagarg* arg);
extern void tmpClearBuffer(struct Buffer* buf);
extern void chkURLBuffer(struct Buffer* buf);
extern int set_param_option(const char* option);
extern char* get_param_option(const char* name);
extern void init_tmp(void);
extern struct Buffer* load_option_panel(void);
extern void sync_with_option(void);
extern char* searchKeyData(void);
