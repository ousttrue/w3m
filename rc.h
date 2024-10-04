#pragma once

const char *get_param_option(const char *name);
int set_param_option(const char *option);
bool str_to_bool(const char *value, bool old);

void init_rc(void);
struct Buffer *load_option_panel(void);
struct parsed_tagarg;
void panel_set_option(struct parsed_tagarg *);
void sync_with_option(void);
struct Url;
extern void initMimeTypes();
const char *rcFile(const char *base);
