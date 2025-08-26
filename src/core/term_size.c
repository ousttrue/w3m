#include "term_size.h"
#include <stdlib.h>
#include <sys/ioctl.h>

#include <term.h>

#define MAX_LINE 200
#define MAX_COLUMN 400
static int LINES, COLS;
int getLines()
{
    return LINES;
}
int getCols()
{
    return COLS;
}

int get_rowcol_tty(int tty, int* row, int* col)
{
    struct winsize wins;
    int i = ioctl(tty, TIOCGWINSZ, &wins);
    if (i >= 0) {
        *row = wins.ws_row;
        *col = wins.ws_col;
    }
    return i;
}

void setlinescols(int tty)
{
    LINES = COLS = 0;

    int row = 0;
    int col = 0;
    int i = get_rowcol_tty(tty, &row, &col);
    if (i >= 0 && row != 0 && col != 0) {
        LINES = row;
        COLS = col;
    }

    char* p;
    if (LINES <= 0 && (p = getenv("LINES")) != NULL && (i = atoi(p)) >= 0)
        LINES = i;
    if (COLS <= 0 && (p = getenv("COLUMNS")) != NULL && (i = atoi(p)) >= 0)
        COLS = i;

    // term.h
    if (LINES <= 0)
        LINES = tgetnum("li"); /* number of line */
    if (COLS <= 0)
        COLS = tgetnum("co"); /* number of column */

    if (COLS > MAX_COLUMN)
        COLS = MAX_COLUMN;
    if (LINES > MAX_LINE)
        LINES = MAX_LINE;
}
