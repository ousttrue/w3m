/* $Id: indep.h,v 1.16 2003/09/22 21:02:19 ukai Exp $ */
#ifndef INDEP_H
#define INDEP_H
#include <gcstr/Str.h>
#include "config.h"
#include <stdlib.h>

#ifndef TRUE
#define TRUE 1
#endif /* TRUE */
#ifndef FALSE
#define FALSE 0
#endif /* FALSE */

#define RAW_MODE 0
#define PAGER_MODE 1
#define HTML_MODE 2
#define HEADER_MODE 3

extern long long strtoclen(const char* s);
extern int strCmp(const void* s1, const void* s2);
extern char* currentdir(void);
extern char* cleanupName(char* name);
extern const char* expandPath(const char* name);
#ifndef HAVE_STRCHR
extern char* strchr(const char* s, int c);
#endif /* not HAVE_STRCHR */
#ifndef HAVE_STRCASECMP
extern int strcasecmp(const char* s1, const char* s2);
extern int strncasecmp(const char* s1, const char* s2, size_t n);
#endif /* not HAVE_STRCASECMP */
#ifndef HAVE_STRCASESTR
extern char* strcasestr(const char* s1, const char* s2);
#endif
extern int strcasemstr(char* str, char* srch[], char** ret_ptr);
int strmatchlen(const char* s1, const char* s2, int maxlen);
extern void cleanup_line(Str s, int mode);
extern char* html_quote(const char* str);
extern char* html_unquote(char* str);
extern char* file_quote(char* str);
extern char* file_unquote(char* str);
extern Str Str_url_unquote(Str x, int is_form, int safe);
extern Str Str_form_quote(Str x);
#define Str_form_unquote(x) Str_url_unquote((x), TRUE, FALSE)
extern char* shell_quote(char* str);

extern char* w3m_auxbin_dir(void);
extern char* w3m_lib_dir(void);
extern char* w3m_etc_dir(void);
extern char* w3m_conf_dir(void);
extern char* w3m_help_dir(void);

#endif /* INDEP_H */
