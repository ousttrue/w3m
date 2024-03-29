#pragma once
#include <string>
#include <list>

#define COOKIE_FILE "cookie"

extern std::string rc_dir;
extern bool ArgvIsURL;

#define INET6 1

std::string rcFile(std::string_view base);
std::string etcFile(const char *base);
std::string confFile(const char *base);
const char *libFile(const char *base);
const char *helpFile(const char *base);
int set_param_option(const char *option);
void init_rc();
const char *w3m_etc_dir();
const char *w3m_conf_dir();
const char *w3m_help_dir();
void sync_with_option(void);
#define BOOKMARK "bookmark.html"

extern const char *w3m_reqlog;

extern const char *siteconf_file;
extern const char *ftppasswd;
extern int ftppass_hostnamegen;

extern int WrapDefault;

extern std::string BookmarkFile;
extern struct auth_cookie *Auth_cookie;
extern std::string config_file;
extern int is_redisplay;
extern int clear_buffer;
extern int set_pixel_per_char;

void panel_set_option(const std::list<std::pair<std::string, std::string>> &);
