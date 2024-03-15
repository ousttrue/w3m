#pragma once
#include <string>

struct keyvalue {
  std::string arg;
  std::string value;
  keyvalue *next;
};

std::string tag_get_value(keyvalue *t, const char *arg);
int tag_exists(keyvalue *t, const char *arg);
keyvalue *cgistr2tagarg(const char *cgistr);
void download_action(keyvalue *arg);
void panel_set_option(keyvalue *);
void set_cookie_flag(keyvalue *arg);
