#include "term_renderer.h"
#include "term_entry.h"
#include "frame.h"
#include "graphicchar.h"
#include <stdio.h>

int highIntensityColors = 0;

enum RF_MODE {
    RF_NEED_TO_MOVE = 0,
    RF_CR_OK = 1,
    RF_NONEED_TO_MOVE = 2,
};

void termBell(const struct Writer* writer)
{
    putWriter(writer, 7);
}

void termClear(const struct Writer* writer)
{
    putsWriter(writer, getTermEntry()->cl);
}

void MOVE(const struct Writer* writer, int line, int column)
{
    putsWriter(writer, getMoveXY(column, line));
}

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

void refreshFrame(const struct Writer* writer, struct Frame* frame)
{
    struct TermEntry* t = getTermEntry();
    // enum RF_MODE moved = RF_NEED_TO_MOVE;
    l_prop mode = 0;
    l_prop color = COL_FTERM;
    l_prop bcolor = COL_BTERM;
    struct Cell* cell = frame->cells;
    for (int line = 0; line < frame->rows; ++line) {
        MOVE(writer, line, 0);
        // moved = RF_CR_OK;
        for (int col = 0; col < frame->cols; ++col, ++cell) {

            // if (cell->prop & S_EOL)
            //     break;

            /*
             * some terminal emulators do linefeed when a
             * character is put on getCols()-th column. this behavior
             * is different from one of vt100, but such terminal
             * emulators are used as vt100-compatible
             * emulators. This behaviour causes scroll when a
             * character is drawn on (getCols()-1,getLines()-1) point.  To
             * avoid the scroll, I prohibit to draw character on
             * (getCols()-1,getLines()-1).
             */
            if ((!(cell->prop & S_STANDOUT) && (mode & S_STANDOUT)) || (!(cell->prop & S_UNDERLINE) && (mode & S_UNDERLINE)) || (!(cell->prop & S_BOLD) && (mode & S_BOLD)) || (!(cell->prop & S_COLORED) && (mode & S_COLORED))
                || (!(cell->prop & S_BCOLORED) && (mode & S_BCOLORED))
                || (!(cell->prop & S_GRAPHICS) && (mode & S_GRAPHICS))) {
                if ((mode & S_COLORED)
                    || (mode & S_BCOLORED))
                    putsWriter(writer, t->op);
                if (mode & S_GRAPHICS)
                    putsWriter(writer, t->ae);
                putsWriter(writer, t->me);
                mode &= ~M_MEND;
            } // {
            //     if (pcol == col - 1)
            //         putsWriter(writer, t->nd);
            //     else if (pcol != col)
            //         MOVE(writer, line, col);

            if ((cell->prop & S_STANDOUT) && !(mode & S_STANDOUT)) {
                putsWriter(writer, t->so);
                mode |= S_STANDOUT;
            }
            if ((cell->prop & S_UNDERLINE) && !(mode & S_UNDERLINE)) {
                putsWriter(writer, t->us);
                mode |= S_UNDERLINE;
            }
            if ((cell->prop & S_BOLD) && !(mode & S_BOLD)) {
                putsWriter(writer, t->md);
                mode |= S_BOLD;
            }
            if ((cell->prop & S_COLORED) && (cell->prop ^ mode) & COL_FCOLOR) {
                color = (cell->prop & COL_FCOLOR);
                mode = ((mode & ~COL_FCOLOR) | color);
                putsWriter(writer, color_seq(color));
            }
            if ((cell->prop & S_BCOLORED)
                && (cell->prop ^ mode) & COL_BCOLOR) {
                bcolor = (cell->prop & COL_BCOLOR);
                mode = ((mode & ~COL_BCOLOR) | bcolor);
                putsWriter(writer, bcolor_seq(bcolor));
            }
            //     if ((pr[col] & S_GRAPHICS) && !(mode & S_GRAPHICS)) {
            //         wc_putc_end(writer);
            //         if (!vt->graph_enabled) {
            //             vt->graph_enabled = 1;
            //             putsWriter(writer, t->eA);
            //         }
            //         putsWriter(writer, t->as);
            //         mode |= S_GRAPHICS;
            //     }
            // if (cell->prop & S_GRAPHICS){
            //     putWriter(writer, graphchar(*pc[col]));
            // }
            // else
            if (cell->prop & S_EOL) {

                putWriter(writer, ' ');
            } else if (CHMODE(cell->prop) != C_WCHAR2) {
                // wc_putc(writer, pc[col]);
                putsWriter(writer, cell->str);
            }
            //     pcol = col + 1;
            // }
        }
        // if (col == getCols())
        //     moved = RF_NEED_TO_MOVE;
        // for (; col < getCols() && !(pr[col] & S_EOL); col++)
        //     pr[col] |= S_EOL;
    }
    // *dirty &= ~(L_NEED_CE | L_CLRTOEOL);
    // if (mode & M_MEND) {
    //     if (mode & (S_COLORED | S_BCOLORED))
    //         putsWriter(writer, t->op);
    //     if (mode & S_GRAPHICS) {
    //         putsWriter(writer, t->ae);
    //         wc_putc_clear_status();
    //     }
    //     putsWriter(writer, t->me);
    //     mode &= ~M_MEND;
    // }
}

// static int
// need_redraw(char* c1, l_prop pr1, char* c2, l_prop pr2)
// {
//     if (!c1 || !c2 || strcmp(c1, c2))
//         return 1;
//     if (*c1 == ' ')
//         return (pr1 ^ pr2) & M_SPACE & ~S_DIRTY;
//
//     if ((pr1 ^ pr2) & ~S_DIRTY)
//         return 1;
//
//     return 0;
// }
//
// #define SPACE " "
//
// void refreshLine(const struct Writer* writer, struct VirtualTerm* vt, int line)
// {
//     struct TermEntry* t = getTermEntry();
//     int pline = vt->CurLine;
//     enum RF_MODE moved = RF_NEED_TO_MOVE;
//     l_prop mode = 0;
//     l_prop color = COL_FTERM;
//     l_prop bcolor = COL_BTERM;
//     Screen* l = vt->ScreenImage[line];
//     enum LineStatus* dirty = &l->isdirty;
//     if (*dirty & L_DIRTY) {
//         *dirty &= ~L_DIRTY;
//         char** pc = l->lineimage;
//         l_prop* pr = l->lineprop;
//         int col = 0;
//         // for (; col < getCols() && !(pr[col] & S_EOL); col++) {
//         //     if (*dirty & L_NEED_CE && col >= l->eol) {
//         //         if (need_redraw(pc[col], pr[col], SPACE, 0))
//         //             break;
//         //     } else {
//         //         if (pr[col] & S_DIRTY)
//         //             break;
//         //     }
//         // }
//
//         int pcol = col;
//         // if (*dirty & (L_NEED_CE | L_CLRTOEOL)) {
//         //     pcol = l->eol;
//         //     if (pcol >= getCols()) {
//         //         *dirty &= ~(L_NEED_CE | L_CLRTOEOL);
//         //         pcol = col;
//         //     }
//         // }
//         // if (line < getLines() - 2 && pline == line - 1 && pcol == 0) {
//         //     switch (moved) {
//         //     case RF_NEED_TO_MOVE:
//         //         MOVE(writer, line, 0);
//         //         moved = RF_CR_OK;
//         //         break;
//         //     case RF_CR_OK:
//         //         putWriter(writer, '\n');
//         //         putWriter(writer, '\r');
//         //         break;
//         //     case RF_NONEED_TO_MOVE:
//         //         moved = RF_CR_OK;
//         //         break;
//         //     }
//         // } else {
//         MOVE(writer, line, pcol);
//         moved = RF_CR_OK;
//         // }
//         if (*dirty & (L_NEED_CE | L_CLRTOEOL)) {
//             putsWriter(writer, t->ce);
//             if (col != pcol)
//                 MOVE(writer, line, col);
//         }
//         pline = line;
//         pcol = col;
//         for (; col < getCols(); col++) {
//             if (pr[col] & S_EOL)
//                 break;
//
//             /*
//              * some terminal emulators do linefeed when a
//              * character is put on getCols()-th column. this behavior
//              * is different from one of vt100, but such terminal
//              * emulators are used as vt100-compatible
//              * emulators. This behaviour causes scroll when a
//              * character is drawn on (getCols()-1,getLines()-1) point.  To
//              * avoid the scroll, I prohibit to draw character on
//              * (getCols()-1,getLines()-1).
//              */
//             if ((!(pr[col] & S_STANDOUT) && (mode & S_STANDOUT)) || (!(pr[col] & S_UNDERLINE) && (mode & S_UNDERLINE)) || (!(pr[col] & S_BOLD) && (mode & S_BOLD)) || (!(pr[col] & S_COLORED) && (mode & S_COLORED))
//                 || (!(pr[col] & S_BCOLORED) && (mode & S_BCOLORED))
//                 || (!(pr[col] & S_GRAPHICS) && (mode & S_GRAPHICS))) {
//                 if ((mode & S_COLORED)
//                     || (mode & S_BCOLORED))
//                     putsWriter(writer, t->op);
//                 if (mode & S_GRAPHICS)
//                     putsWriter(writer, t->ae);
//                 putsWriter(writer, t->me);
//                 mode &= ~M_MEND;
//             }
//             if ((*dirty & L_NEED_CE && col >= l->eol) ? need_redraw(pc[col], pr[col], SPACE,
//                                                             0)
//                                                       : (pr[col] & S_DIRTY)) {
//                 if (pcol == col - 1)
//                     putsWriter(writer, t->nd);
//                 else if (pcol != col)
//                     MOVE(writer, line, col);
//
//                 if ((pr[col] & S_STANDOUT) && !(mode & S_STANDOUT)) {
//                     putsWriter(writer, t->so);
//                     mode |= S_STANDOUT;
//                 }
//                 if ((pr[col] & S_UNDERLINE) && !(mode & S_UNDERLINE)) {
//                     putsWriter(writer, t->us);
//                     mode |= S_UNDERLINE;
//                 }
//                 if ((pr[col] & S_BOLD) && !(mode & S_BOLD)) {
//                     putsWriter(writer, t->md);
//                     mode |= S_BOLD;
//                 }
//                 if ((pr[col] & S_COLORED) && (pr[col] ^ mode) & COL_FCOLOR) {
//                     color = (pr[col] & COL_FCOLOR);
//                     mode = ((mode & ~COL_FCOLOR) | color);
//                     putsWriter(writer, color_seq(color));
//                 }
//                 if ((pr[col] & S_BCOLORED)
//                     && (pr[col] ^ mode) & COL_BCOLOR) {
//                     bcolor = (pr[col] & COL_BCOLOR);
//                     mode = ((mode & ~COL_BCOLOR) | bcolor);
//                     putsWriter(writer, bcolor_seq(bcolor));
//                 }
//                 if ((pr[col] & S_GRAPHICS) && !(mode & S_GRAPHICS)) {
//                     wc_putc_end(writer);
//                     if (!vt->graph_enabled) {
//                         vt->graph_enabled = 1;
//                         putsWriter(writer, t->eA);
//                     }
//                     putsWriter(writer, t->as);
//                     mode |= S_GRAPHICS;
//                 }
//                 if (pr[col] & S_GRAPHICS)
//                     putWriter(writer, graphchar(*pc[col]));
//                 else if (CHMODE(pr[col]) != C_WCHAR2) {
//                     wc_putc(writer, pc[col]);
//                 }
//                 pcol = col + 1;
//             }
//         }
//         if (col == getCols())
//             moved = RF_NEED_TO_MOVE;
//         for (; col < getCols() && !(pr[col] & S_EOL); col++)
//             pr[col] |= S_EOL;
//     }
//     *dirty &= ~(L_NEED_CE | L_CLRTOEOL);
//     if (mode & M_MEND) {
//         if (mode & (S_COLORED | S_BCOLORED))
//             putsWriter(writer, t->op);
//         if (mode & S_GRAPHICS) {
//             putsWriter(writer, t->ae);
//             wc_putc_clear_status();
//         }
//         putsWriter(writer, t->me);
//         mode &= ~M_MEND;
//     }
// }

