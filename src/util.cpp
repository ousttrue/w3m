#include "util.h"
#include "display.h"
#include "terms.h"
#include <stdio.h>
#include <stdlib.h>

int exec_cmd(const std::string &cmd) {
  fmTerm();
  if (auto rv = system(cmd.c_str())) {
    printf("\n[Hit any key]");
    fflush(stdout);
    fmInit();
    getch();
    return rv;
  }
  fmInit();

  return 0;
}
