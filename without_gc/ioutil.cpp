#include "ioutil.h"
#include "quote.h"
#include "url_decode.h"
#include <sstream>
#ifdef _MSC_VER
#include <winsock2.h>
#include <direct.h>
// #define getcwd _getcwd
#endif
// HOST_NAME_MAX is recommended by POSIX, but not required.
// FreeBSD and OSX (as of 10.9) are known to not define it.
// 255 is generally the safe value to assume and upstream
// PHP does this as well.
#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 255
#endif

namespace ioutil {

std::string personal_document_root;
std::string _currentDir;
std::string _hostName = "localhost";
std::string editor = "/usr/bin/vim";

void initialize() {
#ifdef MAXPATHLEN
  {
    char buf[MAXPATHLEN];
    getcwd(buf, MAXPATHLEN);
    _currentDir = buf;
  }
#else
  _currentDir = getcwd(nullptr, 0);
#endif

  if (editor.empty()) {
    if (auto p = getenv("EDITOR")) {
      editor = p;
    }
  }

  {
    char hostname[HOST_NAME_MAX + 2];
    if (gethostname(hostname, HOST_NAME_MAX + 2) == 0) {
      // Don't use hostname if it is truncated.
      hostname[HOST_NAME_MAX + 1] = '\0';
      auto hostname_len = strlen(hostname);
      if (hostname_len <= HOST_NAME_MAX) {
        _hostName = hostname;
      }
    }
  }
}

std::string myEditor(std::string_view file, int line) {
  std::stringstream tmp;
  bool set_file = false;
  bool set_line = false;
  for (auto p = editor.begin(); *p; p++) {
    if (*p == '%' && *(p + 1) == 's' && !set_file) {
      tmp << file;
      set_file = true;
      p++;
    } else if (*p == '%' && *(p + 1) == 'd' && !set_line && line > 0) {
      tmp << line;
      set_line = true;
      p++;
    } else {
      tmp << *p;
    }
  }
  if (!set_file) {
    if (!set_line && line > 1 && strcasestr(editor.c_str(), "vi")) {
      tmp << " +" << line;
    }
    tmp << " " << file;
  }
  return tmp.str();
}
std::string pwd() { return _currentDir; }

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

std::string expandName(std::string_view name) {
#ifdef _MSC_VER
  return {};
#else
  struct passwd *passent, *getpwnam(const char *);
  Str *extpath = NULL;

  if (name.empty()) {
    return {};
  }
  auto p = name.begin();
  if (*p == '/') {
    if ((*(p + 1) == '~' && IS_ALPHA(*(p + 2))) && personal_document_root) {
      p += 2;
      auto q = strchr(p, '/');
      if (q) { /* /~user/dir... */
        passent = getpwnam(allocStr(p, q - p));
        p = q;
      } else { /* /~user */
        passent = getpwnam(p);
        p = "";
      }
      if (!passent)
        goto rest;
      extpath =
          Strnew_m_charp(passent->pw_dir, "/", personal_document_root, NULL);
      if (*personal_document_root == '\0' && *p == '/')
        p++;
    } else
      goto rest;
    if (Strcmp_charp(extpath, "/") == 0 && *p == '/')
      p++;
    Strcat_charp(extpath, p);
    return extpath->ptr;
  } else
    return expandPath(p);
rest:
  return {name.begin(), name.end()};
#endif
}
std::string hostname() { return _hostName; }

bool is_localhost(std::string_view host) {
  if (host.empty()) {
    return true;
  }
  if (host == "localhost") {
    return true;
  }
  if (host == "127.0.0.1") {
    return true;
  }
  if (host == "[::1]") {
    return true;
  }
  if (host == _hostName) {
    return true;
  }
  return false;
}

} // namespace ioutil
