#pragma once
#include <stdio.h>

extern const char* displayTitleTerm;

void set_tty(void);
void close_tty(void);
FILE* get_ttyf();
int get_tty_fd();
int get_rowcol_tty(int* row, int* col);

void ttymode_set(int mode, int imode);
void ttymode_reset(int mode, int imode);
void set_cc(int spec, int val);
void TerminalSet(void*);
void crmode(void);
void nocrmode(void);
void term_raw(void);
void term_echo(void);
void term_noecho(void);
void term_cooked(void);
void term_cbreak(void);

int sleep_till_anykey(int timeout_ms, int purge);
int write1(int);
// void writestr(char* s);
static void writestr(const char* s)
{
    // tputs(s, 1, &write1);
    for(; *s; ++s)
    {
        write1(*s);
    }
}
void flush_tty(void);

char* ttyname_tty(void);
void term_title(const char* s);
int get_pixel_per_cell(int* ppc, int* ppl);
