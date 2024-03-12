#include "url_decode.h"
#include "quote.h"
#include "Str.h"
#include "myctype.h"
#include <sstream>

bool DecodeURL = false;

static char url_unquote_char(const char **pstr) {
  return ((IS_XDIGIT((*(pstr))[1]) && IS_XDIGIT((*(pstr))[2]))
              ? (*(pstr) += 3, (GET_MYCDIGIT((*(pstr))[-2]) << 4) |
                                   GET_MYCDIGIT((*(pstr))[-1]))
              : -1);
}

std::string file_quote(std::string_view str) {
  std::stringstream tmp;
  for (auto p = str.begin(); p != str.end(); ++p) {
    if (is_file_quote(*p)) {
      char buf[4];
      snprintf(buf, sizeof(buf), "%%%02X", (unsigned char)*p);
      tmp << buf;
    } else {
      tmp << *p;
    }
  }
  return tmp.str();
}

std::string file_unquote(std::string_view str) {
  std::stringstream tmp;
  for (auto p = str.data(); p != str.data() + str.size();) {
    if (*p == '%') {
      auto q = &*p;
      int c = url_unquote_char(&q);
      if (c >= 0) {
        if (c != '\0' && c != '\n' && c != '\r') {
          tmp << (char)c;
        }
        p = q;
        continue;
      }
    }
    tmp << *p;
    p++;
  }
  return tmp.str();
}

Str *Str_url_unquote(Str *x, int is_form, int safe) {
  Str *tmp = nullptr;
  const char *p = x->ptr, *ep = x->ptr + x->length, *q;
  int c;

  for (; p < ep;) {
    if (is_form && *p == '+') {
      if (tmp == nullptr)
        tmp = Strnew_charp_n(x->ptr, (int)(p - x->ptr));
      Strcat_char(tmp, ' ');
      p++;
      continue;
    } else if (*p == '%') {
      q = p;
      c = url_unquote_char(&q);
      if (c >= 0 && (!safe || !IS_ASCII(c) || !is_file_quote(c))) {
        if (tmp == NULL)
          tmp = Strnew_charp_n(x->ptr, (int)(p - x->ptr));
        Strcat_char(tmp, (char)c);
        p = q;
        continue;
      }
    }
    if (tmp)
      Strcat_char(tmp, *p);
    p++;
  }
  if (tmp)
    return tmp;
  return x;
}

static const char *url_unquote_conv0(const char *url) {
  auto tmp = Str_url_unquote(Strnew_charp(url), false, true);
  return tmp->ptr;
}

const char *url_decode0(const char *url) {
  if (!DecodeURL)
    return url;
  return url_unquote_conv0(url);
}
