#include "mailcap.h"
#include "matchattr.h"
#include "etc.h"
#include "url.h"
#include "textlist.h"
#include "myctype.h"
#include "local_cgi.h"
#include "hash.h"
#include "indep.h"
#include "proto.h"
#include <string.h>
#include <cstdlib>
#include <stdio.h>
#include <vector>
#include <string>

#define DEF_AUDIO_PLAYER "showaudio"
#define DEF_IMAGE_VIEWER "display"

struct Mailcap {
  std::vector<MailcapEntry> entries;

  void load(FILE *f) {
    Str *tmp;
    while (tmp = Strfgets(f), tmp->length > 0) {
      if (tmp->ptr[0] == '#')
        continue;
    redo:
      while (IS_SPACE(Strlastchar(tmp)))
        Strshrink(tmp, 1);
      if (Strlastchar(tmp) == '\\') {
        /* continuation */
        Strshrink(tmp, 1);
        Strcat(tmp, Strfgets(f));
        goto redo;
      }

      entries.push_back({});
      if (!entries.back().extract(tmp->ptr)) {
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
        if (entry.test) {
          Str *command = unquote_mailcap(entry.test, type, NULL, NULL, NULL);
          if (system(command->ptr) != 0)
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
    .entries = {
        {"image/*", DEF_IMAGE_VIEWER " %s", {}, NULL, NULL, NULL}, /* */
        {"audio/basic", DEF_AUDIO_PLAYER " %s", {}, NULL, NULL, NULL},
    }};

static TextList *mailcap_list;

static struct std::vector<Mailcap> UserMailcap;

#ifndef RC_DIR
#define RC_DIR "~/.w3m"
#endif
#ifndef CONF_DIR
#define CONF_DIR RC_DIR
#endif
#define USER_MAILCAP RC_DIR "/mailcap"
#define SYS_MAILCAP CONF_DIR "/mailcap"
const char *mailcap_files = USER_MAILCAP ", " SYS_MAILCAP;

int MailcapEntry::match(const char *type) const {
  auto cap = this->type;
  auto p = cap;
  for (; *p != '/'; p++) {
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

static int matchMailcapAttr(const char *p, const char *attr, size_t len,
                            Str **value) {
  int quoted;
  const char *q = NULL;

  if (strncasecmp(p, attr, len) == 0) {
    p += len;
    SKIP_BLANKS(p);
    if (value) {
      *value = Strnew();
      if (*p == '=') {
        p++;
        SKIP_BLANKS(p);
        quoted = 0;
        while (*p && (quoted || *p != ';')) {
          if (quoted || !IS_SPACE(*p))
            q = p;
          if (quoted)
            quoted = 0;
          else if (*p == '\\')
            quoted = 1;
          Strcat_char(*value, *p);
          p++;
        }
        if (q)
          Strshrink(*value, p - q - 1);
      }
      return 1;
    } else {
      if (*p == '\0' || *p == ';') {
        return 1;
      }
    }
  }
  return 0;
}

bool MailcapEntry::extract(const char *mcap_entry) {
  auto p = mcap_entry;
  SKIP_BLANKS(p);
  int k = -1;
  int j = 0;
  for (; p[j] && p[j] != ';'; j++) {
    if (!IS_SPACE(p[j]))
      k = j;
  }

  this->type = allocStr(p, (k >= 0) ? k + 1 : j);
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
  this->viewer = allocStr(p, (k >= 0) ? k + 1 : j);
  p += j;

  Str *tmp;
  while (*p == ';') {
    p++;
    SKIP_BLANKS(p);
    if (matchMailcapAttr(p, "needsterminal", 13, NULL)) {
      this->flags |= MAILCAP_NEEDSTERMINAL;
    } else if (matchMailcapAttr(p, "copiousoutput", 13, NULL)) {
      this->flags |= MAILCAP_COPIOUSOUTPUT;
    } else if (matchMailcapAttr(p, "x-htmloutput", 12, NULL) ||
               matchMailcapAttr(p, "htmloutput", 10, NULL)) {
      this->flags |= MAILCAP_HTMLOUTPUT;
    } else if (matchMailcapAttr(p, "test", 4, &tmp)) {
      this->test = allocStr(tmp->ptr, tmp->length);
    } else if (matchMailcapAttr(p, "nametemplate", 12, &tmp)) {
      this->nametemplate = allocStr(tmp->ptr, tmp->length);
    } else if (matchMailcapAttr(p, "edit", 4, &tmp)) {
      this->edit = allocStr(tmp->ptr, tmp->length);
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

void initMailcap(void) {

  if (non_null(mailcap_files))
    mailcap_list = make_domain_list(mailcap_files);
  else
    mailcap_list = NULL;
  if (mailcap_list == NULL)
    return;

  // UserMailcap =
  //     (struct MailcapEntry **)New_N(struct MailcapEntry *,
  //     mailcap_list->nitem);
  for (auto tl = mailcap_list->first; tl; tl = tl->next) {
    if (auto f = fopen(expandPath(tl->ptr), "r")) {
      UserMailcap.push_back({});
      UserMailcap.back().load(f);
      fclose(f);
    }
  }
}

char *acceptableMimeTypes(void) {
  static Str *types = NULL;
  TextList *l;
  Hash_si *mhash;
  const char *p;

  if (types != NULL)
    return types->ptr;

  /* generate acceptable media types */
  l = newTextList();
  mhash = newHash_si(16); /* XXX */
  /* pushText(l, "text"); */
  putHash_si(mhash, "text", 1);
  pushText(l, "image");
  putHash_si(mhash, "image", 1);
  for (int i = 0; i < mailcap_list->nitem; i++) {
    auto &mailcap = UserMailcap[i];
    // if (mp == NULL)
    //   continue;
    for (auto &e : mailcap.entries) {
      p = strchr(e.type, '/');
      if (p == NULL)
        continue;
      auto mt = allocStr(e.type, p - e.type);
      if (getHash_si(mhash, mt, 0) == 0) {
        pushText(l, mt);
        putHash_si(mhash, mt, 1);
      }
    }
  }
  types = Strnew();
  Strcat_charp(types, "text/html, text/*;q=0.5");
  while ((p = popText(l)) != NULL) {
    Strcat_charp(types, ", ");
    Strcat_charp(types, p);
    Strcat_charp(types, "/*");
  }
  return types->ptr;
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

static std::string quote_mailcap(const char *s, int flag) {
  std::string d;
  for (;; ++s)
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

static Str *unquote_mailcap_loop(const char *qstr, const char *type,
                                 const char *name, const char *attr,
                                 MailcapStat *mc_stat, MCF_QuoteFlags flag0) {
  Str *str, *tmp, *test, *then;
  const char *p;
  MC_UnquoteStatus status = MC_NORMAL, prev_status = MC_NORMAL;
  MCF_QuoteFlags flag;
  int sp = 0;

  if (mc_stat) {
    *mc_stat = {};
  }

  if (qstr == NULL)
    return NULL;

  str = Strnew();
  tmp = test = then = NULL;

  for (flag = flag0, p = qstr; *p; p++) {
    if (status == MC_QUOTED) {
      if (prev_status == MC_PREC2)
        Strcat_char(tmp, *p);
      else
        Strcat_char(str, *p);
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
        Strcat_char(str, *p);
      }
      break;
    case MC_PREC:
      if (IS_ALPHA(*p)) {
        switch (*p) {
        case 's':
          if (name) {
            Strcat(str, quote_mailcap(name, flag));
            if (mc_stat)
              *mc_stat |= MCSTAT_REPNAME;
          }
          break;
        case 't':
          if (type) {
            Strcat(str, quote_mailcap(type, flag));
            if (mc_stat)
              *mc_stat |= MCSTAT_REPTYPE;
          }
          break;
        }
        status = MC_NORMAL;
      } else if (*p == '{') {
        status = MC_PREC2;
        test = then = NULL;
        tmp = Strnew();
      } else if (*p == '%') {
        Strcat_char(str, *p);
      }
      break;
    case MC_PREC2:
      if (sp > 0 || *p == '{') {
        Strcat_char(tmp, *p);

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
        if (attr && (q = strcasestr(attr, tmp->ptr)) != NULL &&
            (q == attr || IS_SPACE(*(q - 1)) || *(q - 1) == ';') &&
            matchattr(q, tmp->ptr, tmp->length, &tmp)) {
          Strcat(str, quote_mailcap(tmp->ptr, flag));
          if (mc_stat)
            *mc_stat |= MCSTAT_REPPARAM;
        }
        status = MC_NORMAL;
      } else {
        Strcat_char(tmp, *p);
      }
      break;

    default:
      break;
    }
  }
  return str;
}

Str *unquote_mailcap(const char *qstr, const char *type, const char *name,
                     const char *attr, MailcapStat *mc_stat) {
  return unquote_mailcap_loop(qstr, type, name, attr, mc_stat, {});
}
