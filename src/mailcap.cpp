#include "mailcap.h"
#include "quote.h"
#include "matchattr.h"
#include "cmp.h"
#include "myctype.h"

#include <string.h>
#include <cstdlib>
#include <stdio.h>
#include <vector>
#include <string>
#include <list>
#include <unordered_map>
#include <sstream>
#include <fstream>

#define DEF_AUDIO_PLAYER "showaudio"
#define DEF_IMAGE_VIEWER "display"

struct Mailcap {
  std::vector<MailcapEntry> entries;

  void load(const std::string &file) {
    std::ifstream ifs(file);
    if (!ifs) {
      return;
    }

    std::string tmp;
    while (std::getline(ifs, tmp)) {
      if (tmp.empty()) {
        continue;
      }
      if (tmp[0] == '#') {
        continue;
      }
    redo:
      while (IS_SPACE(tmp.back()))
        tmp.pop_back();
      if (tmp.back() == '\\') {
        /* continuation */
        tmp.pop_back();
        std::string tmp2;
        std::getline(ifs, tmp2);
        tmp += tmp2;
        goto redo;
      }

      entries.push_back({});
      if (!entries.back().extract(tmp)) {
        entries.pop_back();
      }
    }
  }

  MailcapEntry *search(const char *type) const {
    int level = 0;
    struct MailcapEntry *mcap = NULL;
    for (auto entry : this->entries) {
      int i = entry.match(type);
      if (i > level) {
        if (entry.test.size()) {
          auto command = unquote_mailcap(entry.test, type, NULL, NULL, NULL);
          if (system(command.c_str()) != 0)
            continue;
        }
        level = i;
        mcap = &entry;
      }
    }
    return mcap;
  }
};

static Mailcap DefaultMailcap = {
    .entries =
        {
            {"image/*", DEF_IMAGE_VIEWER " %s"},
            {"audio/basic", DEF_AUDIO_PLAYER " %s"},
        },
};

static std::list<std::string> mailcap_list;

static std::vector<Mailcap> UserMailcap;

#ifndef RC_DIR
#define RC_DIR "~/.w3m"
#endif
#ifndef CONF_DIR
#define CONF_DIR RC_DIR
#endif
#define USER_MAILCAP RC_DIR "/mailcap"
#define SYS_MAILCAP CONF_DIR "/mailcap"
std::string mailcap_files = USER_MAILCAP ", " SYS_MAILCAP;

int MailcapEntry::match(const char *type) const {
  auto cap = this->type.begin();
  auto p = cap;
  for (; p != this->type.end() && *p != '/'; p++) {
    if (TOLOWER(*p) != TOLOWER(*type))
      return 0;
    type++;
  }
  if (*type != '/')
    return 0;
  p++;
  type++;

  int level;
  using enum MailcapFlags;
  if (this->flags & MAILCAP_HTMLOUTPUT)
    level = 1;
  else
    level = 0;
  if (*p == '*')
    return 10 + level;
  while (*p) {
    if (TOLOWER(*p) != TOLOWER(*type))
      return 0;
    p++;
    type++;
  }
  if (*type != '\0')
    return 0;
  return 20 + level;
}

static std::optional<std::string> matchMailcapAttr(const char *p,
                                                   std::string_view attr) {
  if (strncasecmp(p, attr.data(), attr.size()) != 0) {
    return {};
  }

  p += attr.size();
  SKIP_BLANKS(p);

  std::string value;
  if (*p == '=') {
    p++;
    SKIP_BLANKS(p);
    bool quoted = 0;
    const char *q = nullptr;
    while (*p && (quoted || *p != ';')) {
      if (quoted || !IS_SPACE(*p))
        q = p;
      if (quoted)
        quoted = 0;
      else if (*p == '\\')
        quoted = 1;
      value += *p;
      p++;
    }
    if (q) {
      for (int i = 0; i < p - q - 1; ++i) {
        value.pop_back();
      }
    }
  }
  return value;
}

bool MailcapEntry::extract(const std::string &mcap_entry) {
  auto p = mcap_entry.data();
  SKIP_BLANKS(p);
  int k = -1;
  int j = 0;
  for (; p[j] && p[j] != ';'; j++) {
    if (!IS_SPACE(p[j]))
      k = j;
  }

  this->type = std::string(p, (k >= 0) ? k + 1 : j);
  if (!p[j])
    return 0;
  p += j + 1;

  SKIP_BLANKS(p);
  k = -1;
  int quoted = 0;
  for (j = 0; p[j] && (quoted || p[j] != ';'); j++) {
    if (quoted || !IS_SPACE(p[j]))
      k = j;
    if (quoted)
      quoted = 0;
    else if (p[j] == '\\')
      quoted = 1;
  }
  this->viewer = std::string(p, (k >= 0) ? k + 1 : j);
  p += j;

  while (*p == ';') {
    p++;
    SKIP_BLANKS(p);
    if (auto value = matchMailcapAttr(p, "needsterminal")) {
      this->flags |= MAILCAP_NEEDSTERMINAL;
    } else if (auto value = matchMailcapAttr(p, "copiousoutput")) {
      this->flags |= MAILCAP_COPIOUSOUTPUT;
    } else if (auto value = matchMailcapAttr(p, "x-htmloutput")) {
      this->flags |= MAILCAP_HTMLOUTPUT;
    } else if (auto value = matchMailcapAttr(p, "htmloutput")) {
      this->flags |= MAILCAP_HTMLOUTPUT;
    } else if (auto value = matchMailcapAttr(p, "test")) {
      this->test = *value;
    } else if (auto value = matchMailcapAttr(p, "nametemplate")) {
      this->nametemplate = *value;
    } else if (auto value = matchMailcapAttr(p, "edit")) {
      this->edit = *value;
    }
    quoted = 0;
    while (*p && (quoted || *p != ';')) {
      if (quoted)
        quoted = 0;
      else if (*p == '\\')
        quoted = 1;
      p++;
    }
  }
  return true;
}

void initMailcap() {
  if (non_null(mailcap_files)) {
    make_domain_list(mailcap_list, mailcap_files);
  }
  if (mailcap_list.empty()) {
    return;
  }

  // UserMailcap =
  //     (struct MailcapEntry **)New_N(struct MailcapEntry *,
  //     mailcap_list->nitem);
  for (auto &tl : mailcap_list) {
    UserMailcap.push_back({});
    UserMailcap.back().load(expandPath(tl));
  }
}

std::string acceptableMimeTypes(void) {
  static std::string types;
  if (types.empty()) {
    /* generate acceptable media types */
    std::list<std::string> l;
    std::unordered_map<std::string, int> mhash; /* XXX */
    /* pushText(l, "text"); */
    mhash.insert({"text", 1});
    l.push_back("image");
    mhash.insert({"image", 1});
    for (size_t i = 0; i < mailcap_list.size(); i++) {
      auto &mailcap = UserMailcap[i];
      // if (mp == NULL)
      //   continue;
      for (auto &e : mailcap.entries) {
        auto p = strchr(e.type.c_str(), '/');
        if (!p)
          continue;
        std::string mt(e.type.c_str(), p - e.type.c_str());
        if (mhash.find(mt) == mhash.end()) {
          l.push_back(mt);
          mhash.insert({mt, 1});
        }
      }
    }

    types += "text/html, text/*;q=0.5";
    while (l.size()) {
      auto p = l.back();
      l.pop_back();
      types += ", ";
      types += p;
      types += "/*";
    }
  }
  return types;
}

struct MailcapEntry *searchExtViewer(const char *type) {
  for (auto &mailcap : UserMailcap) {
    if (auto p = mailcap.search(type)) {
      return p;
    }
  }
  return DefaultMailcap.search(type);
}

enum MC_UnquoteStatus {
  MC_NORMAL = 0,
  MC_PREC = 1,
  MC_PREC2 = 2,
  MC_QUOTED = 3,
};

enum MCF_QuoteFlags {
  MCF_SQUOTED = (1 << 0),
  MCF_DQUOTED = (1 << 1),
};
ENUM_OP_INSTANCE(MCF_QuoteFlags);

static std::string quote_mailcap(const std::string &_s, int flag) {
  std::string d;
  for (auto s = _s.begin(); s != _s.end(); ++s)
    switch (*s) {
    case '\0':
      goto end;
    case '$':
    case '`':
    case '"':
    case '\\':
      if (!(flag & MCF_SQUOTED))
        d.push_back('\\');

      d.push_back(*s);
      break;
    case '\'':
      if (flag & MCF_SQUOTED) {
        d += "'\\''";
        break;
      }
    default:
      if (!flag && !IS_ALNUM(*s))
        d.push_back('\\');
    case '_':
    case '.':
    case ':':
    case '/':
      d.push_back(*s);
      break;
    }
end:
  return d;
}

static std::string
unquote_mailcap_loop(const std::string &qstr, const std::string &type,
                     const std::string &name, const std::string &attr,
                     MailcapStat *mc_stat, MCF_QuoteFlags flag0) {

  if (mc_stat) {
    *mc_stat = {};
  }

  if (qstr.empty()) {
    return {};
  }

  std::stringstream str;
  std::string tmp;
  MCF_QuoteFlags flag = flag0;
  MC_UnquoteStatus status = MC_NORMAL;
  MC_UnquoteStatus prev_status = MC_NORMAL;
  int sp = 0;
  for (auto p = qstr.begin(); p != qstr.end(); p++) {
    if (status == MC_QUOTED) {
      if (prev_status == MC_PREC2)
        tmp += *p;
      else
        str << *p;
      status = prev_status;
      continue;
    } else if (*p == '\\') {
      prev_status = status;
      status = MC_QUOTED;
      continue;
    }
    switch (status) {
    case MC_NORMAL:
      if (*p == '%') {
        status = MC_PREC;
      } else {
        if (*p == '\'') {
          if (!flag0 && flag & MCF_SQUOTED)
            flag &= ~MCF_SQUOTED;
          else if (!flag)
            flag |= MCF_SQUOTED;
        } else if (*p == '"') {
          if (!flag0 && flag & MCF_DQUOTED)
            flag &= ~MCF_DQUOTED;
          else if (!flag)
            flag |= MCF_DQUOTED;
        }
        str << *p;
      }
      break;
    case MC_PREC:
      if (IS_ALPHA(*p)) {
        switch (*p) {
        case 's':
          if (name.size()) {
            str << quote_mailcap(name, flag);
            if (mc_stat)
              *mc_stat |= MCSTAT_REPNAME;
          }
          break;
        case 't':
          if (type.size()) {
            str << quote_mailcap(type, flag);
            if (mc_stat)
              *mc_stat |= MCSTAT_REPTYPE;
          }
          break;
        }
        status = MC_NORMAL;
      } else if (*p == '{') {
        status = MC_PREC2;
        tmp = "";
      } else if (*p == '%') {
        str << *p;
      }
      break;
    case MC_PREC2:
      if (sp > 0 || *p == '{') {
        tmp += *p;

        switch (*p) {
        case '{':
          ++sp;
          break;
        case '}':
          --sp;
          break;
        default:
          break;
        }
      } else if (*p == '}') {
        const char *q;
        std::optional<std::string> _tmp;
        if (attr.size() &&
            (q = strcasestr(attr.c_str(), tmp.c_str())) != NULL &&
            (q == attr || IS_SPACE(*(q - 1)) || *(q - 1) == ';') &&
            (_tmp = matchattr(q, tmp))) {
          tmp = *_tmp;
          str << quote_mailcap(tmp.c_str(), flag);
          if (mc_stat)
            *mc_stat |= MCSTAT_REPPARAM;
        }
        status = MC_NORMAL;
      } else {
        tmp += *p;
      }
      break;

    default:
      break;
    }
  }
  return str.str();
}

std::string unquote_mailcap(const std::string &qstr, const std::string &type,
                            const std::string &name, const std::string &attr,
                            MailcapStat *mc_stat) {
  return unquote_mailcap_loop(qstr, type, name, attr, mc_stat, {});
}
