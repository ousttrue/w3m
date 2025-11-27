#include "tui.h"
#include "w3m_runtime.h"
#include "terms.h"
#include "term_entry.h"
#include "textlist.h"
#include "screen.h"
#include "display.h"
#include "image.h"
#include "buffer.h"
#include "indep.h"
#include <stdio.h>
#include <stdlib.h>

int fmInitialized = false;
int highIntensityColors = false;

static GeneralList* message_list = NULL;
static char* delayed_msg = NULL;

void tui_enter()
{
    if (!fmInitialized) {
        initscr();
        term_raw();
        term_noecho();
        if (displayImage)
            initImage();
    }
    fmInitialized = true;
}

void tui_exit()
{
    if (fmInitialized) {
        scr_move(LINES - 1, 0);
        scr_clrtoeolx();
        tui_render_screen();
        if (activeImage)
            loadImage(NULL, IMG_FLAG_STOP);
        tty_reset();
        fmInitialized = false;
    }
}

int tui_exec(const char* cmd)
{
    tui_exit();
    int rv = system(cmd);
    if (rv) {
        printf("\n[Hit any key]");
        fflush(stdout);
        tui_exit();
        getch();
        return rv;
    }
    tui_enter();

    return 0;
}

void tui_record_err_message(char* s)
{
    if (fmInitialized) {
        if (!message_list)
            message_list = newGeneralList();
        if (message_list->nitem >= LINES)
            popValue(message_list);
        pushValue(message_list, allocStr(s, -1));
    }
}

Buffer* tui_message_list_panel()
{
    Str tmp = Strnew_size(LINES * COLS);
    ListItem* p;

    /* FIXME: gettextize? */
    Strcat_charp(tmp,
        "<html><head><title>List of error messages</title></head><body>"
        "<h1>List of error messages</h1><table cellpadding=0>\n");
    if (message_list)
        for (p = message_list->last; p; p = p->prev)
            Strcat_m_charp(tmp, "<tr><td><pre>", html_quote(p->ptr),
                "</pre></td></tr>\n", NULL);
    else
        Strcat_charp(tmp, "<tr><td>(no message recorded)</td></tr>\n");
    Strcat_charp(tmp, "</table></body></html>");
    return loadHTMLString(tmp);
}

void tui_message(char* s, int return_x, int return_y)
{
    if (!fmInitialized)
        return;
    scr_move(LINES - 1, 0);
    scr_addnstr(s, COLS - 1);
    scr_clrtoeolx();
    scr_move(return_y, return_x);
}

void tui_disp_err_message(char* s, int redraw_current)
{
    tui_record_err_message(s);
    tui_disp_message(s, redraw_current);
}

void tui_disp_message_nsec(char* s, int redraw_current, int sec, int purge, int mouse)
{
    // if (QuietMessage)
    //     return;
    if (!fmInitialized) {
        fprintf(stderr, "%s\n", conv_to_system(s));
        return;
    }
    if (CurrentTab != NULL && Currentbuf != NULL)
        tui_message(s, Currentbuf->cursorX + Currentbuf->rootX,
            Currentbuf->cursorY + Currentbuf->rootY);
    else
        tui_message(s, LINES - 1, 0);
    tui_render_screen();
    tty_sleep_till_anykey(sec, purge);
    if (CurrentTab != NULL && Currentbuf != NULL && redraw_current)
        displayBuffer(Currentbuf, B_NORMAL);
}

void tui_disp_message(char* s, int redraw_current)
{
    tui_disp_message_nsec(s, redraw_current, 10, FALSE, TRUE);
}

void tui_disp_message_nomouse(char* s, int redraw_current)
{
    tui_disp_message_nsec(s, redraw_current, 10, FALSE, FALSE);
}

void tui_set_delayed_message(char* s)
{
    delayed_msg = allocStr(s, -1);
}

void tui_render_delayed_msg()
{
    if (delayed_msg != NULL) {
        tui_disp_message(delayed_msg, FALSE);
        delayed_msg = NULL;
        tui_render_screen();
    }
}

//
//
//

static char*
color_seq(int colmode)
{
    static char seqbuf[32];
    sprintf(seqbuf, "\033[%dm", ((colmode >> 8) & 7) + (highIntensityColors ? 90 : 30));
    return seqbuf;
}

static char*
bcolor_seq(int colmode)
{
    static char seqbuf[32];
    sprintf(seqbuf, "\033[%dm", ((colmode >> 12) & 7) + 40);
    return seqbuf;
}

static bool graph_enabled = false;
#define graphchar(c) (((unsigned)(c) >= ' ' && (unsigned)(c) < 128) ? T_.gcmap[(c) - ' '] : (c))

#define SPACE " "
#define RF_NEED_TO_MOVE 0
#define RF_CR_OK 1
#define RF_NONEED_TO_MOVE 2
#define M_MEND (S_STANDOUT | S_UNDERLINE | S_BOLD | S_COLORED | S_BCOLORED | S_GRAPHICS)
void tui_render_screen(void)
{
    if (!fmInitialized) {
        return;
    }

    struct Screen sc = scr_get();

    int line, col, pcol;
    int pline = sc.CurLine;
    int moved = RF_NEED_TO_MOVE;
    uint16_t *pr, mode = 0;
    uint16_t color = COL_FTERM;
    uint16_t bcolor = COL_BTERM;
    short* dirty;

    wc_putc_init(InnerCharset, DisplayCharset);
    for (line = 0; line <= LINES - 1; line++) {
        dirty = &sc.ScreenImage[line]->isdirty;
        if (*dirty & L_DIRTY) {
            *dirty &= ~L_DIRTY;
            char** pc;
            pc = sc.ScreenImage[line]->lineimage;
            pr = sc.ScreenImage[line]->lineprop;
            for (col = 0; col < COLS && !(pr[col] & S_EOL); col++) {
                if (*dirty & L_NEED_CE && col >= sc.ScreenImage[line]->eol) {
                    if (scr_is_need_redraw(pc[col], pr[col], SPACE, 0))
                        break;
                } else {
                    if (pr[col] & S_DIRTY)
                        break;
                }
            }
            if (*dirty & (L_NEED_CE | L_CLRTOEOL)) {
                pcol = sc.ScreenImage[line]->eol;
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
                    tty_move(line, 0);
                    moved = RF_CR_OK;
                    break;
                case RF_CR_OK:
                    tty_putc('\n');
                    tty_putc('\r');
                    break;
                case RF_NONEED_TO_MOVE:
                    moved = RF_CR_OK;
                    break;
                }
            } else {
                tty_move(line, pcol);
                moved = RF_CR_OK;
            }
            if (*dirty & (L_NEED_CE | L_CLRTOEOL)) {
                tty_write(T_.ce);
                if (col != pcol)
                    tty_move(line, col);
            }
            pline = line;
            pcol = col;
            for (; col < COLS; col++) {
                if (pr[col] & S_EOL)
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
#if !defined(USE_BG_COLOR) || defined(__CYGWIN__)
                if (line == LINES - 1 && col == COLS - 1)
                    break;
#endif /* !defined(USE_BG_COLOR) || defined(__CYGWIN__) */
                if ((!(pr[col] & S_STANDOUT) && (mode & S_STANDOUT)) || (!(pr[col] & S_UNDERLINE) && (mode & S_UNDERLINE)) || (!(pr[col] & S_BOLD) && (mode & S_BOLD)) || (!(pr[col] & S_COLORED) && (mode & S_COLORED))
                    || (!(pr[col] & S_BCOLORED) && (mode & S_BCOLORED))
                    || (!(pr[col] & S_GRAPHICS) && (mode & S_GRAPHICS))) {
                    if ((mode & S_COLORED)
                        || (mode & S_BCOLORED))
                        tty_write(T_.op);
                    if (mode & S_GRAPHICS)
                        tty_write(T_.ae);
                    tty_write(T_.me);
                    mode &= ~M_MEND;
                }
                if ((*dirty & L_NEED_CE && col >= sc.ScreenImage[line]->eol) ? scr_is_need_redraw(pc[col], pr[col], SPACE,
                                                                                   0)
                                                                             : (pr[col] & S_DIRTY)) {
                    if (pcol == col - 1)
                        tty_write(T_.nd);
                    else if (pcol != col)
                        tty_move(line, col);

                    if ((pr[col] & S_STANDOUT) && !(mode & S_STANDOUT)) {
                        tty_write(T_.so);
                        mode |= S_STANDOUT;
                    }
                    if ((pr[col] & S_UNDERLINE) && !(mode & S_UNDERLINE)) {
                        tty_write(T_.us);
                        mode |= S_UNDERLINE;
                    }
                    if ((pr[col] & S_BOLD) && !(mode & S_BOLD)) {
                        tty_write(T_.md);
                        mode |= S_BOLD;
                    }
                    if ((pr[col] & S_COLORED) && (pr[col] ^ mode) & COL_FCOLOR) {
                        color = (pr[col] & COL_FCOLOR);
                        mode = ((mode & ~COL_FCOLOR) | color);
                        tty_write(color_seq(color));
                    }
                    if ((pr[col] & S_BCOLORED)
                        && (pr[col] ^ mode) & COL_BCOLOR) {
                        bcolor = (pr[col] & COL_BCOLOR);
                        mode = ((mode & ~COL_BCOLOR) | bcolor);
                        tty_write(bcolor_seq(bcolor));
                    }
                    if ((pr[col] & S_GRAPHICS) && !(mode & S_GRAPHICS)) {
                        tty_wc_putc_end();
                        if (!graph_enabled) {
                            graph_enabled = 1;
                            tty_write(T_.eA);
                        }
                        tty_write(T_.as);
                        mode |= S_GRAPHICS;
                    }
                    if (pr[col] & S_GRAPHICS)
                        tty_putc(graphchar(*pc[col]));
                    else if (CHMODE(pr[col]) != C_WCHAR2)
                        tty_wc_putc(pc[col]);
                    pcol = col + 1;
                }
            }
            if (col == COLS)
                moved = RF_NEED_TO_MOVE;
            for (; col < COLS && !(pr[col] & S_EOL); col++)
                pr[col] |= S_EOL;
        }
        *dirty &= ~(L_NEED_CE | L_CLRTOEOL);
        if (mode & M_MEND) {
            if (mode & (S_COLORED | S_BCOLORED))
                tty_write(T_.op);
            if (mode & S_GRAPHICS) {
                tty_write(T_.ae);
                wc_putc_clear_status();
            }
            tty_write(T_.me);
            mode &= ~M_MEND;
        }
    }
    tty_wc_putc_end();
    tty_move(sc.CurLine, sc.CurColumn);
    tty_flush();
}
