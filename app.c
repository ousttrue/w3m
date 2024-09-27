#include "app.h"
#include "textlist.h"
#include "indep.h"

int CurrentPid = -1;
const char *tmp_dir = nullptr;
void app_set_tmpdir(const char *dir) { tmp_dir = dir; }
const char *app_get_tmpdir() { return tmp_dir; }

static char *tmpf_base[MAX_TMPF_TYPE] = {
    "tmp", "src", "frame", "cache", "cookie",
};

struct TextList *fileToDelete = nullptr;

void app_init() { fileToDelete = newTextList(); }

void app_no_rcdir(const char *rcdir) {
  if (((tmp_dir = getenv("TMPDIR")) == NULL || *tmp_dir == '\0') &&
      ((tmp_dir = getenv("TMP")) == NULL || *tmp_dir == '\0') &&
      ((tmp_dir = getenv("TEMP")) == NULL || *tmp_dir == '\0'))
    tmp_dir = "/tmp";
#ifndef _WIN32
  tmp_dir = mkdtemp(Strnew_m_charp(tmp_dir, "/w3m-XXXXXX", NULL)->ptr);
  if (tmp_dir == NULL)
    tmp_dir = rc_dir;
#endif
}

static unsigned int tmpf_seq[MAX_TMPF_TYPE];

Str tmpfname(enum TMPF_TYPE type, const char *ext) {
  auto tmpf = Sprintf("%s/w3m%s%d-%d%s", tmp_dir, tmpf_base[type], CurrentPid,
                      tmpf_seq[type]++, (ext) ? ext : "");
  pushText(fileToDelete, tmpf->ptr);
  return tmpf;
}
