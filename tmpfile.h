#pragma once

#define TMPF_DFL 0
#define TMPF_SRC 1
#define TMPF_FRAME 2
#define TMPF_CACHE 3
#define TMPF_COOKIE 4
#define TMPF_HIST 5
#define MAX_TMPF_TYPE 6

extern char *tmp_dir;

struct Str;
Str *tmpfname(int type, const char *ext);
