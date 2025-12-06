/*
 * w3m: WWW wo Miru utility
 *
 * by A.ITO  Feb. 1995
 *
 * You can use,copy,modify and distribute this program without any permission.
 */
#pragma once
#ifdef MAINPROGRAM
#define global
#define init(x) = (x)
#else /* not MAINPROGRAM */
#define global extern
#define init(x)
#endif /* not MAINPROGRAM */

#define DEFUN(funcname, macroname, docstring) void funcname(void)

/*
 * Globals.
 */
#include <stdbool.h>

#define PAGER_MAX_LINE 10000 /* Maximum line kept as pager */
global int PagerMax init(PAGER_MAX_LINE);

global char SearchHeader init(false);
global char* DefaultType init(0);
global char RenderFrame init(false);
global char DecodeCTE init(false);
global char ArgvIsURL init(true);
global char MetaRefresh init(false);
global char LocalhostOnly init(false);

global char* HTTP_proxy init(0);
global char* HTTPS_proxy init(0);
global char* GOPHER_proxy init(0);
global char* FTP_proxy init(0);
global struct Url HTTP_proxy_parsed;
global struct Url HTTPS_proxy_parsed;
global struct Url GOPHER_proxy_parsed;
global struct Url FTP_proxy_parsed;
global char* NO_proxy init(0);
global int NOproxy_netaddr init(true);

global char use_proxy init(true);
#define Do_not_use_proxy (!use_proxy)
global int Do_not_use_ti_te init(false);
global char* NNTP_server init(0);
global char* NNTP_mode init(0);
global int MaxNewsMessage init(50);

global char* document_root init(0);
global char* personal_document_root init(0);
global char* cgi_bin init(0);
global char* index_file init(0);

global int CurrentKey;
global char* CurrentKeyData;
global char* CurrentCmdData;
global char* w3m_reqlog;
extern int enable_inline_image;

global int confirm_on_quit init(true);
global int use_mark init(false);
global int emacs_like_lineedit init(false);
global int space_autocomplete init(false);
global int vi_prec_num init(false);
global int label_topline init(false);
global char* displayTitleTerm init(0);
global int displayLink init(false);
global int displayLinkNumber init(false);
global int displayLineInfo init(false);
global int DecodeURL init(false);
global int retryAsHttp init(true);
global int showLineNum init(false);
global int show_srch_str init(true);
global int autoImage init(true);
global int useExtImageViewer init(true);
global int maxLoadImage init(4);
global int image_map_list init(true);
global int pseudoInlines init(true);
#define DEF_EDITOR "/usr/bin/vi"
#define DEF_MAILER "/usr/bin/mail"
global char* Editor init(DEF_EDITOR);
global char* Mailer init(DEF_MAILER);
#define DEF_EXT_BROWSER "/usr/bin/firefox"
global char* ExtBrowser init(DEF_EXT_BROWSER);
global char* ExtBrowser2 init(0);
global char* ExtBrowser3 init(0);
global char* ExtBrowser4 init(0);
global char* ExtBrowser5 init(0);
global char* ExtBrowser6 init(0);
global char* ExtBrowser7 init(0);
global char* ExtBrowser8 init(0);
global char* ExtBrowser9 init(0);
global int BackgroundExtViewer init(true);
#define PRE_FORM_FILE RC_DIR "/pre_form"
global char* pre_form_file init(PRE_FORM_FILE);
global char* ftppasswd init(0);
global int ftppass_hostnamegen init(true);
global int do_download init(false);
global char* image_source init(0);
global int WrapDefault init(false);
global int IgnoreCase init(true);
global int WrapSearch init(false);
global int squeezeBlankLine init(false);
global char* BookmarkFile init(0);
global int UseExternalDirBuffer init(true);
#define CGI_EXTENSION ".cgi"
global char* DirBufferCommand init("file:///$LIB/dirlist" CGI_EXTENSION);
global int UseDictCommand init(true);
global char* DictCommand init("file:///$LIB/w3mdict" CGI_EXTENSION);
global int ignore_null_img_alt init(true);

#define DISPLAY_INS_DEL_SIMPLE 0
#define DISPLAY_INS_DEL_NORMAL 1
#define DISPLAY_INS_DEL_FONTIFY 2

global int displayInsDel init(DISPLAY_INS_DEL_NORMAL);
global int FoldTextarea init(false);
global int FoldLine init(false);

#define DEFAULT_URL_EMPTY 0
#define DEFAULT_URL_CURRENT 1
#define DEFAULT_URL_LINK 2

global int DefaultURLString init(DEFAULT_URL_CURRENT);
global int MarkAllPages init(false);

global struct auth_cookie* Auth_cookie init(0);

global char FollowLocale init(true);
global char SearchConv init(true);
global char SimplePreserveSpace init(false);
global char DisplayBorders init(false);
global char DisableCenter init(false);
extern int symbol_width;
extern int symbol_width0;

global int relative_wheel_scroll init(false);
global int fixed_wheel_scroll_count init(5);
global int relative_wheel_scroll_ratio init(30);

global int view_unseenobject init(false);

global char* ssl_key_file init(0);
global char* ssl_ca_path init(0);
#define DEF_CAFILE ""
global char* ssl_ca_file init(DEF_CAFILE);
global int ssl_ca_default init(true);
global char* ssl_forbid_method init("2, 3, t, 5");
#if (OPENSSL_VERSION_NUMBER < 0x10100000L) || defined(LIBRESSL_VERSION_NUMBER)
global char* ssl_cipher init("DEFAULT:!LOW:!RC4:!EXP");
#else
global char* ssl_cipher init(0);
#endif

global int clear_buffer init(true);

global int use_lessopen init(false);

global int FollowRedirection init(10);

extern void deleteFiles(void);
void w3m_exit(int i);
