#include "push_symbol.h"
#include "Str.h"

void push_symbol(Str *str, char symbol, int width, int n) {
  // char buf[2];
  //
  // auto p = alt_symbol[(unsigned char)symbol % N_SYMBOL];
  // int i = 0;
  // for (; i < 2 && *p; i++, p++)
  //   buf[i] = (*p == ' ') ? (char)NBSP_CODE : *p;
  //
  // Strcat(str, Sprintf("<_SYMBOL TYPE=%d>", symbol));
  // for (; n > 0; n--)
  //   Strcat_charp_n(str, buf, i);
  // Strcat_charp(str, "</_SYMBOL>");
  for (int i = 0; i < n; ++i) {
    Strcat_char(str, '*');
  }
}

void push_symbol(std::ostream &os, char symbol, int width, int n) {
  for (int i = 0; i < n; ++i) {
    os << '*';
  }
}
