#include "runtime.h"
#include "myctype.h"
#include <Str.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <pwd.h>

const char* expandPath(const char* name)
{
    if (name == NULL)
        return NULL;

    struct passwd* passent;
    Str extpath = NULL;
    const char* p = name;
    if (*p == '~') {
        p++;
        if (IS_ALPHA(*p)) {
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
        return extpath->ptr;
    }
rest:
    return name;
}

const char* expandName(const char* name)
{
    if (name == NULL)
        return NULL;

    struct passwd* passent;
    Str extpath = NULL;

    const char* p = name;
    if (*p == '/') {
        // if ((*(p + 1) == '~' && IS_ALPHA(*(p + 2))) && personal_document_root) {
        //     char* q;
        //     p += 2;
        //     q = strchr(p, '/');
        //     if (q) { /* /~user/dir... */
        //         passent = getpwnam(allocStr(p, q - p));
        //         p = q;
        //     } else { /* /~user */
        //         passent = getpwnam(p);
        //         p = "";
        //     }
        //     if (!passent)
        //         goto rest;
        //     extpath = Strnew_m_charp(passent->pw_dir, "/",
        //         personal_document_root, NULL);
        //     if (*personal_document_root == '\0' && *p == '/')
        //         p++;
        // }
        // else
        goto rest;
        if (Strcmp_charp(extpath, "/") == 0 && *p == '/')
            p++;
        Strcat_charp(extpath, p);
        return extpath->ptr;
    } else
        return expandPath(p);
rest:
    return name;
}

static const char*
w3m_dir(const char* name, char* dft)
{
    const char* value = getenv(name);
    return value ? value : dft;
}

const char* w3m_auxbin_dir()
{
    return w3m_dir("W3M_AUXBIN_DIR", AUXBIN_DIR);
}

const char* w3m_lib_dir()
{
    /* FIXME: use W3M_CGIBIN_DIR? */
    return w3m_dir("W3M_LIB_DIR", CGIBIN_DIR);
}

const char* w3m_etc_dir()
{
    return w3m_dir("W3M_ETC_DIR", ETC_DIR);
}

const char* w3m_conf_dir()
{
    return w3m_dir("W3M_CONF_DIR", CONF_DIR);
}

const char* w3m_help_dir()
{
    return w3m_dir("W3M_HELP_DIR", HELP_DIR);
}
