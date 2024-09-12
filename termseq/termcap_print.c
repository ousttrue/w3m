#include "termcap_load.h"
#include <stdio.h>

void print_capability(const char *name, const char *cap) {
  printf("%s => ", name);
  for (auto p = cap; *p; ++p) {
    if (*p == 0x1b) {
      printf("[esc]");
    } else {
      putc(*p, stdout);
    }
  }
  printf("\n");
}

int main(int argc, char **argv) {
  auto term = "xterm";
  TermCap entry;
  if (!termcap_load(term, &entry)) {
    return 1;
  }

  printf("%s\n", term);
  print_capability("cd", entry.T_cd);
  print_capability("ce", entry.T_ce);

  return 0;
}
