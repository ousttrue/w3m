#pragma once
/// w3m_rc: w3m run command ?
///
/// config
/// process
/// signal
/// tmpfile
#include "w3m_types.h"
#include "geometry.h"
#include "Str.h"
#include <libwc/wtf.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

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
#define PREC_NUM (getRuntime()->prec_num ? getRuntime()->prec_num : 1)

struct TabBuffer* CurrentTab();
struct TabBuffer* FirstTab();
struct TabBuffer* LastTab();
int nTab();

char* conv_from_system(const char* x);
char* conv_to_system(const char* x);
char* url_quote_conv(const char* x, enum wc_ces c);
Str Str_conv_to_system(Str x);
Str Str_conv_from_system(Str x);

#define _INIT_BUFFER_WIDTH (TTY_COLS() - (getRuntime()->showLineNum ? 6 : 1))
#define INIT_BUFFER_WIDTH ((_INIT_BUFFER_WIDTH > 0) ? _INIT_BUFFER_WIDTH : 0)
#define FOLD_BUFFER_WIDTH (getRuntime()->FoldLine ? (INIT_BUFFER_WIDTH + 1) : -1)

#define Currentbuf (getRuntime()->CurrentTab->currentBuffer)
#define Firstbuf (getRuntime()->CurrentTab->firstBuffer)

int getOutputHandle();
void reset_error_exit(int);
char graphchar(char c);
void writestr(const char* s);
int write1(int c);
bool fmInitialized(void);
bool tty_init_termcap(void);
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

bool get_pixel_per_cell(int* ppc, int* ppl);

void tabs_prepare();
bool currentBufferSubmit();
struct FollowOption {
    bool on_target;
    bool do_download;
};
struct FollowResult {
    struct Anchor* anchor;
    struct Buffer* new_buf;
};
struct FollowResult _followForm(struct Buffer* buf, struct FollowOption option, bool submit);
struct FollowResult _followA(struct Buffer* buf, struct FollowOption option);
struct FollowResult gotoLabel(struct Buffer* buf, const char* label);
void _followI(bool do_download);

struct FormList;
struct Buffer* loadLink(const char* url, struct FormList* request,
    const char* target, const char* referer, struct FollowOption option);
struct FormItemList;
Str query_from_followform(struct Buffer* buf, struct FormItemList* fi, bool multipart);
void pushEvent(int cmd, void* data);
void keyPressEventProc(int c);
void escKeyProc(int c, int esc, unsigned char* map);
bool eventUpdate();
void w3m_end_frame();
void w3m_on_key(uint8_t ch);
const char* GetWord(struct Buffer* buf);
int is_wordchar(wc_uint32 c);
wc_uint32 getChar(const char* p);

void show_params(FILE* fp);
int exec_cmd(const char* cmd);
uint8_t blockChild(const char* cmd);

void showProgress(int64_t* linelen, int64_t* trbyte, size_t current_content_length);
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
int searchKeyNum(void);
void _quitfm(bool confirm);
int handleMailto(const char* url);

enum TmpFileTypes {
    TMPF_DFL = 0,
    TMPF_SRC = 1,
    TMPF_FRAME = 2,
    TMPF_CACHE = 3,
    TMPF_COOKIE = 4,
    TMPF_HIST = 5,
    MAX_TMPF_TYPE = 6,
};

Str tmpfname(enum TmpFileTypes type, const char* ext);
void moveTab(struct TabBuffer* t, struct TabBuffer* t2, int right);
