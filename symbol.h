#ifndef W3M_SYMBOL_H
#define W3M_SYMBOL_H

#include "Str.h"
#include "config.h"
#include "url.h"

#define N_GRAPH_SYMBOL 32
#define N_SYMBOL (N_GRAPH_SYMBOL + 14)
#define SYMBOL_BASE 0x20

extern char *graph_symbol[];
extern char *graph2_symbol[];
extern int symbol_width;
extern int symbol_width0;
#ifdef USE_M17N
extern char **get_symbol(wc_ces charset, int *width);
extern char **set_symbol(int width);
extern Str convertLine(URLFile *uf, Str line, int mode, wc_ces * charset,
		       wc_ces doc_charset);
#else
extern char **get_symbol(void);
extern Str convertLine0(URLFile *uf, Str line, int mode);
#define convertLine(uf,line,mode,charset,dcharset) convertLine0(uf,line,mode)
#endif
extern void push_symbol(Str str, char symbol, int width, int n);
#ifdef USE_UNICODE
extern void update_utf8_symbol(void);
#endif
#endif
