#pragma once
#include "Str.h"

enum TMPF_TYPE {
  TMPF_DFL = 0,
  TMPF_SRC = 1,
  TMPF_FRAME = 2,
  TMPF_CACHE = 3,
  TMPF_COOKIE = 4,
  MAX_TMPF_TYPE = 5,
};

void tmpfile_init();
void tmpfile_init_no_rcdir(const char *rcdir);
void tmpfile_deletefiles();

void set_tmpdir(const char *dir);
const char *get_tmpdir();
Str tmpfname(enum TMPF_TYPE type, const char *ext);
