#pragma once
#include <string>
#include <string_view>
#include <list>

enum TmpfType {
  TMPF_DFL = 0,
  TMPF_SRC = 1,
  TMPF_FRAME = 2,
  TMPF_CACHE = 3,
  TMPF_COOKIE = 4,
  TMPF_HIST = 5,
  MAX_TMPF_TYPE = 6,
};

class TmpFile {

  std::string _rc_dir;
  int _pid = 0;

  std::list<std::string> _fileToDelete;

  TmpFile() {}

public:
  TmpFile(const TmpFile &) = delete;
  TmpFile &operator=(const TmpFile &) = delete;
  ~TmpFile();
  static TmpFile &instance() {
    static TmpFile s_instance;
    return s_instance;
  }
  void initialize(const std::string &rc_dir, int pid) {
    _rc_dir = rc_dir;
    _pid = pid;
  }
  std::string tmpfname(TmpfType type, std::string_view ext);
};
