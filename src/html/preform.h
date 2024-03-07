#pragma once

extern const char *pre_form_file;

struct pre_form_item {
  int type;
  const char *name;
  const char *value;
  int checked;
  pre_form_item *next;
};

struct pre_form {
  const char *url;
  struct Regex *re_url;
  const char *name;
  const char *action;
  pre_form_item *item;
  pre_form *next;
};

pre_form *add_pre_form(pre_form *prev, const char *url, Regex *re_url,
                       const char *name, const char *action);
pre_form_item *add_pre_form_item(pre_form *pf, pre_form_item *prev, int type,
                                 const char *name, const char *value,
                                 const char *checked);
void loadPreForm();
