#include "symbol.h"
#include "ctrlcode.h"
#include "Symbols/alt.sym"
#include "Symbols/graph.sym"
#include "line.h"
#include <sstream>

const char **get_symbol() { return alt_symbol; }

#define N_SYMBOL (N_GRAPH_SYMBOL + 14)

std::string conv_symbol(Line *l) {
  std::stringstream tmp;
  auto p = l->lineBuf();
  auto ep = p + l->len();
  auto pr = l->propBuf();
  auto symbol = get_symbol();
  for (; p < ep; p++, pr++) {
    if (*pr & PC_SYMBOL) {
      char c = *p - SYMBOL_BASE;
      tmp << symbol[(unsigned char)c % N_SYMBOL];
    } else {
      tmp << *p;
    }
  }
  return tmp.str();
}
