#pragma once
#include "Str.h"

struct mailcap {
  char *type;
  char *viewer;
  int flags;
  char *test;
  char *nametemplate;
  char *edit;
};

int mailcapMatch(struct mailcap *mcap, const char *type);
struct mailcap *searchMailcap(struct mailcap *table, const char *type);
void initMailcap();
char *acceptableMimeTypes();
struct mailcap *searchExtViewer(const char *type);
Str unquote_mailcap(const char *qstr, const char *type, const char *name,
                    const char *attr, int *mc_stat);
