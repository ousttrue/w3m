#pragma once

extern int LINES, COLS;
#define LASTLINE (LINES - 1)

#define GRAPHIC_CHAR_ASCII 2
#define GRAPHIC_CHAR_DEC 1
#define GRAPHIC_CHAR_CHARSET 0

extern bool Do_not_use_ti_te;
extern char *displayTitleTerm;
extern char UseGraphicChar;

char getch(void);
