#include "quote.h"
#include "regex.h"
#include "myctype.h"
#include "ctrlcode.h"
#include <stdio.h>

extern unsigned char QUOTE_MAP[];
enum QuoteMask GET_QUOTE_TYPE(unsigned char c) { return QUOTE_MAP[(int)(unsigned char)(c)]; }

static char xdigit[0x10] = "0123456789ABCDEF";

#define url_unquote_char(pstr) \
    ((IS_XDIGIT((*(pstr))[1]) && IS_XDIGIT((*(pstr))[2])) ? (*(pstr) += 3, (GET_MYCDIGIT((*(pstr))[-2]) << 4) | GET_MYCDIGIT((*(pstr))[-1])) : -1)

const char* remove_space(const char* str)
{
    const char *p, *q;

    for (p = str; *p && IS_SPACE(*p); p++)
        ;
    for (q = p; *q; q++)
        ;
    for (; q > p && IS_SPACE(*(q - 1)); q--)
        ;
    if (*q != '\0')
        return Strnew_charp_n(p, q - p)->ptr;
    return p;
}

const char* url_quote(const char* str)
{
    Str tmp = NULL;
    const char* p;
    for (p = str; *p; p++) {
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
    if (tmp)
        return tmp->ptr;
    return str;
}

const char* file_quote(const char* str)
{
    Str tmp = NULL;
    const char* p;
    for (p = str; *p; p++) {
        if (is_file_quote(*p)) {
            if (tmp == NULL)
                tmp = Strnew_charp_n(str, (int)(p - str));
            char buf[4];
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

const char* shell_quote(const char* str)
{
    Str tmp = NULL;
    for (const char* p = str; *p; p++) {
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

const char* getWord(const char** str)
{
    const char* p = *str;
    SKIP_BLANKS(p);

    const char* s;
    for (s = p; *p && !IS_SPACE(*p) && *p != ';'; p++)
        ;
    *str = p;
    return Strnew_charp_n(s, p - s)->ptr;
}

const char* getQWord(const char** str)
{
    Str tmp = Strnew();
    const char* p;
    int in_q = 0, in_dq = 0, esc = 0;

    p = *str;
    SKIP_BLANKS(p);
    for (; *p; p++) {
        if (esc) {
            if (in_q) {
                if (*p != '\\' && *p != '\'') /* '..\\..', '..\'..' */
                    Strcat_char(tmp, '\\');
            } else if (in_dq) {
                if (*p != '\\' && *p != '"') /* "..\\..", "..\".." */
                    Strcat_char(tmp, '\\');
            } else {
                if (*p != '\\' && *p != '\'' && /* ..\\.., ..\'.. */
                    *p != '"' && !IS_SPACE(*p)) /* ..\".., ..\.. */
                    Strcat_char(tmp, '\\');
            }
            Strcat_char(tmp, *p);
            esc = 0;
        } else if (*p == '\\') {
            esc = 1;
        } else if (in_q) {
            if (*p == '\'')
                in_q = 0;
            else
                Strcat_char(tmp, *p);
        } else if (in_dq) {
            if (*p == '"')
                in_dq = 0;
            else
                Strcat_char(tmp, *p);
        } else if (*p == '\'') {
            in_q = 1;
        } else if (*p == '"') {
            in_dq = 1;
        } else if (IS_SPACE(*p) || *p == ';') {
            break;
        } else {
            Strcat_char(tmp, *p);
        }
    }
    *str = p;
    return tmp->ptr;
}

/* This extracts /regex/i or m@regex@i from the given string.
 * Then advances *str to the end of regex.
 * If the input does not seems to be a regex, this falls back to getQWord().
 *
 * Returns a word (no matter whether regex or not) in the give string.
 * If regex_ret is non-NULL, compiles the regex and stores there.
 *
 * XXX: Actually this is unrelated to func.c.
 */
const char* getRegexWord(const char** str, Regex** regex_ret)
{
    const char* word = NULL;
    const char *p, *headp, *bodyp, *tailp;
    char delimiter;
    int esc;
    int igncase = 0;

    p = *str;
    SKIP_BLANKS(p);
    headp = p;

    /* Get the opening delimiter */
    if (p[0] == 'm' && IS_PRINT(p[1]) && !IS_ALNUM(p[1]) && p[1] != '\\') {
        delimiter = p[1];
        p += 2;
    } else if (p[0] == '/') {
        delimiter = '/';
        p += 1;
    } else {
        goto not_regex;
    }
    bodyp = p;

    /* Scan the end of the expression */
    for (esc = 0; *p; ++p) {
        if (esc) {
            esc = 0;
        } else {
            if (*p == delimiter)
                break;
            else if (*p == '\\')
                esc = 1;
        }
    }
    if (!*p && *headp == '/')
        goto not_regex;
    tailp = p;

    /* Check the modifiers */
    if (*p == delimiter) {
        while (*++p && !IS_SPACE(*p)) {
            switch (*p) {
            case 'i':
                igncase = 1;
                break;
            }
            /* ignore unknown modifiers */
        }
    }

    /* Save the expression */
    word = allocStr(headp, p - headp);

    /* Compile */
    if (regex_ret) {
        if (*tailp == delimiter)
            ((char*)word)[tailp - headp] = 0;
        *regex_ret = newRegex(word + (bodyp - headp), igncase, NULL, NULL);
        if (*tailp == delimiter)
            ((char*)word)[tailp - headp] = delimiter;
    }
    goto last;

not_regex:
    p = headp;
    word = getQWord(&p);
    if (regex_ret)
        *regex_ret = NULL;

last:
    *str = p;
    return word;
}

const char* file_unquote(const char* str)
{
    Str tmp = NULL;
    for (const char* p = str; *p;) {
        if (*p == '%') {
            const char* q = p;
            int c = url_unquote_char(&q);
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

const char* mybasename(const char* s)
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
    return allocStr(p, -1);
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

Str escape_spaces(Str s)
{
    if (!s)
        return s;

    Str tmp = 0;
    char* p;
    for (p = s->ptr; *p; p++) {
        if (*p == ' ' || *p == CTRL_I) {
            if (!tmp)
                tmp = Strnew_charp_n(s->ptr, (int)(p - s->ptr));
            Strcat_char(tmp, '\\');
        }
        if (tmp)
            Strcat_char(tmp, *p);
    }
    if (tmp)
        return tmp;
    return s;
}

Str unescape_spaces(Str s)
{
    if (!s)
        return s;

    Str tmp = 0;
    char* p;
    for (p = s->ptr; *p; p++) {
        if (*p == '\\' && (*(p + 1) == ' ' || *(p + 1) == CTRL_I)) {
            if (!tmp)
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
