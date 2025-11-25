#include "screen.h"
#include "terms.h"
#include <gcstr/gcstr.h>
#include <stdlib.h>
#include <termcap.h>
#include <wc.h>
#include <wtf.h>
#include <string.h>

#define SPACE " "

#define MAX_LINE 200
#define MAX_COLUMN 400

static int tab_step = 8;
static int max_LINES = 0;
static int max_COLS = 0;
static struct ScreenLine* ScreenElem = 0;
static struct ScreenLine** ScreenImage = 0;
static int CurLine = 0;
static int CurColumn = 0;
static uint16_t CurrentMode = 0;

struct Screen getScreen()
{
    return (struct Screen) {
        .ScreenImage = ScreenImage,
        .CurLine = CurLine,
        .CurColumn = CurColumn,
    };
}

void set_screen_mode(uint16_t mode)
{
    CurrentMode = mode;
}

void setupscreen(void)
{
    int i;

    if (LINES + 1 > max_LINES) {
        max_LINES = LINES + 1;
        max_COLS = 0;
        ScreenElem = New_N(struct ScreenLine, max_LINES);
        ScreenImage = New_N(struct ScreenLine*, max_LINES);
    }
    if (COLS + 1 > max_COLS) {
        max_COLS = COLS + 1;
        for (i = 0; i < max_LINES; i++) {
            ScreenElem[i].lineimage = New_N(char*, max_COLS);
            memset(ScreenElem[i].lineimage, 0, max_COLS * sizeof(char*));
            ScreenElem[i].lineprop = New_N(uint16_t, max_COLS);
        }
    }
    for (i = 0; i < LINES; i++) {
        ScreenImage[i] = &ScreenElem[i];
        ScreenImage[i]->lineprop[0] = S_EOL;
        ScreenImage[i]->isdirty = 0;
    }
    for (; i < max_LINES; i++) {
        ScreenElem[i].isdirty = L_UNUSED;
    }

    clear();
}

void move(int line, int column)
{
    if (line >= 0 && line < LINES)
        CurLine = line;
    if (column >= 0 && column < COLS)
        CurColumn = column;
}

void wrap(void)
{
    if (CurLine == LINES - 1)
        return;
    CurLine++;
    CurColumn = 0;
}

void touch_cursor(void)
{
    int i;
    touch_line();
    for (i = CurColumn; i >= 0; i--) {
        touch_column(i);
        if (CHMODE(ScreenImage[CurLine]->lineprop[i]) != C_WCHAR2)
            break;
    }
    for (i = CurColumn + 1; i < COLS; i++) {
        if (CHMODE(ScreenImage[CurLine]->lineprop[i]) != C_WCHAR2)
            break;
        touch_column(i);
    }
}

void touch_column(int col)
{
    if (col >= 0 && col < COLS)
        ScreenImage[CurLine]->lineprop[col] |= S_DIRTY;
}

void touch_line(void)
{
    if (!(ScreenImage[CurLine]->isdirty & L_DIRTY)) {
        int i;
        for (i = 0; i < COLS; i++)
            ScreenImage[CurLine]->lineprop[i] &= ~S_DIRTY;
        ScreenImage[CurLine]->isdirty |= L_DIRTY;
    }
}

/* XXX: conflicts with curses's clrtoeol(3) ? */
void clrtoeol(void)
{ /* Clear to the end of line */
    int i;
    uint16_t* lprop = ScreenImage[CurLine]->lineprop;

    if (lprop[CurColumn] & S_EOL)
        return;

    if (!(ScreenImage[CurLine]->isdirty & (L_NEED_CE | L_CLRTOEOL)) || ScreenImage[CurLine]->eol > CurColumn)
        ScreenImage[CurLine]->eol = CurColumn;

    ScreenImage[CurLine]->isdirty |= L_CLRTOEOL;
    touch_line();
    for (i = CurColumn; i < COLS && !(lprop[i] & S_EOL); i++) {
        lprop[i] = S_EOL | S_DIRTY;
    }
}

static void
clrtoeol_with_bcolor(void)
{
    int i, cli, cco;
    uint16_t pr;

    if (!(CurrentMode & S_BCOLORED)) {
        clrtoeol();
        return;
    }
    cli = CurLine;
    cco = CurColumn;
    pr = CurrentMode;
    CurrentMode = (CurrentMode & (M_CEOL | S_BCOLORED)) | C_ASCII;
    for (i = CurColumn; i < COLS; i++)
        addch(' ');
    move(cli, cco);
    CurrentMode = pr;
}

void clrtoeolx(void)
{
    clrtoeol_with_bcolor();
}

static void
clrtobot_eol(void (*clrtoeol)())
{
    int l, c;

    l = CurLine;
    c = CurColumn;
    (*clrtoeol)();
    CurColumn = 0;
    CurLine++;
    for (; CurLine < LINES; CurLine++)
        (*clrtoeol)();
    CurLine = l;
    CurColumn = c;
}

void clrtobot(void)
{
    clrtobot_eol(clrtoeol);
}

void clrtobotx(void)
{
    clrtobot_eol(clrtoeolx);
}

bool is_need_redraw(const char* c1, uint16_t pr1, const char* c2, uint16_t pr2)
{
    if (!c1 || !c2 || strcmp(c1, c2))
        return 1;
    if (*c1 == ' ')
        return (pr1 ^ pr2) & M_SPACE & ~S_DIRTY;

    if ((pr1 ^ pr2) & ~S_DIRTY)
        return 1;

    return 0;
}

void addmch(const char* pc, size_t len)
{
    uint16_t* pr;
    int dest, i;
    static Str tmp = NULL;
    char** p;
    char c = *pc;
    int width = wtf_width((wc_uchar*)pc);

    if (tmp == NULL)
        tmp = Strnew();
    Strcopy_charp_n(tmp, pc, len);
    pc = tmp->ptr;

    if (CurColumn == COLS)
        wrap();
    if (CurColumn >= COLS)
        return;
    p = ScreenImage[CurLine]->lineimage;
    pr = ScreenImage[CurLine]->lineprop;

    if (pr[CurColumn] & S_EOL) {
        if (c == ' ' && !(CurrentMode & M_SPACE)) {
            CurColumn++;
            return;
        }
        for (i = CurColumn; i >= 0 && (pr[i] & S_EOL); i--) {
            SETCH(p[i], SPACE, 1);
            SETPROP(pr[i], (pr[i] & M_CEOL) | C_ASCII);
        }
    }

    if (c == '\t' || c == '\n' || c == '\r' || c == '\b')
        SETCHMODE(CurrentMode, C_CTRL);
    else if (len > 1)
        SETCHMODE(CurrentMode, C_WCHAR1);
    else if (!IS_CNTRL(c))
        SETCHMODE(CurrentMode, C_ASCII);
    else
        return;

    /* Required to erase bold or underlined character for some * terminal
     * emulators. */
    i = CurColumn + width - 1;
    if (i < COLS && (((pr[i] & S_BOLD) && is_need_redraw(p[i], pr[i], pc, CurrentMode)) || ((pr[i] & S_UNDERLINE) && !(CurrentMode & S_UNDERLINE)))) {
        touch_line();
        i++;
        if (i < COLS) {
            touch_column(i);
            if (pr[i] & S_EOL) {
                SETCH(p[i], SPACE, 1);
                SETPROP(pr[i], (pr[i] & M_CEOL) | C_ASCII);
            } else {
                for (i++; i < COLS && CHMODE(pr[i]) == C_WCHAR2; i++)
                    touch_column(i);
            }
        }
    }

    if (CurColumn + width > COLS) {
        touch_line();
        for (i = CurColumn; i < COLS; i++) {
            SETCH(p[i], SPACE, 1);
            SETPROP(pr[i], (pr[i] & ~C_WHICHCHAR) | C_ASCII);
            touch_column(i);
        }
        wrap();
        if (CurColumn + width > COLS)
            return;
        p = ScreenImage[CurLine]->lineimage;
        pr = ScreenImage[CurLine]->lineprop;
    }
    if (CHMODE(pr[CurColumn]) == C_WCHAR2) {
        touch_line();
        for (i = CurColumn - 1; i >= 0; i--) {
            uint16_t l = CHMODE(pr[i]);
            SETCH(p[i], SPACE, 1);
            SETPROP(pr[i], (pr[i] & ~C_WHICHCHAR) | C_ASCII);
            touch_column(i);
            if (l != C_WCHAR2)
                break;
        }
    }
    if (CHMODE(CurrentMode) != C_CTRL) {
        if (is_need_redraw(p[CurColumn], pr[CurColumn], pc, CurrentMode)) {
            SETCH(p[CurColumn], pc, len);
            SETPROP(pr[CurColumn], CurrentMode);
            touch_line();
            touch_column(CurColumn);
            SETCHMODE(CurrentMode, C_WCHAR2);
            for (i = CurColumn + 1; i < CurColumn + width; i++) {
                SETCH(p[i], SPACE, 1);
                SETPROP(pr[i], (pr[CurColumn] & ~C_WHICHCHAR) | C_WCHAR2);
                touch_column(i);
            }
            for (; i < COLS && CHMODE(pr[i]) == C_WCHAR2; i++) {
                SETCH(p[i], SPACE, 1);
                SETPROP(pr[i], (pr[i] & ~C_WHICHCHAR) | C_ASCII);
                touch_column(i);
            }
        }
        CurColumn += width;
    } else if (c == '\t') {
        dest = (CurColumn + tab_step) / tab_step * tab_step;
        if (dest >= COLS) {
            wrap();
            touch_line();
            dest = tab_step;
            p = ScreenImage[CurLine]->lineimage;
            pr = ScreenImage[CurLine]->lineprop;
        }
        for (i = CurColumn; i < dest; i++) {
            if (is_need_redraw(p[i], pr[i], SPACE, CurrentMode)) {
                SETCH(p[i], SPACE, 1);
                SETPROP(pr[i], CurrentMode);
                touch_line();
                touch_column(i);
            }
        }
        CurColumn = i;
    } else if (c == '\n') {
        wrap();
    } else if (c == '\r') { /* Carriage return */
        CurColumn = 0;
    } else if (c == '\b' && CurColumn > 0) { /* Backspace */
        CurColumn--;
        while (CurColumn > 0 && CHMODE(pr[CurColumn]) == C_WCHAR2)
            CurColumn--;
    }
}

void setlinescols(void)
{
    char* p;
    int i;
#if defined(HAVE_TERMIOS_H) && defined(TIOCGWINSZ)
    struct winsize wins;

    i = ioctl(tty, TIOCGWINSZ, &wins);
    if (i >= 0 && wins.ws_row != 0 && wins.ws_col != 0) {
        LINES = wins.ws_row;
        COLS = wins.ws_col;
    }
#endif /* defined(HAVE-TERMIOS_H) && defined(TIOCGWINSZ) */
    if (LINES <= 0 && (p = getenv("LINES")) != NULL && (i = atoi(p)) >= 0)
        LINES = i;
    if (COLS <= 0 && (p = getenv("COLUMNS")) != NULL && (i = atoi(p)) >= 0)
        COLS = i;
    if (LINES <= 0)
        LINES = tgetnum("li"); /* number of line */
    if (COLS <= 0)
        COLS = tgetnum("co"); /* number of column */
    if (COLS > MAX_COLUMN)
        COLS = MAX_COLUMN;
    if (LINES > MAX_LINE)
        LINES = MAX_LINE;
#if defined(__CYGWIN__)
    LASTLINE = LINES - (isWinConsole == TERM_CYGWIN_RESERVE_IME ? 2 : 1);
#endif /* defined(__CYGWIN__) */
}

void standout(void)
{
    CurrentMode |= S_STANDOUT;
}

void standend(void)
{
    CurrentMode &= ~S_STANDOUT;
}

void toggle_stand(void)
{
    int i;
    uint16_t* pr = ScreenImage[CurLine]->lineprop;
    pr[CurColumn] ^= S_STANDOUT;
    if (CHMODE(pr[CurColumn]) != C_WCHAR2) {
        for (i = CurColumn + 1; CHMODE(pr[i]) == C_WCHAR2; i++)
            pr[i] ^= S_STANDOUT;
    }
}

void bold(void)
{
    CurrentMode |= S_BOLD;
}

void boldend(void)
{
    CurrentMode &= ~S_BOLD;
}

void underline(void)
{
    CurrentMode |= S_UNDERLINE;
}

void underlineend(void)
{
    CurrentMode &= ~S_UNDERLINE;
}

void graphstart(void)
{
    CurrentMode |= S_GRAPHICS;
}

void graphend(void)
{
    CurrentMode &= ~S_GRAPHICS;
}

void setfcolor(int color)
{
    CurrentMode &= ~COL_FCOLOR;
    if ((color & 0xf) <= 7)
        CurrentMode |= (((color & 7) | 8) << 8);
}

void setbcolor(int color)
{
    CurrentMode &= ~COL_BCOLOR;
    if ((color & 0xf) <= 7)
        CurrentMode |= (((color & 7) | 8) << 12);
}
