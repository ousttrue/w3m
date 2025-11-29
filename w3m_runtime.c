#include "w3m_runtime.h"
#include <gcstr.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>

struct w3m w3m = { 0 };

#define DISPLAY_CHARSET WC_CES_UTF_8
#define DOCUMENT_CHARSET WC_CES_UTF_8
#define SYSTEM_CHARSET WC_CES_UTF_8

wc_ces DisplayCharset = DISPLAY_CHARSET;
wc_ces DocumentCharset = DOCUMENT_CHARSET;
wc_ces SystemCharset = SYSTEM_CHARSET;
wc_ces BookmarkCharset = SYSTEM_CHARSET;

static char*
w3m_dir(const char* name, char* dft)
{
#ifdef USE_PATH_ENVVAR
    char* value = getenv(name);
    return value ? value : dft;
#else
    return dft;
#endif
}

char* w3m_auxbin_dir(void)
{
    return w3m_dir("W3M_AUXBIN_DIR", AUXBIN_DIR);
}

char* w3m_lib_dir(void)
{
    /* FIXME: use W3M_CGIBIN_DIR? */
    return w3m_dir("W3M_LIB_DIR", CGIBIN_DIR);
}

char* w3m_etc_dir(void)
{
    return w3m_dir("W3M_ETC_DIR", ETC_DIR);
}

char* w3m_conf_dir(void)
{
    return w3m_dir("W3M_CONF_DIR", CONF_DIR);
}

char* w3m_help_dir(void)
{
    return w3m_dir("W3M_HELP_DIR", HELP_DIR);
}

Str expandPath(const char* name)
{
    if (name == NULL)
        return NULL;

    const char* p = name;
    if (*p == '~') {
        p++;
        Str extpath = NULL;
        if (IS_ALPHA(*p)) {
            struct passwd* passent;
            char* q = strchr(p, '/');
            if (q) { /* ~user/dir... */
                passent = getpwnam(allocStr(p, q - p));
                p = q;
            } else { /* ~user */
                passent = getpwnam(p);
                p = "";
            }
            if (!passent)
                goto rest;
            extpath = Strnew_charp(passent->pw_dir);
        } else if (*p == '/' || *p == '\0') { /* ~/dir... or ~ */
            extpath = Strnew_charp(getenv("HOME"));
        } else
            goto rest;
        if (Strcmp_charp(extpath, "/") == 0 && *p == '/')
            p++;
        Strcat_charp(extpath, p);
        return extpath;
    }
rest:
    return Strnew_charp(name);
}
