#include "tmpfile.h"
#include "rc.h"
#include "w3m.h"
#include "app.h"
#include <sstream>

static const char *tmpf_base[MAX_TMPF_TYPE] = {
    "tmp", "src", "frame", "cache", "cookie", "hist",
};
static unsigned int tmpf_seq[MAX_TMPF_TYPE];

std::string tmpfname(TmpfType type, const std::string &ext) {
  std::stringstream ss;
  ss << rc_dir << "/w3m" << tmpf_base[type] << App::instance().pid()
     << tmpf_seq[type]++ << ext;
  auto tmpf = ss.str();
  fileToDelete.push_back(tmpf);
  return tmpf;
}
