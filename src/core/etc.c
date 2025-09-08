#include "etc.h"
#include "convertline.h"
#include "indep.h"
#include "auth.h"
#include "local.h"
#include "mysignal.h"
#include "quote.h"
#include "ui.h"
#include "rc.h"
#include "myctype.h"
#include <pwd.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/stat.h>

int nextpage_topline = (false);
int disable_secret_security_check = (false);

// Str myExtCommand(char* cmd, char* arg, int redirect)
// {
//     Str tmp = NULL;
//     char* p;
//     int set_arg = false;
//
//     for (p = cmd; *p; p++) {
//         if (*p == '%' && *(p + 1) == 's' && !set_arg) {
//             if (tmp == NULL)
//                 tmp = Strnew_charp_n(cmd, (int)(p - cmd));
//             Strcat_charp(tmp, arg);
//             set_arg = true;
//             p++;
//         } else {
//             if (tmp)
//                 Strcat_char(tmp, *p);
//         }
//     }
//     if (!set_arg) {
//         if (redirect)
//             tmp = Strnew_m_charp("(", cmd, ") < ", arg, NULL);
//         else
//             tmp = Strnew_m_charp(cmd, " ", arg, NULL);
//     }
//     return tmp;
// }

Str myEditor(char* cmd, char* file, int line)
{
    Str tmp = NULL;
    char* p;
    int set_file = false, set_line = false;

    for (p = cmd; *p; p++) {
        if (*p == '%' && *(p + 1) == 's' && !set_file) {
            if (tmp == NULL)
                tmp = Strnew_charp_n(cmd, (int)(p - cmd));
            Strcat_charp(tmp, file);
            set_file = true;
            p++;
        } else if (*p == '%' && *(p + 1) == 'd' && !set_line && line > 0) {
            if (tmp == NULL)
                tmp = Strnew_charp_n(cmd, (int)(p - cmd));
            Strcat(tmp, Sprintf("%d", line));
            set_line = true;
            p++;
        } else {
            if (tmp)
                Strcat_char(tmp, *p);
        }
    }
    if (!set_file) {
        if (tmp == NULL)
            tmp = Strnew_charp(cmd);
        if (!set_line && line > 1 && strcasestr(cmd, "vi"))
            Strcat(tmp, Sprintf(" +%d", line));
        Strcat_m_charp(tmp, " ", file, NULL);
    }
    return tmp;
}

const char* expandName(const char* name)
{
    if (name == NULL)
        return NULL;

    struct passwd* passent;
    Str extpath = NULL;

    const char* p = name;
    if (*p == '/') {
        if ((*(p + 1) == '~' && IS_ALPHA(*(p + 2)))
            && personal_document_root) {
            char* q;
            p += 2;
            q = strchr(p, '/');
            if (q) { /* /~user/dir... */
                passent = getpwnam(allocStr(p, q - p));
                p = q;
            } else { /* /~user */
                passent = getpwnam(p);
                p = "";
            }
            if (!passent)
                goto rest;
            extpath = Strnew_m_charp(passent->pw_dir, "/",
                personal_document_root, NULL);
            if (*personal_document_root == '\0' && *p == '/')
                p++;
        } else
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

char* url_unquote_conv(char* url, wc_ces charset)
{
    wc_uint8 old_auto_detect = WcOption.auto_detect;
    Str tmp;
    tmp = Str_url_unquote(Strnew_charp(url), false, true);
    if (!charset || charset == WC_CES_US_ASCII)
        charset = SystemCharset;
    WcOption.auto_detect = WC_OPT_DETECT_ON;
    tmp = convertLine(NULL, tmp, RAW_MODE, &charset, charset, InnerCharset);
    WcOption.auto_detect = old_auto_detect;
    return tmp->ptr;
}
