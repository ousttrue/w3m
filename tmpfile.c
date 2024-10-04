#include "tmpfile.h"
#include "alloc.h"
#include "app.h"
#include "textlist.h"
#include "display.h"

static struct TextList *fileToDelete = nullptr;
void tmpfile_init() { fileToDelete = newTextList(); }

static const char *tmp_dir = nullptr;
void tmpfile_init_no_rcdir(const char *rcdir) {
  if (((tmp_dir = getenv("TMPDIR")) == NULL || *tmp_dir == '\0') &&
      ((tmp_dir = getenv("TMP")) == NULL || *tmp_dir == '\0') &&
      ((tmp_dir = getenv("TEMP")) == NULL || *tmp_dir == '\0'))
    tmp_dir = "/tmp";
#ifndef _WIN32
  tmp_dir = mkdtemp(Strnew_m_charp(tmp_dir, "/w3m-XXXXXX", NULL)->ptr);
  if (tmp_dir == NULL)
    tmp_dir = rcdir;
#endif
}

void set_tmpdir(const char *dir) { tmp_dir = dir; }
const char *get_tmpdir() { return tmp_dir; }

static char *tmpf_base[MAX_TMPF_TYPE] = {
    "tmp", "src", "frame", "cache", "cookie",
};

static unsigned int tmpf_seq[MAX_TMPF_TYPE];

Str tmpfname(enum TMPF_TYPE type, const char *ext) {
  auto tmpf = Sprintf("%s/w3m%s%d-%d%s", tmp_dir, tmpf_base[type], CurrentPid,
                      tmpf_seq[type]++, (ext) ? ext : "");
  pushText(fileToDelete, tmpf->ptr);
  return tmpf;
}

#define INLINE_IMG_NONE 0
#define INLINE_IMG_OSC5379 1
#define INLINE_IMG_SIXEL 2
#define INLINE_IMG_ITERM2 3
#define INLINE_IMG_KITTY 4

void tmpfile_deletefiles() {
  const char *f;
  while ((f = popText(fileToDelete)) != NULL) {
    unlink(f);
    if (enable_inline_image == INLINE_IMG_SIXEL &&
        strcmp(f + strlen(f) - 4, ".gif") == 0) {
      Str firstframe = Strnew_charp(f);
      Strcat_charp(firstframe, "-1");
      unlink(firstframe->ptr);
    }
  }
}
