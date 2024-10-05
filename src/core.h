#pragma once
#include <stdint.h>

extern const char *CurrentDir;

struct Event {
  int cmd;
  void *data;
  struct Event *next;
};
extern struct Event *CurrentEvent;

void initialize();
const char *getCurrentDir();
uint32_t getCurrentPid();

void pushEvent(int cmd, void *data);

const char *w3m_auxbin_dir();
const char *w3m_lib_dir();
const char *w3m_etc_dir();
const char *w3m_conf_dir();
const char *w3m_help_dir();
const char *etcFile(const char *base);
const char *confFile(const char *base);
const char *expandPath(const char *name);
