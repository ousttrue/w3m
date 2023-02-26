#pragma once
#include <stdio.h>
#include <optional>
#include <tuple>

namespace tty
{
// low
void set();
int get();
void reset();
void flush();
FILE *get_file();

void close();
char *get_name();

char getch();
int write1(char c);
void writestr(char *s);

// mode
void set_cc(int spec, int val);
void set_mode(int mode, int imode);
void reset_mode(int mode, int imode);
void term_raw();
void crmode(void);
void nocrmode(void);
void term_echo(void);
void term_noecho(void);
void term_cooked(void);
void term_cbreak(void);

// high
std::optional<std::tuple<unsigned short, unsigned short>> get_lines_cols();
int sleep_till_anykey(int sec, int purge);
int get_pixel_per_cell(int *ppc, int *ppl);

} // namespace tty
