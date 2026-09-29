/* vi: set sw=4 ts=8 ai sm noet : */
#include "indep.h"

#include "Str.h"
#include "alloc.h"
#include "config.h"
#include "entity.h"
#include "fm.h"
#include "proto.h"
#include "myctype.h"
#include "pathdefs.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/param.h>
#include <sys/types.h>

char* remove_space(char* str)
{
    char *p, *q;

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

/* Parse an HTML entity.  Returns NULL on failure and a string on success.
 * *str is set to the last byte parsed both on success and failure.
 * is_attr produces stricter processing of `;' for attribute values.
 * If psimple is not NULL, it is set when the entity is single-byte and
 * maps to itself in conv_entity (i.e. it can be displayed).
 */
char* getescapestr(char** str, int is_attr, int* psimple)
{
    char *p = *str, *res;
    unsigned long ucs;
    int i, last_match_idx, overflow;
    const struct entity_item *item, *last_match, *entity_end;

    if (*p == '&')
        p++;
    if (*p == '#') {
        p++;
        overflow = 0;
        if (*p == 'x' || *p == 'X') {
            p++;
            if (!IS_XDIGIT(*p))
                goto fail;
            for (ucs = GET_MYCDIGIT(*p), p++; IS_XDIGIT(*p); p++) {
                ucs = ucs * 0x10 + GET_MYCDIGIT(*p);
                if (ucs > 0x10FFFF)
                    overflow = 1;
            }
        } else {
            if (!IS_DIGIT(*p))
                goto fail;
            for (ucs = GET_MYCDIGIT(*p), p++; IS_DIGIT(*p); p++) {
                ucs = ucs * 10 + GET_MYCDIGIT(*p);
                if (ucs > 0x10FFFF)
                    overflow = 1;
            }
        }
        if (*p == ';')
            p++;
        *str = p;
        if (ucs == 0 || overflow || (ucs >= 0xD800 && ucs <= 0xDFFF))
            ucs = 0xFFFD; /* HTML5 behavior for invalid numeric entities */
    } else {
        if (!IS_ALPHA(*p))
            goto fail;
        item = &entity[entity_char_start[*p - 'A']];
        last_match = NULL;
        last_match_idx = -1;
        entity_end = entity + sizeof(entity) / sizeof(entity[0]);
        for (i = 1; p[i] != '\0'; i++) {
            if (item->name[i] == p[i])
                continue; /* current entry matches */
            if (!item->name[i]) {
                /* Found match; save it for the case where there isn't
                 * anything better. */
                last_match = item;
                last_match_idx = i;
            }
            /* Cycle to the next entry that could match.
             * We want to look at all entries that prefix match (0, i - 1). */
            item++;
            while (1) {
                if (item < entity_end && !strncmp(p, item->name, i)) {
                    if (item->name[i] == p[i])
                        break; /* found match */
                    item++; /* try next */
                } else {
                    /* out of entries */
                    item = NULL;
                    goto done;
                }
            }
        }
    done:
        if (!item || item->name[i]) {
            /* partial match */
            if (!last_match)
                goto fail;
            item = last_match;
            i = last_match_idx;
        }
        if (item->name[i - 1] != ';') {
            /* In HTML5, some character entities such as &lt; &gt; can be
             * written without the semicolon (like &gt or &lt).  We encode
             * these by omitting the semicolon, and then optionally skip it
             * in the input stream here.
             *
             * (Attributes have stricter processing so that &lt=, &gt=,
             * etc. are not regarded as character entities.)
             */
            if (p[i] == ';') /* item allows skipping the last ";"*/
                i++;
            else if (is_attr && (p[i] == '=' || IS_ALNUM(p[i])))
                goto fail;
        }
        *str = p + i;
        ucs = item->unit1;
        if (item->unit2) {
            if (!(ucs >= 0xD800 && ucs <= 0xDBFF)) { /* two codepoints */
                char* a = conv_entity(ucs);
                char* b = conv_entity(item->unit2);
                if (psimple)
                    *psimple = FALSE;
                return Strnew_m_charp(a, b, NULL)->ptr;
            }
            /* two surrogates */
            ucs = 0x10000 | ((ucs - 0xD800) << 10) | (item->unit2 - 0xDC00);
        }
    }
    res = conv_entity(ucs);
    if (psimple)
        *psimple = (ucs == (unsigned char)res[0]) && !res[1];
    return res;
fail:
    *str = p;
    return NULL;
}

static char*
getescapecmd_impl(char** s, int is_attr)
{
    char* save = *s;
    Str tmp;
    char* value = getescapestr(s, is_attr, NULL);

    if (value)
        return value;

    if (*save != '&')
        tmp = Strnew_charp("&");
    else
        tmp = Strnew();
    Strcat_charp_n(tmp, save, *s - save);
    return tmp->ptr;
}

char* getescapecmd(char** s)
{
    return getescapecmd_impl(s, FALSE);
}

char* html_quote(char* str)
{
    Str tmp = NULL;
    char *p, *q;

    for (p = str; *p; p++) {
        q = html_quote_char(*p);
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

static char*
html_unquote_impl(char* str, int is_attr)
{
    Str tmp = NULL;
    char *p, *q;

    for (p = str; *p;) {
        if (*p == '&') {
            if (tmp == NULL)
                tmp = Strnew_charp_n(str, (int)(p - str));
            q = getescapecmd_impl(&p, is_attr);
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

char* html_unquote(char* str)
{
    return html_unquote_impl(str, FALSE);
}

char* html_unquote_attr(char* str)
{
    return html_unquote_impl(str, TRUE);
}

static const char xdigit[0x10] = "0123456789ABCDEF";

#define url_unquote_char(pstr) \
    ((IS_XDIGIT((*(pstr))[1]) && IS_XDIGIT((*(pstr))[2])) ? (*(pstr) += 3, (GET_MYCDIGIT((*(pstr))[-2]) << 4) | GET_MYCDIGIT((*(pstr))[-1])) : -1)

char* url_quote(char* str)
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

char* file_unquote(char* str)
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

const char*
shell_quote(const char* str)
{
    Str tmp = NULL;
    const char* p;

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

