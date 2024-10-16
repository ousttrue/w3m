#include "symbol.h"
#include "fm.h"
#include "ctrlcode.h"
#include "symbols/alt.sym"
#include "symbols/graph.sym"

char **get_symbol(void) { return alt_symbol; }

void push_symbol(Str str, char symbol, int width, int n) {
  char buf[2], *p;
  int i;

  p = alt_symbol[(unsigned char)symbol % N_SYMBOL];
  for (i = 0; i < 2 && *p; i++, p++)
    buf[i] = (*p == ' ') ? NBSP_CODE : *p;

  Strcat(str, Sprintf("<_SYMBOL TYPE=%d>", symbol));
  for (; n > 0; n--)
    Strcat_charp_n(str, buf, i);
  Strcat_charp(str, "</_SYMBOL>");
}
