#include "tmpfile.h"
#include "rc.h"
#include "w3m.h"
#include "textlist.h"
#include <sstream>

const char *tmp_dir = {};

static const char *tmpf_base[MAX_TMPF_TYPE] = {
    "tmp", "src", "frame", "cache", "cookie", "hist",
};
static unsigned int tmpf_seq[MAX_TMPF_TYPE];

std::string tmpfname(TmpfType type, const std::string &ext) {
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

  std::stringstream ss;
  ss << dir << "/w3m" << tmpf_base[type] << CurrentPid << tmpf_seq[type]++
     << ext;
  auto tmpf = ss.str();
  fileToDelete.push_back(tmpf);
  return tmpf;
}
