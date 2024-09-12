#pragma once
#include "termcap_entry.h"

bool termcap_load(TermCap *t, const char *ent);
void termcap_load_xterm(TermCap *t);
