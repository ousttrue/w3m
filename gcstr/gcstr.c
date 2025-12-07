#include "gcstr.h"
#include "entity.h"
#include <math.h>
#include <string.h>

wc_ces InnerCharset = WC_CES_WTF; /* Don't change */

#define SP_NORMAL 0
#define SP_PREC 1
#define SP_PREC2 2

int vscpf(const char* fmt, va_list ap)
{
    int len = 0;
    int status = SP_NORMAL;
    int p = 0;
    for (const char* f = fmt; *f; f++) {
    redo:
        switch (status) {
        case SP_NORMAL:
            if (*f == '%') {
                status = SP_PREC;
                p = 0;
            } else
                len++;
            break;
        case SP_PREC:
            if (IS_ALPHA(*f)) {
                /* conversion char. */
                int vi;
                char* vs;

                switch (*f) {
                case 'l':
                case 'h':
                case 'L':
                case 'w':
                    continue;
                case 'd':
                case 'i':
                case 'o':
                case 'x':
                case 'X':
                case 'u':
                    vi = va_arg(ap, int);
                    len += (p > 0) ? p : 10;
                    break;
                case 'f':
                case 'g':
                case 'e':
                case 'G':
                case 'E':
                    va_arg(ap, double);
                    len += (p > 0) ? p : 15;
                    break;
                case 'c':
                    len += 1;
                    vi = va_arg(ap, int);
                    break;
                case 's':
                    vs = va_arg(ap, char*);
                    vi = strlen(vs);
                    len += (p > vi) ? p : vi;
                    break;
                case 'p':
                    va_arg(ap, void*);
                    len += 10;
                    break;
                case 'n':
                    va_arg(ap, void*);
                    break;
                }
                status = SP_NORMAL;
            } else if (IS_DIGIT(*f))
                p = p * 10 + *f - '0';
            else if (*f == '.')
                status = SP_PREC2;
            else if (*f == '%') {
                status = SP_NORMAL;
                len++;
            }
            break;
        case SP_PREC2:
            if (IS_ALPHA(*f)) {
                status = SP_PREC;
                goto redo;
            }
            break;
        }
    }

    return len;
}

static char Base64Table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

Str base64_encode(const char* src, size_t len)
{
    Str dest;
    const unsigned char *in, *endw, *s;
    unsigned long j;
    size_t k;

    s = (unsigned char*)src;

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
    Strnulterm(dest);
    return dest;
}

static const char* _size_unit[] = { "b", "kb", "Mb", "Gb", "Tb",
    "Pb", "Eb", "Zb", "Bb", "Yb", NULL };

Str convert_size(long long size, bool usefloat)
{
    const char** sizes = _size_unit;
    float csize = (float)size;
    int sizepos = 0;
    while (csize >= 999.495 && sizes[sizepos + 1]) {
        csize = csize / 1024.0;
        sizepos++;
    }
    return Sprintf(usefloat ? "%.3g%s" : "%.0f%s",
        floor(csize * 100.0 + 0.5) / 100.0, sizes[sizepos]);
}

Str convert_size2(long long size1, long long size2, bool usefloat)
{
    const char** sizes = _size_unit;
    float csize = (float)((size1 > size2) ? size1 : size2);
    float factor = 1;
    int sizepos = 0;
    while (csize / factor >= 999.495 && sizes[sizepos + 1]) {
        factor *= 1024.0;
        sizepos++;
    }
    return Sprintf(usefloat ? "%.3g/%.3g%s" : "%.0f/%.0f%s",
        floor(size1 / factor * 100.0 + 0.5) / 100.0,
        floor(size2 / factor * 100.0 + 0.5) / 100.0,
        sizes[sizepos]);
}

bool matchattr(const char* p, const char* attr, int len, Str* value)
{
    int quoted;
    char* q = NULL;

    if (strncasecmp(p, attr, len) == 0) {
        p += len;
        SKIP_BLANKS(&p);
        if (value) {
            *value = Strnew();
            if (*p == '=') {
                p++;
                SKIP_BLANKS(&p);
                quoted = 0;
                while (!IS_ENDL(*p) && (quoted || *p != ';')) {
                    if (!IS_SPACE(*p))
                        q = p;
                    if (*p == '"')
                        quoted = (quoted) ? 0 : 1;
                    else
                        Strcat_char(*value, *p);
                    p++;
                }
                if (q)
                    Strshrink(*value, p - q - 1);
            }
            return 1;
        } else {
            if (IS_ENDT(*p)) {
                return 1;
            }
        }
    }
    return 0;
}

Str remove_space(const char* str)
{
    const char *p, *q;
    for (p = str; *p && IS_SPACE(*p); p++)
        ;
    for (q = p; *q; q++)
        ;
    for (; q > p && IS_SPACE(*(q - 1)); q--)
        ;
    if (*q != '\0')
        return Strnew_charp_n(p, q - p);
    return Strnew_charp(p);
}

Str mybasename(const char* s)
{
    const char* p = s;
    while (*p)
        p++;
    while (s <= p && *p != '/')
        p--;
    if (*p == '/')
        p++;
    else
        p = s;
    return Strnew_charp(p);
}

#define DEF_SAVE_FILE "index.html"

Str guess_filename(const char* file)
{
    Str s = NULL;
    if (file != NULL)
        s = mybasename(file);
    if (s == NULL || s->length == 0)
        return Strnew_charp(DEF_SAVE_FILE);

    char* p = s->ptr;
    if (*p == '#')
        p++;
    while (*p != '\0') {
        if ((*p == '#' && *(p + 1) != '\0') || *p == '?') {
            *p = '\0';
            break;
        }
        p++;
    }
    return s;
}

Str unescape_spaces(Str s)
{
    Str tmp = NULL;
    char* p;

    if (s == NULL)
        return s;
    for (p = s->ptr; *p; p++) {
        if (*p == '\\' && (*(p + 1) == ' ' || *(p + 1) == CTRL_I)) {
            if (tmp == NULL)
                tmp = Strnew_charp_n(s->ptr, (int)(p - s->ptr));
        } else {
            if (tmp)
                Strcat_char(tmp, *p);
        }
    }
    if (tmp)
        return tmp;
    return s;
}

Str lastFileName(const char* path)
{
    const char* p = path;
    const char* q = path;
    while (*p != '\0') {
        if (*p == '/')
            q = p + 1;
        p++;
    }
    return Strnew_charp(q);
}

Str url_quote(const char* str)
{
    static const char xdigit[0x10] = "0123456789ABCDEF";
    Str tmp = NULL;
    for (const char* p = str; *p; p++) {
        if (is_url_quote(*p)) {
            if (tmp == NULL)
                tmp = Strnew_charp_n(str, (int)(p - str));
            Strcat_char(tmp, '%');
            Strcat_char(tmp, xdigit[((unsigned char)*p >> 4) & 0xF]);
            Strcat_char(tmp, xdigit[(unsigned char)*p & 0xF]);
        } else {
            if (tmp)
                Strcat_char(tmp, *p);
        }
    }
    return tmp ? tmp : Strnew_charp(str);
}

Str url_quote_conv(const char* x, wc_ces c)
{
    return url_quote(wc_conv_strict((x), InnerCharset, (c))->ptr);
}

char* html_quote(const char* str)
{
    Str tmp = NULL;
    char* p;
    for (p = str; *p; p++) {
        const char* q = html_quote_char(*p);
        if (q) {
            if (tmp == NULL)
                tmp = Strnew_charp_n(str, (int)(p - str));
            Strcat_charp(tmp, q);
        } else {
            if (tmp)
                Strcat_char(tmp, *p);
        }
    }
    if (tmp)
        return tmp->ptr;
    return str;
}

char* html_unquote(char* str)
{
    Str tmp = NULL;
    char *p;
    for (p = str; *p;) {
        if (*p == '&') {
            if (tmp == NULL)
                tmp = Strnew_charp_n(str, (int)(p - str));
            const char* q = getescapecmd(&p);
            Strcat_charp(tmp, q);
        } else {
            if (tmp)
                Strcat_char(tmp, *p);
            p++;
        }
    }

    if (tmp)
        return tmp->ptr;
    return str;
}

#define url_unquote_char(pstr) \
    ((IS_XDIGIT((*(pstr))[1]) && IS_XDIGIT((*(pstr))[2])) ? (*(pstr) += 3, (GET_MYCDIGIT((*(pstr))[-2]) << 4) | GET_MYCDIGIT((*(pstr))[-1])) : -1)

char* file_quote(char* str)
{
    Str tmp = NULL;
    char* p;
    char buf[4];

    for (p = str; *p; p++) {
        if (is_file_quote(*p)) {
            if (tmp == NULL)
                tmp = Strnew_charp_n(str, (int)(p - str));
            sprintf(buf, "%%%02X", (unsigned char)*p);
            Strcat_charp(tmp, buf);
        } else {
            if (tmp)
                Strcat_char(tmp, *p);
        }
    }
    if (tmp)
        return tmp->ptr;
    return str;
}

char* file_unquote(const char* str)
{
    Str tmp = NULL;
    char *p, *q;
    int c;

    for (p = str; *p;) {
        if (*p == '%') {
            q = p;
            c = url_unquote_char(&q);
            if (c >= 0) {
                if (tmp == NULL)
                    tmp = Strnew_charp_n(str, (int)(p - str));
                if (c != '\0' && c != '\n' && c != '\r')
                    Strcat_char(tmp, (char)c);
                p = q;
                continue;
            }
        }
        if (tmp)
            Strcat_char(tmp, *p);
        p++;
    }
    if (tmp)
        return tmp->ptr;
    return str;
}

Str Str_form_quote(Str x)
{
    Str tmp = NULL;
    char *p = x->ptr, *ep = x->ptr + x->length;
    char buf[4];

    for (; p < ep; p++) {
        if (*p == ' ') {
            if (tmp == NULL)
                tmp = Strnew_charp_n(x->ptr, (int)(p - x->ptr));
            Strcat_char(tmp, '+');
        } else if (is_url_unsafe(*p)) {
            if (tmp == NULL)
                tmp = Strnew_charp_n(x->ptr, (int)(p - x->ptr));
            sprintf(buf, "%%%02X", (unsigned char)*p);
            Strcat_charp(tmp, buf);
        } else {
            if (tmp)
                Strcat_char(tmp, *p);
        }
    }
    if (tmp)
        return tmp;
    return x;
}

Str Str_url_unquote(Str x, int is_form, int safe)
{
    Str tmp = NULL;
    char *p = x->ptr, *ep = x->ptr + x->length, *q;
    int c;

    for (; p < ep;) {
        if (is_form && *p == '+') {
            if (tmp == NULL)
                tmp = Strnew_charp_n(x->ptr, (int)(p - x->ptr));
            Strcat_char(tmp, ' ');
            p++;
            continue;
        } else if (*p == '%') {
            q = p;
            c = url_unquote_char(&q);
            if (c >= 0 && (!safe || !IS_ASCII(c) || !is_file_quote(c))) {
                if (tmp == NULL)
                    tmp = Strnew_charp_n(x->ptr, (int)(p - x->ptr));
                Strcat_char(tmp, (char)c);
                p = q;
                continue;
            }
        }
        if (tmp)
            Strcat_char(tmp, *p);
        p++;
    }
    if (tmp)
        return tmp;
    return x;
}

char* shell_quote(char* str)
{
    Str tmp = NULL;
    char* p;

    for (p = str; *p; p++) {
        if (is_shell_unsafe(*p)) {
            if (tmp == NULL)
                tmp = Strnew_charp_n(str, (int)(p - str));
            Strcat_char(tmp, '\\');
            Strcat_char(tmp, *p);
        } else {
            if (tmp)
                Strcat_char(tmp, *p);
        }
    }
    if (tmp)
        return tmp->ptr;
    return str;
}


