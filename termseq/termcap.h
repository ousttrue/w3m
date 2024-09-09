#pragma once

typedef int (*outfun)(char);
void tputs(const char *str, int nlines, outfun callback);
char *tgoto(const char *, int, int);
