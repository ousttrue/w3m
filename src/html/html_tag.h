#pragma once

struct HtmlTag {
  const char *arg;
  const char *value;
  struct HtmlTag *next;
};

const char *tag_get_value(struct HtmlTag *t, const char *arg);
bool tag_exists(struct HtmlTag *t, const char *arg);
struct HtmlTag *cgistr2tagarg(const char *cgistr);
