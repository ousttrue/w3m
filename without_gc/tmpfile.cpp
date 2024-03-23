#include "tmpfile.h"
#include <sstream>
#include <iostream>
#include <assert.h>

static const char *tmpf_base[MAX_TMPF_TYPE] = {
    "tmp", "src", "frame", "cache", "cookie", "hist",
};

TmpFile::~TmpFile() {
  for (auto &f : _fileToDelete) {
    std::cout << "fileToDelete: " << f << std::endl;
    unlink(f.c_str());
  }
  _fileToDelete.clear();
}

std::string TmpFile::tmpfname(TmpfType type, std::string_view ext) {
  assert(_rc_dir.size());
  assert(_pid);

  std::stringstream ss;
  unsigned int tmpf_seq[MAX_TMPF_TYPE];
  ss << _rc_dir << "/w3m" << tmpf_base[type] << _pid << tmpf_seq[type]++ << ext;
  auto tmpf = ss.str();
  _fileToDelete.push_back(tmpf);
  return tmpf;
}
