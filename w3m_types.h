#pragma once
#include "Str.h"
#include "termcap_util.h"
#include <libwc/ces.h>

#ifdef FALSE
#undef FALSE
#endif
#ifdef TRUE
#undef TRUE
#endif
#define FALSE 0
#define TRUE 1

#define MAILTO_OPTIONS_IGNORE 1
#define MAILTO_OPTIONS_USE_MAILTO_URL 2

#define DISPLAY_INS_DEL_SIMPLE 0
#define DISPLAY_INS_DEL_NORMAL 1
#define DISPLAY_INS_DEL_FONTIFY 2

#define DEFAULT_URL_EMPTY 0
#define DEFAULT_URL_CURRENT 1
#define DEFAULT_URL_LINK 2

#define DNS_ORDER_UNSPEC 0
#define DNS_ORDER_INET_INET6 1
#define DNS_ORDER_INET6_INET 2
#define DNS_ORDER_INET_ONLY 4
#define DNS_ORDER_INET6_ONLY 6

struct Runtime {
    struct TextList* NO_proxy_domains;

    char* CurrentDir;
    int CurrentPid;
    struct TextList* fileToDelete;
    char* tmp_dir;

    int view_unseenobject;
    int is_redisplay;
    int clear_buffer;
    double image_scale;
    char* keymap_file;
    int FollowRedirection;
    int w3m_backend;
    int multicolList;
    char FollowLocale;
    char UseContentCharset;
    char SearchConv;
    char SimplePreserveSpace;
    char UseAltEntity;
    char DisplayBorders;
    char DisableCenter;
    int no_rc_dir;
    char* rc_dir;
    char* param_tmp_dir;
    char* mkd_tmp_dir;
    char* config_file;
    int default_use_cookie;
    int use_cookie;
    int show_cookie;
    int accept_cookie;
    int accept_bad_cookie;
    char* cookie_reject_domains;
    char* cookie_accept_domains;
    char* cookie_avoid_wrong_number_of_dots;
    int w3m_dump;
    Str header_string;
    int override_content_type;
    int override_user_agent;
    int confirm_on_quit;
    int use_mark;
    int vi_prec_num;
    int label_topline;
    int nextpage_topline;
    char* displayTitleTerm;
    int displayLinkNumber;
    int retryAsHttp;
    int show_srch_str;
    char* Imgdisplay;
    int autoImage;
    int useExtImageViewer;
    int maxLoadImage;
    int image_map_list;
    int pseudoInlines;
    char* Editor;
    char* Mailer;
    int MailtoOptions;
    char* ExtBrowser;
    char* ExtBrowser2;
    char* ExtBrowser3;
    char* ExtBrowser4;
    char* ExtBrowser5;
    char* ExtBrowser6;
    char* ExtBrowser7;
    char* ExtBrowser8;
    char* ExtBrowser9;
    int BackgroundExtViewer;
    int disable_secret_security_check;
    char* passwd_file;
    char* pre_form_file;
    char* ftppasswd;
    int ftppass_hostnamegen;
    char* UserAgent;
    int NoSendReferer;
    int CrossOriginReferer;
    char* AcceptLang;
    const char* AcceptEncoding;
    char* AcceptMedia;
    int WrapDefault;
    int IgnoreCase;
    int WrapSearch;
    int squeezeBlankLine;
    char* BookmarkFile;
    int UseExternalDirBuffer;
    char* DirBufferCommand;
    int UseDictCommand;
    char* DictCommand;
    int ignore_null_img_alt;
    int displayInsDel;
    int FoldTextarea;
    int DefaultURLString;
    int MarkAllPages;
    char* mailcap_files;
    char* mimetypes_files;
    char* urimethodmap_files;
    int open_tab_blank;
    int open_tab_dl_list;
    int close_tab_back;
    int TabCols;
    int DNS_order;
    char NoCache;
    char use_proxy;
    char* document_root;
    char* personal_document_root;
    char* cgi_bin;
    char* index_file;
    char* HTTP_proxy;
    char* HTTPS_proxy;
    char* FTP_proxy;
    char* NO_proxy;
    int NOproxy_netaddr;
    int IndentIncr;
    int PagerMax;
    const char* DefaultType;
    char RenderFrame;
    char TargetSelf;
    char PermitSaveToPipe;
    char AutoUncompress;
    char PreserveTimestamp;
    char ArgvIsURL;
    char MetaRefresh;
    char LocalhostOnly;
    char* HostName;
    char TrapSignal;
    int ssl_verify_server;
    char* ssl_cert_file;
    char* ssl_key_file;
    char* ssl_ca_path;
    char* ssl_ca_file;
    int ssl_ca_default;
    int ssl_path_modified;
    char* ssl_forbid_method;
    char* ssl_min_version;
    char* ssl_cipher;

    const char* image_source;
    int DecodeURL;
    char QuietMessage;
    int ShowEffect;
    int displayLink;
    int displayLineInfo;
    int useColor;
    int displayImage;
    int enable_inline_image;
    int activeImage;
    double pixel_per_char;
    int pixel_per_char_i;
    int set_pixel_per_char;
    double pixel_per_line;
    int pixel_per_line_i;
    int set_pixel_per_line;
    int basic_color; /* don't change */
    int anchor_color; /* blue  */
    int image_color; /* green */
    int form_color; /* red   */
    int bg_color; /* don't change */
    int mark_color; /* cyan */
    int useActiveColor;
    int active_color; /* cyan */
    int useVisitedColor;
    int visited_color; /* magenta  */

    int space_autocomplete;
    int emacs_like_lineedit;

    // Don't change
    enum wc_ces InnerCharset;
    enum wc_ces DisplayCharset;
    enum wc_ces DocumentCharset;
    enum wc_ces SystemCharset;
    enum wc_ces BookmarkCharset;
    // FIXME: charset of source code
    enum wc_ces OptionCharset;
    int OptionEncode;

    char ExtHalfdump;
    int Tabstop;
    int showLineNum;
    int FoldLine;

    int lines;
    int cols;

    struct TermcapEntry termcap;

    bool Do_not_use_ti_te;
    int highIntensityColors;

    int UseHistory;
    int URLHistSize;
    int SaveURLHist;
    struct Hist* LoadHist;
    struct Hist* SaveHist;
    struct Hist* URLHist;
    struct Hist* ShellHist;
    struct Hist* TextHist;

    struct TabBuffer* CurrentTab;
    struct TabBuffer* FirstTab;
    struct TabBuffer* LastTab;
    int nTab;

    int CurrentKey;
    int prec_num;
    int prev_key;
    const char* CurrentKeyData;
    const char* CurrentCmdData;
    struct Event* CurrentEvent;
    struct Event* LastEvent;
};
extern struct Runtime g_runtime;

//
// params
//
struct sel_c {
    int value;
    const char* cvalue;
    const char* text;
};

enum ParamTypes {
    P_INT = 0,
    P_SHORT = 1,
    P_CHARINT = 2,
    P_CHAR = 3,
    P_STRING = 4,
    P_SSLPATH = 5,
    P_COLOR = 6,
    P_CODE = 7,
    P_PIXELS = 8,
    P_NZINT = 9,
    P_SCALE = 10,
};

enum ParamInputTypes {
    PI_TEXT = 0,
    PI_ONOFF = 1,
    PI_SEL_C = 2,
    PI_CODE = 3,
};

struct param_ptr {
    const char* name;
    enum ParamTypes type;
    enum ParamInputTypes inputtype;
    /// pointer to global variable
    void* varptr;
    const char* comment;
    void* select;
};

struct param_section {
    const char* name;
    struct param_ptr* params;
};
