#pragma once

#define MAILCAP_NEEDSTERMINAL 0x01
#define MAILCAP_COPIOUSOUTPUT 0x02
#define MAILCAP_HTMLOUTPUT 0x04

#define MCSTAT_REPNAME 0x01
#define MCSTAT_REPTYPE 0x02
#define MCSTAT_REPPARAM 0x04

struct mailcap {
  const char *type;
  const char *viewer;
  int flags;
  char *test;
  char *nametemplate;
  char *edit;
};

struct Str;
Str *unquote_mailcap(const char *qstr, const char *type, const char *name,
                     const char *attr, int *mc_stat);
struct mailcap *searchExtViewer(const char *type);
struct mailcap *searchMailcap(struct mailcap *table, const char *type);

inline int is_dump_text_type(const char *type) {
  struct mailcap *mcap;
  return (type && (mcap = searchExtViewer(type)) &&
          (mcap->flags & (MAILCAP_HTMLOUTPUT | MAILCAP_COPIOUSOUTPUT)));
}
