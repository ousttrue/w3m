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

#ifdef FALSE
#undef FALSE
#endif

#ifdef TRUE
#undef TRUE
#endif

#define FALSE 0
#define TRUE 1

/*
 * Globals.
 */

#define PAGER_MAX_LINE 10000 /* Maximum line kept as pager */
global int PagerMax init(PAGER_MAX_LINE);

global char SearchHeader init(FALSE);
global char* DefaultType init(0);
global char RenderFrame init(FALSE);
global char TargetSelf init(FALSE);
global char DecodeCTE init(FALSE);
global char ArgvIsURL init(TRUE);
global char MetaRefresh init(FALSE);
global char LocalhostOnly init(FALSE);
global char* HostName init(0);

global char* HTTP_proxy init(0);
global char* HTTPS_proxy init(0);
global char* GOPHER_proxy init(0);
global char* FTP_proxy init(0);
global struct Url HTTP_proxy_parsed;
global struct Url HTTPS_proxy_parsed;
global struct Url GOPHER_proxy_parsed;
global struct Url FTP_proxy_parsed;
global char* NO_proxy init(0);
global int NOproxy_netaddr init(TRUE);

global char use_proxy init(TRUE);
#define Do_not_use_proxy (!use_proxy)
global int Do_not_use_ti_te init(FALSE);
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

global int confirm_on_quit init(TRUE);
global int use_mark init(FALSE);
global int emacs_like_lineedit init(FALSE);
global int space_autocomplete init(FALSE);
global int vi_prec_num init(FALSE);
global int label_topline init(FALSE);
global int nextpage_topline init(FALSE);
global char* displayTitleTerm init(0);
global int displayLink init(FALSE);
global int displayLinkNumber init(FALSE);
global int displayLineInfo init(FALSE);
global int DecodeURL init(FALSE);
global int retryAsHttp init(TRUE);
global int showLineNum init(FALSE);
global int show_srch_str init(TRUE);
global int autoImage init(TRUE);
global int useExtImageViewer init(TRUE);
global int maxLoadImage init(4);
global int image_map_list init(TRUE);
global int pseudoInlines init(TRUE);
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
global int BackgroundExtViewer init(TRUE);
#define PRE_FORM_FILE RC_DIR "/pre_form"
global char* pre_form_file init(PRE_FORM_FILE);
global char* ftppasswd init(0);
global int ftppass_hostnamegen init(TRUE);
global int do_download init(FALSE);
global char* image_source init(0);
global int WrapDefault init(FALSE);
global int IgnoreCase init(TRUE);
global int WrapSearch init(FALSE);
global int squeezeBlankLine init(FALSE);
global char* BookmarkFile init(0);
global int UseExternalDirBuffer init(TRUE);
#define CGI_EXTENSION ".cgi"
global char* DirBufferCommand init("file:///$LIB/dirlist" CGI_EXTENSION);
global int UseDictCommand init(TRUE);
global char* DictCommand init("file:///$LIB/w3mdict" CGI_EXTENSION);
global int ignore_null_img_alt init(TRUE);

#define DISPLAY_INS_DEL_SIMPLE 0
#define DISPLAY_INS_DEL_NORMAL 1
#define DISPLAY_INS_DEL_FONTIFY 2

global int displayInsDel init(DISPLAY_INS_DEL_NORMAL);
global int FoldTextarea init(FALSE);
global int FoldLine init(FALSE);

#define DEFAULT_URL_EMPTY 0
#define DEFAULT_URL_CURRENT 1
#define DEFAULT_URL_LINK 2

global int DefaultURLString init(DEFAULT_URL_CURRENT);
global int MarkAllPages init(FALSE);

global struct auth_cookie* Auth_cookie init(0);

global char FollowLocale init(TRUE);
global char UseContentCharset init(TRUE);
global char SearchConv init(TRUE);
global char SimplePreserveSpace init(FALSE);
global char DisplayBorders init(FALSE);
global char DisableCenter init(FALSE);
extern int symbol_width;
extern int symbol_width0;

global char* rc_dir init(0);
global char* tmp_dir;
global char* param_tmp_dir init(0);
global char* config_file init(0);

global int relative_wheel_scroll init(FALSE);
global int fixed_wheel_scroll_count init(5);
global int relative_wheel_scroll_ratio init(30);

global int view_unseenobject init(FALSE);

global char* ssl_key_file init(0);
global char* ssl_ca_path init(0);
#define DEF_CAFILE ""
global char* ssl_ca_file init(DEF_CAFILE);
global int ssl_ca_default init(TRUE);
global char* ssl_forbid_method init("2, 3, t, 5");
#if (OPENSSL_VERSION_NUMBER < 0x10100000L) || defined(LIBRESSL_VERSION_NUMBER)
global char* ssl_cipher init("DEFAULT:!LOW:!RC4:!EXP");
#else
global char* ssl_cipher init(0);
#endif

global int is_redisplay init(FALSE);
global int clear_buffer init(TRUE);

global int use_lessopen init(FALSE);

global int FollowRedirection init(10);

extern void deleteFiles(void);
void w3m_exit(int i);
