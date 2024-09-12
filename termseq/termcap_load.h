#pragma once
#include "termcap_entry.h"

bool termcap_load(const char *ent, TermCap *entry);
void termcap_load_xterm(TermCap *entry);
