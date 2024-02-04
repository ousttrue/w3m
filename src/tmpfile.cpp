#include "tmpfile.h"
#include "Str.h"
#include "rc.h"
#include "w3m.h"
#include "textlist.h"

const char *tmp_dir;

static const char *tmpf_base[MAX_TMPF_TYPE] = {
    "tmp", "src", "frame", "cache", "cookie", "hist",
};
static unsigned int tmpf_seq[MAX_TMPF_TYPE];

Str *tmpfname(int type, const char *ext) {
  const char *dir;
  switch (type) {
  case TMPF_HIST:
    dir = rc_dir;
    break;
  case TMPF_DFL:
  case TMPF_COOKIE:
  case TMPF_SRC:
  case TMPF_FRAME:
  case TMPF_CACHE:
  default:
    dir = tmp_dir;
  }

  auto tmpf = Sprintf("%s/w3m%s%d-%d%s", dir, tmpf_base[type], CurrentPid,
                      tmpf_seq[type]++, (ext) ? ext : "");
  pushText(fileToDelete, tmpf->ptr);
  return tmpf;
}
