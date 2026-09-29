#include "str_gc.h"
#include "config.h"
#include "indep.h"
#include "myctype.h"
#include "textlist.h"
#include <pwd.h>
#include <libgen.h>
#include <stdlib.h>

const wc_ces InnerCharset = WC_CES_WTF;
wc_ces SystemCharset = SYSTEM_CHARSET;
const char* Editor = DEF_EDITOR;
const char* personal_document_root = NULL;
char* rc_dir = NULL;
char* tmp_dir;

Str mydirname(const char* s)
{
    const char* p = s;
    while (*p)
        p++;
    if (s != p)
        p--;
    while (s != p && *p == '/')
        p--;
    while (s != p && *p != '/')
        p--;
    if (*p != '/')
        return Strnew_charp(".");
    while (s != p && *p == '/')
        p--;
    return Strnew_charp_n(s, strlen(s) - strlen(p) + 1);
}

static char roman_num1[] = {
    'i',
    'x',
    'c',
    'm',
    '*',
};
static char roman_num5[] = {
    'v',
    'l',
    'd',
    '*',
};

static Str
romanNum2(int l, int n)
{
    Str s = Strnew();

    switch (n) {
    case 1:
    case 2:
    case 3:
        for (; n > 0; n--)
            Strcat_char(s, roman_num1[l]);
        break;
    case 4:
        Strcat_char(s, roman_num1[l]);
        Strcat_char(s, roman_num5[l]);
        break;
    case 5:
    case 6:
    case 7:
    case 8:
        Strcat_char(s, roman_num5[l]);
        for (n -= 5; n > 0; n--)
            Strcat_char(s, roman_num1[l]);
        break;
    case 9:
        Strcat_char(s, roman_num1[l]);
        Strcat_char(s, roman_num1[l + 1]);
        break;
    }
    return s;
}

Str romanNumeral(int n)
{
    Str r = Strnew();

    if (n <= 0)
        return r;
    if (n >= 4000) {
        Strcat_charp(r, "**");
        return r;
    }
    Strcat(r, romanNum2(3, n / 1000));
    Strcat(r, romanNum2(2, (n % 1000) / 100));
    Strcat(r, romanNum2(1, (n % 100) / 10));
    Strcat(r, romanNum2(0, n % 10));

    return r;
}

Str romanAlphabet(int n)
{
    Str r = Strnew();
    int l;
    char buf[14];

    if (n <= 0)
        return r;

    l = 0;
    while (n) {
        buf[l++] = 'a' + (n - 1) % 26;
        n = (n - 1) / 26;
    }
    l--;
    for (; l >= 0; l--)
        Strcat_char(r, buf[l]);

    return r;
}

Str myExtCommand(const char* cmd, const char* arg, bool redirect)
{
    Str tmp = NULL;
    const char* p;
    bool set_arg = false;

    for (p = cmd; *p; p++) {
        if (*p == '%' && *(p + 1) == 's' && !set_arg) {
            if (tmp == NULL)
                tmp = Strnew_charp_n(cmd, (int)(p - cmd));
            Strcat_charp(tmp, arg);
            set_arg = true;
            p++;
        } else {
            if (tmp)
                Strcat_char(tmp, *p);
        }
    }
    if (!set_arg) {
        if (redirect)
            tmp = Strnew_m_charp("(", cmd, ") < ", arg, NULL);
        else
            tmp = Strnew_m_charp(cmd, " ", arg, NULL);
    }
    return tmp;
}

Str editor_cmd(const char* file, int line)
{
    Str tmp;
    const char* p;
    int n;
    bool file_set = false;
    bool line_set = false;

    static const char* eds[] = {
        "emacs",
        "hx", /* helix */
        "micro",
        "nano",
        "nvi",
        "nvim",
        "vi",
        "vim",
        NULL
    };

    tmp = Strnew();
    if (!*Editor)
        return tmp;

    for (p = Editor; *p; p++) {
        if (*p == '%' && p[1] == 's') {
            Strcat_charp(tmp, file);
            file_set = true;
            p++;
        } else if (*p == '%' && p[1] == 'd') {
            Strcat(tmp, Sprintf("%d", line));
            line_set = true;
            p++;
        } else
            Strcat_char(tmp, *p);
    }

    if (file_set)
        return tmp;

    n = strcspn(Editor, " ");
    if (!(p = basename(Strnew_charp_n(Editor, n)->ptr)))
        p = Editor;

    if (!line_set && line > 0)
        for (const char** e = eds; *e; e++)
            if (!strcmp(p, *e)) {
                Strcat(tmp, Sprintf(" +%d", line));
                break;
            }

    Strcat_m_charp(tmp, " ", file, NULL);
    return tmp;
}

Str expandPath(const char* name)
{
    if (name == NULL)
        return NULL;

    struct passwd* passent;
    Str extpath = NULL;
    const char* p = name;
    if (*p == '~') {
        p++;
#ifndef __MINGW32_VERSION
        if (IS_ALPHA(*p)) {
            const char* q = strchr(p, '/');
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
        } else
#endif /* __MINGW32_VERSION */
            if (*p == '/' || *p == '\0') { /* ~/dir... or ~ */
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
Str expandName(const char* name)
{
    struct passwd* passent;
    Str extpath = NULL;

    if (name == NULL)
        return NULL;
    const char* p = name;
    if (*p == '/') {
        if ((*(p + 1) == '~' && IS_ALPHA(*(p + 2)))
            && personal_document_root) {
            p += 2;
            const char* q = strchr(p, '/');
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
        return extpath;
    } else
        return expandPath(p);
rest:
    return Strnew_charp(name);
}

Str file_to_url(const char* file, const char* CurrentDir)
{
    Str tmp;
#ifdef SUPPORT_DOS_DRIVE_PREFIX
    char* drive = NULL;
#endif
#ifdef SUPPORT_NETBIOS_SHARE
    char* host = NULL;
#endif

    if (!(file = expandPath(file)->ptr))
        return NULL;
#ifdef SUPPORT_NETBIOS_SHARE
    if (file[0] == '/' && file[1] == '/') {
        char* p;
        file += 2;
        if (*file) {
            p = strchr(file, '/');
            if (p != NULL && p != file) {
                host = allocStr(file, (p - file));
                file = p;
            }
        }
    }
#endif
#ifdef SUPPORT_DOS_DRIVE_PREFIX
    if (IS_ALPHA(file[0]) && file[1] == ':') {
        drive = allocStr(file, 2);
        file += 2;
    } else
#endif
        if (file[0] != '/') {
        tmp = Strnew_charp(CurrentDir);
        if (Strlastchar(tmp) != '/')
            Strcat_char(tmp, '/');
        Strcat_charp(tmp, file);
        file = tmp->ptr;
    }
    tmp = Strnew_charp("file://");
#ifdef SUPPORT_NETBIOS_SHARE
    if (host)
        Strcat_charp(tmp, host);
#endif
#ifdef SUPPORT_DOS_DRIVE_PREFIX
    if (drive)
        Strcat_charp(tmp, drive);
#endif
    Strcat_charp(tmp, file_quote(cleanupName(file)->ptr));
    return tmp;
}

Str cleanupName(const char* name)
{
    char* p;
    const char* q;

    Str buf = Strnew_charp(name);
    p = buf->ptr;
    q = name;
    while (*q != '\0') {
        if (strncmp(p, "/../", 4) == 0) { /* foo/bar/../FOO */
            if (p - 2 == buf->ptr && strncmp(p - 2, "..", 2) == 0) {
                /* ../../       */
                p += 3;
                q += 3;
            } else if (p - 3 >= buf->ptr && strncmp(p - 3, "/..", 3) == 0) {
                /* ../../../    */
                p += 3;
                q += 3;
            } else {
                while (p != buf->ptr && *--p != '/')
                    ; /* ->foo/FOO */
                *p = '\0';
                q += 3;
                Strcat_charp(buf, q);
            }
        } else if (strcmp(p, "/..") == 0) { /* foo/bar/..   */
            if (p - 2 == buf->ptr && strncmp(p - 2, "..", 2) == 0) {
                /* ../..        */
            } else if (p - 3 >= buf->ptr && strncmp(p - 3, "/..", 3) == 0) {
                /* ../../..     */
            } else {
                while (p != buf->ptr && *--p != '/')
                    ; /* ->foo/ */
                *++p = '\0';
            }
            break;
        } else if (strncmp(p, "/./", 3) == 0) { /* foo/./bar */
            *p = '\0'; /* -> foo/bar           */
            q += 2;
            Strcat_charp(buf, q);
        } else if (strcmp(p, "/.") == 0) { /* foo/. */
            *++p = '\0'; /* -> foo/              */
            break;
        } else if (strncmp(p, "//", 2) == 0) { /* foo//bar */
            /* -> foo/bar           */
            *p = '\0';
            q++;
            Strcat_charp(buf, q);
        } else {
            p++;
            q++;
        }
    }
    return buf;
}

static const char Base64Table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static void Strcatc(Str x, char y)
{
    ((x)->ptr[(x)->length++] = (y));
}

Str base64_encode(const char* src, size_t len)
{
    Str dest;
    const unsigned char *in, *endw, *s;
    unsigned long j;
    size_t k;

    s = (const unsigned char*)src;

    k = len;
    if (k % 3)
        k += 3 - (k % 3);

    k = k / 3 * 4;

    if (!len || k + 1 < len)
        return Strnew();

    dest = Strnew_size(k);
    if (dest->area_size <= k) {
        Strfree(dest);
        return Strnew();
    }

    in = s;

    endw = s + len - 2;

    while (in < endw) {
        j = *in++;
        j = j << 8 | *in++;
        j = j << 8 | *in++;

        Strcatc(dest, Base64Table[(j >> 18) & 0x3f]);
        Strcatc(dest, Base64Table[(j >> 12) & 0x3f]);
        Strcatc(dest, Base64Table[(j >> 6) & 0x3f]);
        Strcatc(dest, Base64Table[j & 0x3f]);
    }

    if (s + len - in) {
        j = *in++;
        if (s + len - in) {
            j = j << 8 | *in++;
            j = j << 8;
            Strcatc(dest, Base64Table[(j >> 18) & 0x3f]);
            Strcatc(dest, Base64Table[(j >> 12) & 0x3f]);
            Strcatc(dest, Base64Table[(j >> 6) & 0x3f]);
        } else {
            j = j << 8;
            j = j << 8;
            Strcatc(dest, Base64Table[(j >> 18) & 0x3f]);
            Strcatc(dest, Base64Table[(j >> 12) & 0x3f]);
            Strcatc(dest, '=');
        }
        Strcatc(dest, '=');
    }
    dest->ptr[dest->length] = '\0';
    return dest;
}

TextList* fileToDelete;

void initFileToDelete()
{
    fileToDelete = newTextList();
}

void pushTmpFile(const char* tmpf)
{
    pushText(fileToDelete, tmpf);
}

const char* popFileToDelete()
{
    return popText(fileToDelete);
}

static const char* tmpf_base[MAX_TMPF_TYPE] = {
    "tmp",
    "src",
    "frame",
    "cache",
    "cookie",
    "hist",
};
static unsigned int tmpf_seq[MAX_TMPF_TYPE];

Str tmpfname(int CurrentPid, enum TmpFileType type, const char* ext)
{
    Str tmpf = Sprintf("%s/w3m%s%d-%d%s",
        type == TMPF_HIST ? rc_dir : tmp_dir,
        tmpf_base[type],
        CurrentPid, tmpf_seq[type]++, (ext) ? ext : "");
    pushText(fileToDelete, tmpf->ptr);
    return tmpf;
}

void cleanup_line(Str s, enum LineMode mode)
{
    if (s->length >= 2 && s->ptr[s->length - 2] == '\r' && s->ptr[s->length - 1] == '\n') {
        Strshrink(s, 2);
        Strcat_char(s, '\n');
    } else if (Strlastchar(s) == '\r')
        s->ptr[s->length - 1] = '\n';
    else if (Strlastchar(s) != '\n')
        Strcat_char(s, '\n');
    if (mode != PAGER_MODE) {
        int i;
        for (i = 0; i < s->length; i++) {
            if (s->ptr[i] == '\0')
                s->ptr[i] = ' ';
        }
    }
}

/*
 * convert line
 */
Str convertLine(bool do_chop, Str line, int mode, wc_ces* charset, wc_ces doc_charset)
{
    line = wc_Str_conv_with_detect(line, charset, doc_charset, InnerCharset);
    if (mode != RAW_MODE)
        cleanup_line(line, mode);
    // if (uf && uf->scheme == SCM_NEWS)
    if (do_chop)
        Strchop(line);
    return line;
}

char* url_unquote_conv(const char* url, wc_ces charset)
{
    wc_uint8 old_auto_detect = WcOption.auto_detect;
    Str tmp;
    tmp = Str_url_unquote(Strnew_charp(url), FALSE, TRUE);
    if (!charset || charset == WC_CES_US_ASCII)
        charset = SystemCharset;
    WcOption.auto_detect = WC_OPT_DETECT_ON;
    tmp = convertLine(false, tmp, RAW_MODE, &charset, charset);
    WcOption.auto_detect = old_auto_detect;
    return tmp->ptr;
}

