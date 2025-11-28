#include "KeyValueList.h"
#include <strings.h>
#include <gcstr.h>
#include "indep.h"
// #include "parsetag.h"
// #include <string.h>

const char* tag_get_value(struct KeyValueList* t, const char* arg)
{
    for (; t; t = t->next) {
        if (!strcasecmp(t->arg, arg))
            return t->value;
    }
    return 0;
}

bool tag_exists(struct KeyValueList* t, const char* arg)
{
    for (; t; t = t->next) {
        if (!strcasecmp(t->arg, arg))
            return 1;
    }
    return 0;
}

struct KeyValueList*
cgistr2tagarg(const char* cgistr)
{
    Str tag;
    Str value;
    struct KeyValueList* t0 = 0;
    struct KeyValueList* t = 0;
    do {
        t = New(struct KeyValueList);
        t->next = t0;
        t0 = t;
        tag = Strnew();
        while (*cgistr && *cgistr != '=' && *cgistr != '&')
            Strcat_char(tag, *cgistr++);
        t->arg = Str_form_unquote(tag)->ptr;
        t->value = NULL;
        if (*cgistr == '\0')
            return t;
        else if (*cgistr == '=') {
            cgistr++;
            value = Strnew();
            while (*cgistr && *cgistr != '&')
                Strcat_char(value, *cgistr++);
            t->value = Str_form_unquote(value)->ptr;
        } else if (*cgistr == '&')
            cgistr++;
    } while (*cgistr);
    return t;
}
