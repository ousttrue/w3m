#include "Str.h"
#include "readbuffer.h"
#include "ctrlcode.h"
#include "Symbols/alt.sym"
#include "Symbols/graph.sym"

const char **get_symbol(void) { return alt_symbol; }

void push_symbol(Str *str, char symbol, int width, int n) {
  char buf[2];

  auto p = alt_symbol[(unsigned char)symbol % N_SYMBOL];
  int i = 0;
  for (; i < 2 && *p; i++, p++)
    buf[i] = (*p == ' ') ? (char)NBSP_CODE : *p;

  Strcat(str, Sprintf("<_SYMBOL TYPE=%d>", symbol));
  for (; n > 0; n--)
    Strcat_charp_n(str, buf, i);
  Strcat_charp(str, "</_SYMBOL>");
}
