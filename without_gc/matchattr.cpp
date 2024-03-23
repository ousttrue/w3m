#include "matchattr.h"
#include "quote.h"
#include "myctype.h"
#include <sstream>

// get: ^attr\S*=\S*value;
// get: ^attr\S*=\S*"value";
std::optional<std::string> matchattr(std::string_view _p,
                                     std::string_view attr) {
  auto len = attr.size();
  if (strcasecmp(_p.substr(0, attr.size()), attr) == 0) {
    _p = _p.substr(len);
    while (_p.size() && IS_SPACE(_p[0])) {
      _p = _p.substr(1);
    }

    auto p = _p.data();

    std::stringstream value;
    const char *q = {};
    if (*p == '=') {
      p++;
      SKIP_BLANKS(p);
      bool quoted = {};
      while (!IS_ENDL(*p) && (quoted || *p != ';')) {
        if (!IS_SPACE(*p))
          q = p;
        if (*p == '"')
          quoted = (quoted) ? 0 : 1;
        else
          value << *p;
        p++;
      }
    }

    auto ret = value.str();
    if (q) {
      for (int i = 0; i < p - q - 1; ++i) {
        ret.pop_back();
      }
    }
    return ret;
  }
  return {};
}
