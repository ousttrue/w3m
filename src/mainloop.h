#pragma once

extern bool vi_prec_num;

void mainloop();
void escKeyProc(int c, int esc, unsigned char *map);
void multiKeyProc();
int scrollNum();
int searchKeyNum();
void clearKeyData();
const char *goLineStr();
int getHseq(int nmark);
int getLastHseq(int nmark);
int precNum();
char tty_getch();
