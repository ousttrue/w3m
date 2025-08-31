#include "quote.h"

#include "indep.h"
#include "regex.h"
#include <myctype.h>

char* getWord(char** str)
{
    char* p = *str;
    SKIP_BLANKS(p);

    char* s;
    for (s = p; *p && !IS_SPACE(*p) && *p != ';'; p++)
        ;
    *str = p;
    return Strnew_charp_n(s, p - s)->ptr;
}

char* getQWord(char** str)
{
    Str tmp = Strnew();
    char* p;
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
char* getRegexWord(const char** str, Regex** regex_ret)
{
    char* word = NULL;
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
            word[tailp - headp] = 0;
        *regex_ret = newRegex(word + (bodyp - headp), igncase, NULL, NULL);
        if (*tailp == delimiter)
            word[tailp - headp] = delimiter;
    }
    goto last;

not_regex:
    p = headp;
    word = getQWord((char**)&p);
    if (regex_ret)
        *regex_ret = NULL;

last:
    *str = p;
    return word;
}
