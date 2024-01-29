#pragma once

struct parsed_tagarg {
  char *arg;
  char *value;
  parsed_tagarg *next;
};

char *tag_get_value(parsed_tagarg *t, char *arg);
int tag_exists(parsed_tagarg *t, char *arg);
parsed_tagarg *cgistr2tagarg(char *cgistr);
void download_action(struct parsed_tagarg *arg);
void follow_map(struct parsed_tagarg *arg);
void panel_set_option(struct parsed_tagarg *);
void set_cookie_flag(struct parsed_tagarg *arg);
