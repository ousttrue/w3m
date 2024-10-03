#pragma once

const char *get_param_option(const char *name);
int set_param_option(const char *option);
int str_to_bool(const char *value, int old);

void init_rc(void);
struct Buffer *load_option_panel(void);
struct parsed_tagarg;
void panel_set_option(struct parsed_tagarg *);
void sync_with_option(void);
char *rcFile(char *base);
char *etcFile(char *base);
char *confFile(char *base);
char *libFile(char *base);
char *helpFile(char *base);
