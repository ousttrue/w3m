#include "push_symbol.h"


void push_symbol(std::ostream &os, char symbol, int width, int n) {
  for (int i = 0; i < n; ++i) {
    os << '*';
  }
}
