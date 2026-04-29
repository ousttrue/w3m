#include "terms.h"
#include "filepath.h"
#include "global.h"
#include "constants.h"
#include "content_type.h"
#include "alloc.h"
#include "term_tty.h"
#include "terminfo_entry.h"
#include "etc.h"
#include "signal_util.h"
#include "buffer.h"
#include "main.h"
#include "myctype.h"

#include "wc_util.h"
#include <libwc/putc.h>

static enum CellProperty CHMODE(enum CellProperty c) { return ((c)&C_WHICHCHAR); }
#define SETCHMODE(var, mode) ((var) = (((var) & ~C_WHICHCHAR) | mode))

// #define SETCH(var, ch, len) ((var) = New_Reuse(char, (var), (len) + 1), \
//     strncpy((var), (ch), (len + 1)))
// #define SETPROP(var, prop) (var = (((var) & S_DIRTY) | prop))
static void setCell(struct Cell* cell, CellCharBytes ch, size_t len, enum CellProperty prop)
{
    cell->bytes = New_Reuse(uint8_t, cell->bytes, len + 1);
    strncpy((char*)cell->bytes, (const char*)ch, len + 1);
    cell->prop = (cell->prop & S_DIRTY) | prop;
}

static bool
need_redraw(const struct Cell* cell, const CellCharBytes c2, enum CellProperty pr2)
{
    if (!cell->bytes || !c2 || strcmp((const char*)cell->bytes, (const char*)c2))
        return 1;
    if (cell->bytes[0] == ' ')
        return (cell->prop ^ pr2) & M_SPACE & ~S_DIRTY;

    if ((cell->prop ^ pr2) & ~S_DIRTY)
        return 1;

    return 0;
}

struct ScreenLine {
    struct Cell* cells;
    enum LineFlags isdirty;
    short eol;
};

static int max_LINES = 0, max_COLS = 0;
static int tab_step = 8;
static int CurLine, CurColumn;
static struct ScreenLine *ScreenElem = NULL, **ScreenImage = NULL;
static enum CellProperty CurrentMode = 0;
static int graph_enabled = 0;

static struct TermInfo terminfo;

void reset_tty(void)
{
    terminfo_reset(&write1, &terminfo, Do_not_use_ti_te);
    clear_tty();
}

static uint8_t*
skip_gif_header(uint8_t* p)
{
    /* Header */
    p += 10;

    if (*(p) & 0x80) {
        p += (3 * (2 << ((*p) & 0x7)));
    }
    p += 3;

    return p;
}

void set_int(void)
{
    signal(SIGHUP, reset_exit);
    signal(SIGINT, reset_exit);
    signal(SIGQUIT, reset_exit);
    signal(SIGTERM, reset_exit);
    signal(SIGILL, error_dump);
    signal(SIGIOT, error_dump);
    signal(SIGFPE, error_dump);
#ifdef SIGBUS
    signal(SIGBUS, error_dump);
#endif /* SIGBUS */
    /* signal(SIGSEGV, error_dump); */
}

#define graphchar(c) (((unsigned)(c) >= ' ' && (unsigned)(c) < 128) ? terminfo.gcmap[(c) - ' '] : (c))

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
            ScreenElem[i].cells = New_N(struct Cell, max_COLS);
            memset(ScreenElem[i].cells, 0, max_COLS * sizeof(struct Cell));
        }
    }
    for (i = 0; i < LINES; i++) {
        ScreenImage[i] = &ScreenElem[i];
        ScreenImage[i]->cells[0].prop = S_EOL;
        ScreenImage[i]->isdirty = 0;
    }
    for (; i < max_LINES; i++) {
        ScreenElem[i].isdirty = L_UNUSED;
    }

    clear();
}

/*
 * struct ScreenLine initialize
 */
int initscr(void)
{
    set_int();
    getTCstr(&terminfo);
    if (terminfo.T_ti && !Do_not_use_ti_te)
        writestr(&write1, terminfo.T_ti);
    setupscreen();
    return 0;
}

void sc_move(int line, int column)
{
    if (line >= 0 && line < LINES)
        CurLine = line;
    if (column >= 0 && column < COLS)
        CurColumn = column;
}

void addch(uint8_t c)
{
    addmch(&c, 1);
}

static void touch_column(int col)
{
    if (col >= 0 && col < COLS)
        ScreenImage[CurLine]->cells[col].prop |= S_DIRTY;
}

void addmch(const uint8_t* src, size_t len)
{
    int dest, i;
    static Str tmp = NULL;
    int width = wtf_width(WcOption, src[0]);

    if (tmp == NULL)
        tmp = Strnew();
    Strcopy_charp_n(tmp, (const char*)src, len);
    char* pc = tmp->ptr;

    if (CurColumn == COLS)
        wrap();
    if (CurColumn >= COLS)
        return;

    struct Cell* line = ScreenImage[CurLine]->cells;
    if (line[CurColumn].prop & S_EOL) {
        if (src[0] == ' ' && !(CurrentMode & M_SPACE)) {
            CurColumn++;
            return;
        }
        for (i = CurColumn; i >= 0 && (line[i].prop & S_EOL); i--) {
            setCell(&line[i], (CellCharBytes)SPACE, 1, (line[i].prop & M_CEOL) | C_ASCII);
        }
    }

    if (src[0] == '\t' || src[0] == '\n' || src[0] == '\r' || src[0] == '\b')
        SETCHMODE(CurrentMode, C_CTRL);
    else if (len > 1)
        SETCHMODE(CurrentMode, C_WCHAR1);
    else if (!IS_CNTRL(src[0]))
        SETCHMODE(CurrentMode, C_ASCII);
    else
        return;

    /* Required to erase bold or underlined character for some * terminal
     * emulators. */
    i = CurColumn + width - 1;
    if (i < COLS && (((line[i].prop & S_BOLD) && need_redraw(&line[i], (CellCharBytes)pc, CurrentMode)) || ((line[i].prop & S_UNDERLINE) && !(CurrentMode & S_UNDERLINE)))) {
        touch_line();
        i++;
        if (i < COLS) {
            touch_column(i);
            if (line[i].prop & S_EOL) {
                setCell(&line[i], (CellCharBytes)SPACE, 1, (line[i].prop & M_CEOL) | C_ASCII);
            } else {
                for (i++; i < COLS && CHMODE(line[i].prop) == C_WCHAR2; i++)
                    touch_column(i);
            }
        }
    }

    if (CurColumn + width > COLS) {
        touch_line();
        for (i = CurColumn; i < COLS; i++) {
            setCell(&line[i], (CellCharBytes)SPACE, 1, (line[i].prop & ~C_WHICHCHAR) | C_ASCII);
            touch_column(i);
        }
        wrap();
        if (CurColumn + width > COLS)
            return;
        line = ScreenImage[CurLine]->cells;
    }
    if (CHMODE(line[CurColumn].prop) == C_WCHAR2) {
        touch_line();
        for (i = CurColumn - 1; i >= 0; i--) {
            enum CellProperty l = CHMODE(line[i].prop);
            setCell(&line[i], (CellCharBytes)SPACE, 1, (line[i].prop & ~C_WHICHCHAR) | C_ASCII);
            touch_column(i);
            if (l != C_WCHAR2)
                break;
        }
    }
    if (CHMODE(CurrentMode) != C_CTRL) {
        if (need_redraw(&line[CurColumn], (CellCharBytes)pc, CurrentMode)) {
            setCell(&line[CurColumn], (CellCharBytes)pc, len, CurrentMode);
            touch_line();
            touch_column(CurColumn);
            SETCHMODE(CurrentMode, C_WCHAR2);
            for (i = CurColumn + 1; i < CurColumn + width; i++) {
                setCell(&line[i], (CellCharBytes)SPACE, 1, (line[CurColumn].prop & ~C_WHICHCHAR) | C_WCHAR2);
                touch_column(i);
            }
            for (; i < COLS && CHMODE(line[i].prop) == C_WCHAR2; i++) {
                setCell(&line[i], (CellCharBytes)SPACE, 1, (line[i].prop & ~C_WHICHCHAR) | C_ASCII);
                touch_column(i);
            }
        }
        CurColumn += width;
    } else if (src[0] == '\t') {
        dest = (CurColumn + tab_step) / tab_step * tab_step;
        if (dest >= COLS) {
            wrap();
            touch_line();
            dest = tab_step;
            line = ScreenImage[CurLine]->cells;
        }
        for (i = CurColumn; i < dest; i++) {
            if (need_redraw(&line[i], (CellCharBytes)SPACE, CurrentMode)) {
                setCell(&line[i], (CellCharBytes)SPACE, 1, CurrentMode);
                touch_line();
                touch_column(i);
            }
        }
        CurColumn = i;
    } else if (src[0] == '\n') {
        wrap();
    } else if (src[0] == '\r') { /* Carriage return */
        CurColumn = 0;
    } else if (src[0] == '\b' && CurColumn > 0) { /* Backspace */
        CurColumn--;
        while (CurColumn > 0 && CHMODE(line[CurColumn].prop) == C_WCHAR2)
            CurColumn--;
    }
}

void wrap(void)
{
    if (CurLine == (LINES - 1))
        return;
    CurLine++;
    CurColumn = 0;
}

void touch_line(void)
{
    if (!(ScreenImage[CurLine]->isdirty & L_DIRTY)) {
        int i;
        for (i = 0; i < COLS; i++)
            ScreenImage[CurLine]->cells[i].prop &= ~S_DIRTY;
        ScreenImage[CurLine]->isdirty |= L_DIRTY;
    }
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
    struct Cell* line = ScreenImage[CurLine]->cells;
    line[CurColumn].prop ^= S_STANDOUT;
    if (CHMODE(line[CurColumn].prop) != C_WCHAR2) {
        for (int i = CurColumn + 1; CHMODE(line[i].prop) == C_WCHAR2; i++)
            line[i].prop ^= S_STANDOUT;
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

int graph_ok(void)
{
    if (UseGraphicChar != GRAPHIC_CHAR_DEC)
        return 0;
    return terminfo.T_as[0] != 0 && terminfo.T_ae[0] != 0 && terminfo.T_ac[0] != 0;
}

void setfcolor(int color)
{
    CurrentMode &= ~COL_FCOLOR;
    if ((color & 0xf) <= 7)
        CurrentMode |= (((color & 7) | 8) << 8);
}

static char*
color_seq(int colmode)
{
    static char seqbuf[32];
    sprintf(seqbuf, "\033[%dm", ((colmode >> 8) & 7) + (highIntensityColors ? 90 : 30));
    return seqbuf;
}

void setbcolor(int color)
{
    CurrentMode &= ~COL_BCOLOR;
    if ((color & 0xf) <= 7)
        CurrentMode |= (((color & 7) | 8) << 12);
}

static char*
bcolor_seq(int colmode)
{
    static char seqbuf[32];
    sprintf(seqbuf, "\033[%dm", ((colmode >> 12) & 7) + 40);
    return seqbuf;
}

enum MoveStatus {
    RF_NEED_TO_MOVE = 0,
    RF_CR_OK = 1,
    RF_NONEED_TO_MOVE = 2,
};
void refresh(void)
{
    int line, col, pcol;
    int pline = CurLine;
    enum MoveStatus moved = RF_NEED_TO_MOVE;
    enum CellProperty mode = 0;
    enum CellProperty color = COL_FTERM;
    enum CellProperty bcolor = COL_BTERM;
    enum LineFlags* dirty;

    wc_putc_init(WcOption, InnerCharset, DisplayCharset);
    for (line = 0; line <= (LINES - 1); line++) {
        dirty = &ScreenImage[line]->isdirty;
        if (*dirty & L_DIRTY) {
            *dirty &= ~L_DIRTY;
            struct Cell* cells = ScreenImage[line]->cells;
            for (col = 0; col < COLS && !(cells[col].prop & S_EOL); col++) {
                if (*dirty & L_NEED_CE && col >= ScreenImage[line]->eol) {
                    if (need_redraw(&cells[col], (CellCharBytes)SPACE, 0))
                        break;
                } else {
                    if (cells[col].prop & S_DIRTY)
                        break;
                }
            }
            if (*dirty & (L_NEED_CE | L_CLRTOEOL)) {
                pcol = ScreenImage[line]->eol;
                if (pcol >= COLS) {
                    *dirty &= ~(L_NEED_CE | L_CLRTOEOL);
                    pcol = col;
                }
            } else {
                pcol = col;
            }
            if (line < LINES - 2 && pline == line - 1 && pcol == 0) {
                switch (moved) {
                case RF_NEED_TO_MOVE:
                    MOVE(&write1, &terminfo, line, 0);
                    moved = RF_CR_OK;
                    break;
                case RF_CR_OK:
                    write1('\n');
                    write1('\r');
                    break;
                case RF_NONEED_TO_MOVE:
                    moved = RF_CR_OK;
                    break;
                }
            } else {
                MOVE(&write1, &terminfo, line, pcol);
                moved = RF_CR_OK;
            }
            if (*dirty & (L_NEED_CE | L_CLRTOEOL)) {
                writestr(&write1, terminfo.T_ce);
                if (col != pcol)
                    MOVE(&write1, &terminfo, line, col);
            }
            pline = line;
            pcol = col;
            for (; col < COLS; col++) {
                if (cells[col].prop & S_EOL)
                    break;

                /*
                 * some terminal emulators do linefeed when a
                 * character is put on COLS-th column. this behavior
                 * is different from one of vt100, but such terminal
                 * emulators are used as vt100-compatible
                 * emulators. This behaviour causes scroll when a
                 * character is drawn on (COLS-1,LINES-1) point.  To
                 * avoid the scroll, I prohibit to draw character on
                 * (COLS-1,LINES-1).
                 */
                if ((!(cells[col].prop & S_STANDOUT) && (mode & S_STANDOUT))
                    || (!(cells[col].prop & S_UNDERLINE) && (mode & S_UNDERLINE))
                    || (!(cells[col].prop & S_BOLD) && (mode & S_BOLD))
                    || (!(cells[col].prop & S_COLORED) && (mode & S_COLORED))
                    || (!(cells[col].prop & S_BCOLORED) && (mode & S_BCOLORED))
                    || (!(cells[col].prop & S_GRAPHICS) && (mode & S_GRAPHICS))) {
                    if ((mode & S_COLORED) || (mode & S_BCOLORED))
                        writestr(&write1, terminfo.T_op);
                    if (mode & S_GRAPHICS)
                        writestr(&write1, terminfo.T_ae);
                    writestr(&write1, terminfo.T_me);
                    mode &= ~M_MEND;
                }
                if ((*dirty & L_NEED_CE && col >= ScreenImage[line]->eol) ? need_redraw(&cells[col], (CellCharBytes)SPACE, 0)
                                                                          : (cells[col].prop & S_DIRTY)) {
                    if (pcol == col - 1)
                        writestr(&write1, terminfo.T_nd);
                    else if (pcol != col)
                        MOVE(&write1, &terminfo, line, col);

                    if ((cells[col].prop & S_STANDOUT) && !(mode & S_STANDOUT)) {
                        writestr(&write1, terminfo.T_so);
                        mode |= S_STANDOUT;
                    }
                    if ((cells[col].prop & S_UNDERLINE) && !(mode & S_UNDERLINE)) {
                        writestr(&write1, terminfo.T_us);
                        mode |= S_UNDERLINE;
                    }
                    if ((cells[col].prop & S_BOLD) && !(mode & S_BOLD)) {
                        writestr(&write1, terminfo.T_md);
                        mode |= S_BOLD;
                    }
                    if ((cells[col].prop & S_COLORED) && (cells[col].prop ^ mode) & COL_FCOLOR) {
                        color = (cells[col].prop & COL_FCOLOR);
                        mode = ((mode & ~COL_FCOLOR) | color);
                        writestr(&write1, color_seq(color));
                    }
                    if ((cells[col].prop & S_BCOLORED)
                        && (cells[col].prop ^ mode) & COL_BCOLOR) {
                        bcolor = (cells[col].prop & COL_BCOLOR);
                        mode = ((mode & ~COL_BCOLOR) | bcolor);
                        writestr(&write1, bcolor_seq(bcolor));
                    }
                    if ((cells[col].prop & S_GRAPHICS) && !(mode & S_GRAPHICS)) {
                        wc_putc_end(&writer);
                        if (!graph_enabled) {
                            graph_enabled = 1;
                            writestr(&write1, terminfo.T_eA);
                        }
                        writestr(&write1, terminfo.T_as);
                        mode |= S_GRAPHICS;
                    }
                    if (cells[col].prop & S_GRAPHICS)
                        write1(graphchar(cells[col].bytes[0]));
                    else if (CHMODE(cells[col].prop) != C_WCHAR2)
                        wc_putc(WcOption, (char*)cells[col].bytes, &writer);
                    pcol = col + 1;
                }
            }
            if (col == COLS)
                moved = RF_NEED_TO_MOVE;
            for (; col < COLS && !(cells[col].prop & S_EOL); col++)
                cells[col].prop |= S_EOL;
        }
        *dirty &= ~(L_NEED_CE | L_CLRTOEOL);
        if (mode & M_MEND) {
            if (mode & (S_COLORED | S_BCOLORED))
                writestr(&write1, terminfo.T_op);
            if (mode & S_GRAPHICS) {
                writestr(&write1, terminfo.T_ae);
                wc_putc_clear_status();
            }
            writestr(&write1, terminfo.T_me);
            mode &= ~M_MEND;
        }
    }
    wc_putc_end(writer);
    MOVE(&write1, &terminfo, CurLine, CurColumn);
    flush_tty();
}

void clear(void)
{
    writestr(&write1, terminfo.T_cl);
    sc_move(0, 0);
    for (int i = 0; i < LINES; i++) {
        ScreenImage[i]->isdirty = 0;
        struct Cell* p = ScreenImage[i]->cells;
        for (int j = 0; j < COLS; j++) {
            p[j].prop = S_EOL;
        }
    }
    CurrentMode = C_ASCII;
}

/* XXX: conflicts with curses's clrtoeol(3) ? */
void clrtoeol(void)
{ /* Clear to the end of line */
    struct Cell* line = ScreenImage[CurLine]->cells;

    if (line[CurColumn].prop & S_EOL)
        return;

    if (!(ScreenImage[CurLine]->isdirty & (L_NEED_CE | L_CLRTOEOL)) || ScreenImage[CurLine]->eol > CurColumn)
        ScreenImage[CurLine]->eol = CurColumn;

    ScreenImage[CurLine]->isdirty |= L_CLRTOEOL;
    touch_line();
    for (int i = CurColumn; i < COLS && !(line[i].prop & S_EOL); i++) {
        line[i].prop = S_EOL | S_DIRTY;
    }
}

static void
clrtoeol_with_bcolor(void)
{
    int i, cli, cco;
    enum CellProperty pr;

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
    sc_move(cli, cco);
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

void addstr(const char* s)
{
    int len;

    while (*s != '\0') {
        len = wtf_len((wc_uchar*)s);
        addmch((CellCharBytes)s, len);
        s += len;
    }
}

void addnstr(const char* s, int n)
{
    for (int i = 0; *s != '\0';) {
        int width = wtf_width(WcOption, *(const wc_uchar*)s);
        if (i + width > n)
            break;
        int len = wtf_len((const wc_uchar*)s);
        addmch((CellCharBytes)s, len);
        s += len;
        i += width;
    }
}

void sc_addnstr_sup(const char* s, int n)
{
    int i = 0;
    for (; *s != '\0';) {
        int width = wtf_width(WcOption, *(const wc_uchar*)s);
        if (i + width > n)
            break;
        int len = wtf_len((const wc_uchar*)s);
        addmch((CellCharBytes)s, len);
        s += len;
        i += width;
    }
    for (; i < n; i++)
        addch(' ');
}
