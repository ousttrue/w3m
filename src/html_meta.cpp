#include "html_meta.h"
#include "cmp.h"
#include "myctype.h"

MetaRefreshInfo getMetaRefreshParam(const std::string &_q) {
  if (_q.empty()) {
    return {};
  }

  auto refresh_interval = stoi(_q);
  if (refresh_interval < 0) {
    return {};
  }

  std::string s_tmp;
  auto q = _q.data();
  while (q != _q.data() + _q.size()) {
    if (!strncasecmp(&*q, "url=", 4)) {
      q += 4;
      if (*q == '\"' || *q == '\'') /* " or ' */
        q++;
      auto r = q;
      while (*r && !IS_SPACE(*r) && *r != ';')
        r++;

      s_tmp = std::string(q, r - q);

      if (s_tmp.size() > 0 && (s_tmp.back() == '\"' ||  /* " */
                               s_tmp.back() == '\'')) { /* ' */
        s_tmp.pop_back();
      }
      q = r;
    }
    while (*q && *q != ';')
      q++;
    if (*q == ';')
      q++;
    while (*q && *q == ' ')
      q++;
  }

  return {
      .interval = refresh_interval,
      .url = s_tmp,
  };
}
