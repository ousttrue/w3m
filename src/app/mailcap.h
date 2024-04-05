#pragma once
#include "enum_template.h"
#include <string>

extern std::string mailcap_files;

enum MailcapFlags {
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
  std::string type;
  std::string viewer;
  MailcapFlags flags = {};
  std::string test;
  std::string nametemplate;
  std::string edit;

  bool extract(const std::string &mcap_entry);
  int match(const char *type) const;
};

std::string unquote_mailcap(const std::string &qstr, const std::string &type,
                            const std::string &name, const std::string &attr,
                            MailcapStat *mc_stat);
// MailcapEntry *searchExtViewer(const char *type);
void initMailcap();
std::string acceptableMimeTypes();
