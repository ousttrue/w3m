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

