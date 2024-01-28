#pragma once
#include "Str.h"

Str unquote_mailcap(char *qstr, const char *type, char *name, char *attr,
                    int *mc_stat);
struct mailcap *searchExtViewer(const char *type);
struct mailcap *searchMailcap(struct mailcap *table, const char *type);
