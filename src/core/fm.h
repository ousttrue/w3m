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

global char TargetSelf init(FALSE);

global int CurrentKey;
global char* CurrentKeyData;
global char* CurrentCmdData;

global int confirm_on_quit init(TRUE);
global int use_mark init(FALSE);

global char* Mailer init(DEF_MAILER);

#define MAILTO_OPTIONS_IGNORE 1
#define MAILTO_OPTIONS_USE_MAILTO_URL 2
global int MailtoOptions init(MAILTO_OPTIONS_IGNORE);
global char* ExtBrowser init(DEF_EXT_BROWSER);
global char* ExtBrowser2 init(NULL);
global char* ExtBrowser3 init(NULL);
global char* ExtBrowser4 init(NULL);
global char* ExtBrowser5 init(NULL);
global char* ExtBrowser6 init(NULL);
global char* ExtBrowser7 init(NULL);
global char* ExtBrowser8 init(NULL);
global char* ExtBrowser9 init(NULL);
global int BackgroundExtViewer init(TRUE);
global int WrapDefault init(FALSE);
global char* BookmarkFile init(NULL);

global int UseDictCommand init(TRUE);
global char* DictCommand init("file:///$LIB/w3mdict" CGI_EXTENSION);
#define DEFAULT_URL_EMPTY 0
#define DEFAULT_URL_CURRENT 1
#define DEFAULT_URL_LINK 2
global int DefaultURLString init(DEFAULT_URL_CURRENT);



global char ExtHalfdump init(FALSE);
global char FollowLocale init(TRUE);

global char UseAltEntity init(FALSE);
global char* param_tmp_dir init(NULL);
#ifdef HAVE_MKDTEMP
global char* mkd_tmp_dir init(NULL);
#endif
global char* config_file init(NULL);


global int is_redisplay init(FALSE);
global int clear_buffer init(TRUE);



void w3m_exit(int i);

