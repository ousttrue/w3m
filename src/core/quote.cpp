#include "quote.h"
#include "ctrlcode.h"
#include "myctype.h"
#include "entity.h"
#include <sstream>
#include <string.h>
#include <charconv>
#include <assert.h>
#ifdef _MSC_VER
#else
#include <pwd.h>
#endif

unsigned char QUOTE_MAP[0x100] = {
    /* NUL SOH STX ETX EOT ENQ ACK BEL  BS  HT  LF  VT  FF  CR  SO  SI */
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    /* DLE DC1 DC2 DC3 DC4 NAK SYN ETB CAN  EM SUB ESC  FS  GS  RS  US */
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    /* SPC   !   "   #   $   %   &   '   (   )   *   +   ,   -   .   / */
    24,
    72,
    76,
    40,
    8,
    40,
    41,
    77,
    72,
    72,
    72,
    40,
    72,
    8,
    0,
    64,
    /*   0   1   2   3   4   5   6   7   8   9   :   ;   <   =   >   ? */
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    32,
    72,
    74,
    72,
    75,
    40,
    /*   @   A   B   C   D   E   F   G   H   I   J   K   L   M   N   O */
    72,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    /*   P   Q   R   S   T   U   V   W   X   Y   Z   [   \   ]   ^   _ */
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    72,
    72,
    72,
    72,
    0,
    /*   `   a   b   c   d   e   f   g   h   i   j   k   l   m   n   o */
    72,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    /*   p   q   r   s   t   u   v   w   x   y   z   {   |   }   ~ DEL */
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    72,
    72,
    72,
    72,
    24,

    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
};

const char *HTML_QUOTE_MAP[] = {
    nullptr, "&amp;", "&lt;", "&gt;", "&quot;", "&apos;", nullptr, nullptr,
};

std::string_view remove_space(std::string_view str) {
  auto p = str.begin();
  for (; p != str.end() && IS_SPACE(*p); p++)
    ;

  auto q = p;
  for (; q != str.end(); q++)
    ;

  for (; q > p && IS_SPACE(*(q - 1)); q--)
    ;

  return {p, q};
}

void make_domain_list(std::list<std::string> &list,
                      const std::string &domain_list) {
  list.clear();
  if (domain_list.empty()) {
    return;
  }
  for (auto p = domain_list.begin(); p != domain_list.end();) {
    while (p != domain_list.end() && IS_SPACE(*p))
      p++;
    // Strclear(tmp);
    std::stringstream tmp;
    while (p != domain_list.end() && !IS_SPACE(*p) && *p != ',') {
      tmp << *p++;
    }
    auto str = tmp.str();
    if (str.size()) {
      list.push_back(str);
    }
    while (p != domain_list.end() && IS_SPACE(*p)) {
      p++;
    }
    if (p != domain_list.end() && *p == ',') {
      p++;
    }
  }
}

std::string cleanupName(std::string_view name) {
  std::string buf(name.begin(), name.end());
  auto p = buf.data();
  auto q = name.data();
  while (q != name.data() + name.size()) {
    if (strncmp(p, "/../", 4) == 0) { /* foo/bar/../FOO */
      if (p - 2 == buf && strncmp(p - 2, "..", 2) == 0) {
        /* ../../       */
        p += 3;
        q += 3;
      } else if (p - 3 >= buf && strncmp(p - 3, "/..", 3) == 0) {
        /* ../../../    */
        p += 3;
        q += 3;
      } else {
        while (p != buf && *--p != '/')
          ; /* ->foo/FOO */
        *p = '\0';
        q += 3;
        buf += q;
      }
    } else if (strcmp(p, "/..") == 0) { /* foo/bar/..   */
      if (p - 2 == buf && strncmp(p - 2, "..", 2) == 0) {
        /* ../..        */
      } else if (p - 3 >= buf && strncmp(p - 3, "/..", 3) == 0) {
        /* ../../..     */
      } else {
        while (p != buf && *--p != '/')
          ; /* ->foo/ */
        *++p = '\0';
      }
      break;
    } else if (strncmp(p, "/./", 3) == 0) { /* foo/./bar */
      *p = '\0';                            /* -> foo/bar           */
      q += 2;
      buf += q;
    } else if (strcmp(p, "/.") == 0) { /* foo/. */
      *++p = '\0';                     /* -> foo/              */
      break;
    } else if (strncmp(p, "//", 2) == 0) { /* foo//bar */
      /* -> foo/bar           */
      *p = '\0';
      q++;
      buf += q;
    } else {
      p++;
      q++;
    }
  }
  return buf;
}

std::string cleanup_line(std::string_view _s, CleanupMode mode) {
  std::string s{_s.begin(), _s.end()};
  if (s.empty()) {
    return s;
  }
  if (mode == RAW_MODE) {
    return s;
  }

  if (s.size() >= 2 && s[s.size() - 2] == '\r' && s[s.size() - 1] == '\n') {
    s.pop_back();
    s.pop_back();
    s += '\n';
  } else if (s.back() == '\r') {
    s[s.size() - 1] = '\n';
  } else if (s.back() != '\n') {
    s += '\n';
  }

  if (mode != PAGER_MODE) {
    for (size_t i = 0; i < s.size(); i++) {
      if (s[i] == '\0') {
        s[i] = ' ';
      }
    }
  }
  return s;
}

std::string expandPath(std::string_view name) {
  if (name.empty()) {
    return "";
  }

  auto p = name.begin();
  if (*p == '~') {
    p++;
    std::stringstream ss;
#ifdef _MSC_VER
    if (*p == '/' || *p == '\0') { /* ~/dir... or ~ */
      ss << getenv("USERPROFILE");
    }
#else
    if (IS_ALPHA(*p)) {
      auto q = strchr(p, '/');
      struct passwd *passent;
      if (q) { /* ~user/dir... */
        passent = getpwnam(std::string(p).substr(0, q - p).c_str());
        p = q;
      } else { /* ~user */
        passent = getpwnam(p);
        p = "";
      }
      if (!passent) {
        return {name.begin(), name.end()};
      }
      ss << passent->pw_dir;
    } else if (*p == '/' || *p == '\0') { /* ~/dir... or ~ */
      ss << getenv("HOME");
    }
#endif
    else {
      return {name.begin(), name.end()};
    }
    if (ss.str() == "/" && *p == '/') {
      p++;
    }
    ss << std::string_view{p, name.end()};
    return ss.str();
  }

  return {name.begin(), name.end()};
}

unsigned int total_dot_number(std::string_view v, unsigned int max_count) {
  unsigned int count = 0;
  for (auto c : v) {
    if (count >= max_count) {
      break;
    }
    if (c == '.') {
      count++;
    }
  }
  return count;
}

std::string html_quote(std::string_view str) {
  std::stringstream tmp;
  auto end = str.data() + str.size();
  for (auto p = str.data(); p != end; p++) {
    auto q = html_quote_char(*p);
    if (q) {
      tmp << q;
    } else {
      tmp << *p;
    }
  }
  return tmp.str();
}

std::string html_unquote(std::string_view str) {
  std::stringstream tmp;
  auto end = str.data() + str.size();
  for (auto p = str.data(); p != end;) {
    if (*p == '&') {
      auto [q, pp] = getescapecmd(p);
      p = pp.data();
      tmp << q;
    } else {
      tmp << *p;
      p++;
    }
  }
  return tmp.str();
}

std::string qstr_unquote(std::string_view s) {
  if (s.empty())
    return {};

  auto p = s.data();
  if (*p == '"') {
    std::stringstream tmp;
    for (p++; *p != '\0'; p++) {
      if (*p == '\\')
        p++;
      tmp << *p;
    }
    auto str = tmp.str();
    if (str.back() == '"')
      str.pop_back();
    return str;
  } else {
    return {s.begin(), s.end()};
  }
}

std::string mybasename(std::string_view s) {
  auto p = s.end();
  while (s.begin() <= p && *p != '/')
    p--;
  if (*p == '/')
    p++;
  else
    p = s.begin();
  return {p, s.end()};
}

int stoi(std::string_view s) {
  int value;
  auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
  assert(ec == std::errc{});
  return value;
}

std::string shell_quote(std::string_view str) {
  std::stringstream tmp;
  for (auto p = str.begin(); p != str.end(); p++) {
    if (is_shell_unsafe(*p)) {
      tmp << '\\';
      tmp << *p;
    } else {
      tmp << *p;
    }
  }
  return tmp.str();
}

std::string unescape_spaces(std::string_view s) {
  std::stringstream tmp;
  for (auto p = s.begin(); p != s.end(); p++) {
    if (*p == '\\' && (*(p + 1) == ' ' || *(p + 1) == CTRL_I)) {
      // ?
      tmp << *p;
    } else {
      tmp << *p;
    }
  }
  return tmp.str();
}

std::string_view Strremovefirstspaces(std::string_view s) {
  auto p = s.begin();
  for (; p != s.end() && IS_SPACE(*p); p++)
    ;
  return {p, s.end()};
}

std::string_view Strremovetrailingspaces(std::string_view s) {
  if (s.empty()) {
    return s;
  }

  auto p = s.begin() + s.size() - 1;
  for (; p != s.begin() && IS_SPACE(*p); p--)
    ;
  return {s.begin(), p};
}

void Strshrink(std::string &s, int n) {
  for (int i = 0; i < n && s.size(); ++i) {
    s.pop_back();
  }
}

void Strlower(std::string &s) {
  for (auto &c : s) {
    c = TOLOWER(c);
  }
}

void Strupper(std::string &s) {
  for (auto &c : s) {
    c = TOUPPER(c);
  }
}

void Strchop(std::string &s) {
  while (s.size() > 0 && (s.back() == '\n' || s.back() == '\r')) {
    s.pop_back();
  }
}

static char roman_num1[] = {
    'i', 'x', 'c', 'm', '*',
};
static char roman_num5[] = {
    'v',
    'l',
    'd',
    '*',
};

static std::string romanNum2(int l, int n) {
  std::stringstream s;

  switch (n) {
  case 1:
  case 2:
  case 3:
    for (; n > 0; n--)
      s << roman_num1[l];
    break;
  case 4:
    s << roman_num1[l];
    s << roman_num5[l];
    break;
  case 5:
  case 6:
  case 7:
  case 8:
    s << roman_num5[l];
    for (n -= 5; n > 0; n--)
      s << roman_num1[l];
    break;
  case 9:
    s << roman_num1[l];
    s << roman_num1[l + 1];
    break;
  }
  return s.str();
}

std::string romanNumeral(int n) {
  if (n <= 0) {
    return {};
  }

  if (n >= 4000) {
    return "**";
  }

  std::stringstream r;
  r << romanNum2(3, n / 1000);
  r << romanNum2(2, (n % 1000) / 100);
  r << romanNum2(1, (n % 100) / 10);
  r << romanNum2(0, n % 10);

  return r.str();
}

std::string romanAlphabet(int n) {
  if (n <= 0) {
    return {};
  }

  char buf[14];
  int l = 0;
  while (n) {
    buf[l++] = 'a' + (n - 1) % 26;
    n = (n - 1) / 26;
  }

  l--;
  std::stringstream r;
  for (; l >= 0; l--)
    r << buf[l];

  return r.str();
}


