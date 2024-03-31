#include "url_quote.h"
#include "quote.h"
#include <sstream>

static auto xdigit = "0123456789ABCDEF";

std::string url_quote(std::string_view str) {
  std::stringstream tmp;
  for (auto p : str) {
    if (is_url_quote(p)) {
      tmp << '%';
      tmp << xdigit[((unsigned char)p >> 4) & 0xF];
      tmp << xdigit[(unsigned char)p & 0xF];
    } else {
      tmp << p;
    }
  }
  return tmp.str();
}
