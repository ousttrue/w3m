#include "symbol.h"
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

Str *conv_symbol(Line *l) {
  Str *tmp = NULL;
  char *p = l->lineBuf.data();
  char *ep = p + l->len;
  Lineprop *pr = l->propBuf.data();
  auto symbol = get_symbol();

  for (; p < ep; p++, pr++) {
    if (*pr & PC_SYMBOL) {
      char c = *p - SYMBOL_BASE;
      if (tmp == NULL) {
        tmp = Strnew_size(l->len);
        Strcopy_charp_n(tmp, l->lineBuf.data(), p - l->lineBuf.data());
      }
      Strcat_charp(tmp, symbol[(unsigned char)c % N_SYMBOL]);
    } else if (tmp != NULL)
      Strcat_char(tmp, *p);
  }
  if (tmp)
    return tmp;
  else
    return Strnew_charp_n(l->lineBuf.data(), l->len);
}
