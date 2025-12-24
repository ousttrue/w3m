#include "content.h"
#include "etc.h"
#include "indep.h"
#include "myctype.h"
#include <string.h>

bool matchattr(const char* p, const char* attr, int len, Str* value)
{
    int quoted;
    const char* q = NULL;
    if (strncasecmp(p, attr, len) == 0) {
        p += len;
        p = skip_blanks(p);
        if (value) {
            *value = Strnew();
            if (*p == '=') {
                p++;
                p = skip_blanks(p);
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

const char* checkHeader(struct Content content, const char* field)
{
    if (field == NULL || content.document_header == NULL)
        return NULL;

    int len = strlen(field);
    for (TextListItem* i = content.document_header->first; i != NULL; i = i->next) {
        if (!strncasecmp(i->ptr, field, len)) {
            char* p = i->ptr + len;
            return remove_space(p);
        }
    }
    return NULL;
}

const char* guess_filename(const char* file)
{
    char *p = NULL, *s;

    if (file != NULL)
        p = mybasename(file);
    if (p == NULL || *p == '\0')
        return DEF_SAVE_FILE;
    s = p;
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

const char* guess_save_name(struct Content content, const char* path)
{
    if (content.document_header) {
        Str name = NULL;
        const char *p, *q;
        if ((p = checkHeader(content, "Content-Disposition:")) != NULL && (q = strcasestr(p, "filename")) != NULL && (q == p || IS_SPACE(*(q - 1)) || *(q - 1) == ';') && matchattr(q, "filename", 8, &name))
            path = name->ptr;
        else if ((p = checkHeader(content, "Content-Type:")) != NULL && (q = strcasestr(p, "name")) != NULL && (q == p || IS_SPACE(*(q - 1)) || *(q - 1) == ';') && matchattr(q, "name", 4, &name))
            path = name->ptr;
    }
    return guess_filename(path);
}
