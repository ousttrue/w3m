/* vi: set sw=4 ts=8 ai sm noet : */
#ifndef W3M_RC_H
#define W3M_RC_H

#include "buffer.h"
#include "config.h"
#include "parsetag.h"

#include <stdio.h>

enum {
    DEFAULT_URL_EMPTY,
    DEFAULT_URL_CURRENT,
    DEFAULT_URL_LINK,
};

enum {
    GRAPHIC_CHAR_CHARSET,
    GRAPHIC_CHAR_DEC,
    GRAPHIC_CHAR_ASCII,
};

enum {
    DISPLAY_INS_DEL_SIMPLE,
    DISPLAY_INS_DEL_NORMAL,
    DISPLAY_INS_DEL_FONTIFY,
};

#define MAILTO_OPTIONS_IGNORE 1
#define MAILTO_OPTIONS_USE_MAILTO_URL 2
#define MAXIMUM_PIXEL_PER_CHAR 32.0
#define MINIMUM_PIXEL_PER_CHAR 4.0

#define IS_EMPTY_PARSED_URL(pu) ((pu)->scheme == SCM_UNKNOWN && !(pu)->file)
#define SCONF_RESERVED 0
#define SCONF_SUBSTITUTE_URL 1
#define SCONF_URL_CHARSET 2
#define SCONF_NO_REFERER_FROM 3
#define SCONF_NO_REFERER_TO 4
#define SCONF_USER_AGENT 5
#define SCONF_N_FIELD 6
#define query_SCONF_SUBSTITUTE_URL(pu) ((char*)querySiteconf(pu, SCONF_SUBSTITUTE_URL))
#define query_SCONF_USER_AGENT(pu) ((const char*)querySiteconf(pu, SCONF_USER_AGENT))
#define query_SCONF_URL_CHARSET(pu) ((const wc_ces*)querySiteconf(pu, SCONF_URL_CHARSET))
#define query_SCONF_NO_REFERER_FROM(pu) ((const int*)querySiteconf(pu, SCONF_NO_REFERER_FROM))
#define query_SCONF_NO_REFERER_TO(pu) ((const int*)querySiteconf(pu, SCONF_NO_REFERER_TO))

#ifdef USE_IMAGE
#define DEFAULT_PIXEL_PER_CHAR 7.0 /* arbitrary */
#define DEFAULT_PIXEL_PER_LINE 14.0 /* arbitrary */
#else
#define DEFAULT_PIXEL_PER_CHAR 8.0 /* arbitrary */
#endif

Buffer* load_option_panel(void);
Str auxbinFile(const char* base);
Str confFile(const char* base);
char* get_param_option(char* name);
Str rcFile(const char* base);
int set_param_option(char* option);
int str_to_bool(const char* value, int old);
void* querySiteconf(ParsedURL* query_pu, int field);
void init_rc(void);
void panel_set_option(struct parsed_tagarg* arg);
void show_params(FILE* fp);
void sync_with_option(void);

extern char* AcceptEncoding;
extern char* AcceptLang;
extern char* AcceptMedia;
extern char* BookmarkFile;
extern char* ExtBrowser2;
extern char* ExtBrowser3;
extern char* ExtBrowser4;
extern char* ExtBrowser5;
extern char* ExtBrowser6;
extern char* ExtBrowser7;
extern char* ExtBrowser8;
extern char* ExtBrowser9;
extern char* ExtBrowser;
extern char* Mailer;
extern char* NNTP_mode;
extern char* NNTP_server;
extern char* UserAgent;
extern char* config_file;
extern char* cookie_accept_domains;
extern char* cookie_avoid_wrong_number_of_dots;
extern char* cookie_reject_domains;
extern char* ftppasswd;
extern char* index_file;
extern char* keymap_file;
extern char* mailcap_files;
extern char* mimetypes_files;
extern char* mkd_tmp_dir;
extern char* param_dl_dir;
extern char* param_tmp_dir;
extern char* passwd_file;
extern char* pre_form_file;
extern char* siteconf_file;
extern char ArgvIsURL;
extern char AutoUncompress;
extern char DecodeCTE;
extern char DisableCenter;
extern char DisplayBorders;
extern char LocalhostOnly;
extern char MetaRefresh;
extern char PreserveTimestamp;
extern char RenderFrame;
extern char TargetSelf;
extern char UseAltEntity;
extern char UseGraphicChar;
extern const char* DirBufferCommand;
extern double pixel_per_char;
extern int BackgroundExtViewer;
extern int CrossOriginReferer;
extern int DecodeURL;
extern int DefaultURLString;
extern int FoldLine;
extern int FoldPre;
extern int FoldTextarea;
extern int FollowRedirection;
extern int IgnoreCase;
extern int IndentIncr;
extern int MailtoOptions;
extern int MarkAllPages;
extern int MaxCols;
extern int MaxNewsMessage;
extern int MessageDelay;
extern int NoSendReferer;
extern int PagerMax;
extern int SmartCase;
extern int UseExternalDirBuffer;
extern int WrapDefault;
extern int accept_bad_cookie;
extern int accept_cookie;
extern int clear_buffer;
extern int close_tab_back;
extern int confirm_on_quit;
extern int disable_secret_security_check;
extern int displayColumnNumber;
extern int displayInsDel;
extern int displayLineInfo;
extern int displayLink;
extern int displayLinkNumber;
extern int emacs_like_lineedit;
extern int enable_inline_image;
extern int exit_on_last;
extern int ftppass_hostnamegen;
extern int ignore_null_img_alt;
extern int label_topline;
extern int multicolList;
extern int nextpage_topline;
extern int open_tab_blank;
extern int open_tab_dl_list;
extern int pixel_per_char_i;
extern int pseudoInlines;
extern int retryAsHttp;
extern int rl_paste;
extern int set_pixel_per_char;
extern int showLineNum;
extern int show_cookie;
extern int show_srch_str;
extern int space_autocomplete;
extern int squeezeBlankLine;
extern int use_cookie;
extern int use_lessopen;
extern int vi_prec_num;
extern int zeroBasedLinkNo;

#ifdef INET6
#define DNS_ORDER_UNSPEC 0
#define DNS_ORDER_INET_INET6 1
#define DNS_ORDER_INET6_INET 2
#define DNS_ORDER_INET_ONLY 4
#define DNS_ORDER_INET6_ONLY 6
extern int DNS_order;
#endif

#ifdef USE_DICT
extern int UseDictCommand;
extern const char* DictCommand;
extern const char* DictPrompt;
#endif /* USE_DICT */

#ifdef USE_EXTERNAL_URI_LOADER
extern char* urimethodmap_files;
#endif

#ifndef USE_HELP_CGI
char* helpFile(const char* base);
#endif

#ifdef USE_HISTORY
extern int UseHistory;
extern int URLHistSize;
extern int SaveURLHist;
#endif

#ifdef USE_IMAGE
extern char* Imgdisplay;
extern double image_scale;
extern double pixel_per_line;
extern int autoImage;
extern int displayImage;
extern int image_map_list;
extern int maxLoadImage;
extern int pixel_per_line_i;
extern int set_pixel_per_line;
extern int useExtImageViewer;
extern int view_unseenobject;
#else
extern int view_unseenobject;
extern int displayImage; /* XXX: emacs-w3m use display_image=off */
#endif

#ifdef USE_MARK
extern int use_mark;
#endif

#ifdef USE_MIGEMO
extern char* migemo_command;
extern int use_migemo;
#endif

#ifdef USE_MOUSE
extern int fixed_wheel_scroll_count;
extern int relative_wheel_scroll;
extern int relative_wheel_scroll_ratio;
extern int reverse_mouse;
extern int use_mouse;
#endif

#ifdef USE_W3MMAILER
#define MAILTO_OPTIONS_USE_W3MMAILER 0
#endif

#endif
