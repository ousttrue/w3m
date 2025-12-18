#pragma once
/// process
/// tty
/// signal
#include "Str.h"
#include <wc.h>
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
        exitRawMode();                         \
    }
#define TRAP_OFF                        \
    if (TrapSignal) {                   \
        enterRawMode();                 \
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
    int lines;
    int cols;

    char *T_cd, *T_ce, *T_kr, *T_kl, *T_cr, *T_bt, *T_ta, *T_sc, *T_rc,
        *T_so, *T_se, *T_us, *T_ue, *T_cl, *T_cm, *T_al, *T_sr, *T_md, *T_me,
        *T_ti, *T_te, *T_nd, *T_as, *T_ae, *T_eA, *T_ac, *T_op;
    char gcmap[96];

    bool Do_not_use_ti_te;

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

#define Currentbuf (getRuntime()->CurrentTab->currentBuffer)
#define Firstbuf (getRuntime()->CurrentTab->firstBuffer)

int getOutputHandle();
void reset_error_exit(int);
char graphchar(char c);
void writestr(const char* s);
int write1(int c);
bool fmInitialized(void);
void init_tty();
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
void setlinescols(void);

inline static int TTY_LINES(void) { return getRuntime()->lines; }
inline static int TTY_COLS(void) { return getRuntime()->cols; }
inline static int LASTLINE(void) { return getRuntime()->lines - 1; }
void tty_set_cols(int cols);
int graph_ok(void);

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

int get_pixel_per_cell(int* ppc, int* ppl);

void tabs_prepare();
bool currentBufferSubmit();
void _followForm(bool submit, bool on_target, bool do_download);
struct form_list;
struct Buffer* loadLink(char* url, char* target, char* referer, struct form_list* request, bool on_target, bool do_download);
struct form_item_list;
void query_from_followform(Str* query, struct form_item_list* fi, int multipart);
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
