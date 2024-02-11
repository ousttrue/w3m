#pragma once

/* Search Result */
#define SR_FOUND 0x1
#define SR_NOTFOUND 0x2
#define SR_WRAPPED 0x4

struct LineLayout;
int forwardSearch(LineLayout *layout, const char *str);
int backwardSearch(LineLayout *layout, const char *str);
int srchcore(const char *str, int (*func)(LineLayout *, const char *));
void isrch(int (*func)(LineLayout *, const char *), const char *prompt);
void srch(int (*func)(LineLayout *, const char *), const char *prompt);
void srch_nxtprv(int reverse);
