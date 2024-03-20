#pragma once
#include <string_view>
#include <optional>
#include <assert.h>

class stringtoken {
  std::string_view _src;
  size_t _pos = 0;

public:
  stringtoken(std::string_view s) : _src(s) {}
  size_t pos() const { return _pos; }
  bool is_end() const { return _pos >= _src.size(); }
  char current() const {
    assert(!is_end());
    return _src[_pos];
  }
  const char *ptr() const {
    // assert(!is_end());
    return _src.data() + _pos;
  }
  void next() {
    assert(!is_end());
    ++_pos;
  }

  // get next tag
  std::optional<std::string_view> sloppy_parse_line();
};
