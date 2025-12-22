#pragma once
/// w3m_rc: w3m run command ?
///
/// config
/// process
/// signal
#include "Str.h"
#include "termcap_util.h"
#include <libwc/wtf.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define nextChar(s, l) \
    do {               \
        (s)++;         \
    } while ((s) < (l)->len && (l)->propBuf[s] & PC_WCHAR2)

#define prevChar(s, l) \
    do {               \
        (s)--;         \
    } while ((s) > 0 && (l)->propBuf[s] & PC_WCHAR2)

#define TRAP_ON                                \
    if (TrapSignal) {                          \
        prevtrap = mySignal(SIGINT, KeyAbort); \
        tty_cbreak(true);                      \
    }
#define TRAP_OFF                        \
    if (TrapSignal) {                   \
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

struct Runtime {
    // Don't change
    wc_ces InnerCharset;
    wc_ces DisplayCharset;
    wc_ces DocumentCharset;
    wc_ces SystemCharset;
    wc_ces BookmarkCharset;

    char ExtHalfdump;
    int Tabstop;
    int showLineNum;
    int FoldLine;

    int lines;
    int cols;

    struct TermcapEntry termcap;

    bool Do_not_use_ti_te;
    int highIntensityColors;

    int UseHistory;
    int URLHistSize;
    int SaveURLHist;
    struct Hist* LoadHist;
    struct Hist* SaveHist;
    struct Hist* URLHist;
    struct Hist* ShellHist;
    struct Hist* TextHist;

    struct TabBuffer* CurrentTab;
    struct TabBuffer* FirstTab;
    struct TabBuffer* LastTab;
    int nTab;

    int CurrentKey;
    int prec_num;
    int prev_key;
    const char* CurrentKeyData;
    const char* CurrentCmdData;
    struct Event* CurrentEvent;
    struct Event* LastEvent;
};
struct Runtime* getRuntime(void);
struct TabBuffer* CurrentTab();
struct TabBuffer* FirstTab();
struct TabBuffer* LastTab();
int nTab();

char* conv_from_system(const char* x);
char* conv_to_system(const char* x);
char* url_quote_conv(const char* x, wc_ces c);
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
void tty_refresh(void);

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
struct Buffer* loadLink(char* url, char* target, char* referer, struct FormList* request, bool on_target, bool do_download);
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
int exec_cmd(const char *cmd);
uint8_t blockChild(const char *cmd);
