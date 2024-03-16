#pragma once
#include <stdio.h>
#include <string>

extern const char *rc_dir;
extern char *index_file;
extern bool AutoUncompress;
extern bool PreserveTimestamp;
extern bool ArgvIsURL;
extern bool LocalhostOnly;
extern bool retryAsHttp;

#define INET6 1
#ifdef INET6
#define DNS_ORDER_UNSPEC 0
#define DNS_ORDER_INET_INET6 1
#define DNS_ORDER_INET6_INET 2
#define DNS_ORDER_INET_ONLY 4
#define DNS_ORDER_INET6_ONLY 6
extern int ai_family_order_table[7][3]; /* XXX */
#endif                                  /* INET6 */
extern int DNS_order;

const char *rcFile(const char *base);
const char *etcFile(const char *base);
const char *confFile(const char *base);
const char *auxbinFile(const char *base);
const char *libFile(const char *base);
const char *helpFile(const char *base);
int set_param_option(const char *option);
void init_rc();
const char *w3m_auxbin_dir();
const char *w3m_lib_dir();
const char *w3m_etc_dir();
const char *w3m_conf_dir();
const char *w3m_help_dir();
void sync_with_option(void);
#define KEYMAP_FILE "keymap"
#define CGI_EXTENSION ".cgi"
#define BOOKMARK "bookmark.html"

extern const char *w3m_reqlog;
extern const char *w3m_version;
extern char *document_root;
extern int label_topline;
extern int displayImage;

extern int BackgroundExtViewer;
extern const char *siteconf_file;
extern const char *ftppasswd;
extern int ftppass_hostnamegen;

extern int WrapDefault;

extern const char *BookmarkFile;
extern const char *DirBufferCommand;
extern struct auth_cookie *Auth_cookie;
extern struct Cookie *First_cookie;
extern int no_rc_dir;
extern const char *config_file;
extern int is_redisplay;
extern int clear_buffer;
extern int set_pixel_per_char;
extern const char *keymap_file;
