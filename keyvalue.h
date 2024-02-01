#pragma once

struct keyvalue {
  const char *arg;
  const char *value;
  keyvalue *next;
};

const char *tag_get_value(keyvalue *t, const char *arg);
int tag_exists(keyvalue *t, const char *arg);
keyvalue *cgistr2tagarg(const char *cgistr);
void download_action(keyvalue *arg);
void panel_set_option(keyvalue *);
void set_cookie_flag(keyvalue *arg);
