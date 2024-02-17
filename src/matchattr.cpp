#include "matchattr.h"
#include "quote.h"
#include "myctype.h"
#include "Str.h"

// get: ^attr\S*=\S*value;
// get: ^attr\S*=\S*"value";
bool matchattr(const char *p, const char *attr, int len, Str **value) {
  const char *q = {};
  if (strncasecmp(p, attr, len) == 0) {
    p += len;
    SKIP_BLANKS(p);
    if (value) {
      *value = Strnew();
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
            Strcat_char(*value, *p);
          p++;
        }
        if (q)
          Strshrink(*value, p - q - 1);
      }
      return 1;
    } else {
      if (IS_ENDT(*p)) {
        return 1;
      }
    }
  }
  return 0;
}
