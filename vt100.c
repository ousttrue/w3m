#include "vt100.h"

TermCap *termcap_get() {
  static TermCap s_termcap;
  return &s_termcap;
};
