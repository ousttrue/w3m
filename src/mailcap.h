#pragma once
#include "enum_template.h"

extern const char *mailcap_files;

enum MailcapFlags  {
  MAILCAP_NEEDSTERMINAL = 0x01,
  MAILCAP_COPIOUSOUTPUT = 0x02,
  MAILCAP_HTMLOUTPUT = 0x04,
};
ENUM_OP_INSTANCE(MailcapFlags);

enum MailcapStat {
  MCSTAT_REPNAME = 0x01,
  MCSTAT_REPTYPE = 0x02,
  MCSTAT_REPPARAM = 0x04,
};
// ENUM_OP_INSTANCE(MailcapStat);

struct MailcapEntry {
  const char *type = 0;
  const char *viewer = 0;
  MailcapFlags flags = {};
  char *test = 0;
  char *nametemplate = 0;
  char *edit = 0;

  bool extract(const char *mcap_entry);
  int match(const char *type) const;
};

struct Str;
Str *unquote_mailcap(const char *qstr, const char *type, const char *name,
                     const char *attr, MailcapStat *mc_stat);
MailcapEntry *searchExtViewer(const char *type);
void initMailcap();
char *acceptableMimeTypes();
