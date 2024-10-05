#pragma once

#define CGI_EXTENSION ".cgi"

extern const char *rc_dir;
extern const char *DictCommand;

#define MAILTO_OPTIONS_IGNORE 1
#define MAILTO_OPTIONS_USE_MAILTO_URL 2
extern int MailtoOptions;

#define DEFAULT_URL_EMPTY 0
#define DEFAULT_URL_CURRENT 1
#define DEFAULT_URL_LINK 2
extern int DefaultURLString;

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
