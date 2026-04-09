#include "content_type.h"
#include "global.h"
#include "textlist.h"
#include "indep.h"
#include "alloc.h"
#include <strings.h>

#ifndef NULL
#define NULL 0
#endif

static TextList* mimetypes_list = 0;
static struct ext_map** UserMimeTypes = 0;

// struct table2 {
//     char* item1;
//     char* item2;
// };
struct ext_map {
    const char* ext;
    const char* content_type;
};

static struct ext_map DefaultGuess[] = {
    { "html", "text/html" },
    { "htm", "text/html" },
    { "shtml", "text/html" },
    { "xhtml", "application/xhtml+xml" },
    { "gif", "image/gif" },
    { "jpeg", "image/jpeg" },
    { "jpg", "image/jpeg" },
    { "png", "image/png" },
    { "xbm", "image/xbm" },
    { "au", "audio/basic" },
    { "gz", "application/x-gzip" },
    { "Z", "application/x-compress" },
    { "bz2", "application/x-bzip" },
    { "tar", "application/x-tar" },
    { "zip", "application/x-zip" },
    { "lha", "application/x-lha" },
    { "lzh", "application/x-lha" },
    { "ps", "application/postscript" },
    { "pdf", "application/pdf" },
    { 0, 0 }
};

static struct ext_map* loadMimeTypes(char* filename)
{
    FILE* f;
    char *d, *type;
    int i, n;
    Str tmp;

    f = fopen(expandPath(filename), "r");
    if (f == NULL)
        return NULL;
    n = 0;
    while (tmp = Strfgets(f), tmp->length > 0) {
        d = tmp->ptr;
        if (d[0] != '#') {
            d = strtok(d, " \t\n\r");
            if (d != NULL) {
                d = strtok(NULL, " \t\n\r");
                for (i = 0; d != NULL; i++)
                    d = strtok(NULL, " \t\n\r");
                n += i;
            }
        }
    }
    fseek(f, 0, 0);
    struct ext_map* mtypes = New_N(struct ext_map, n + 1);
    i = 0;
    while (tmp = Strfgets(f), tmp->length > 0) {
        d = tmp->ptr;
        if (d[0] == '#')
            continue;
        type = strtok(d, " \t\n\r");
        if (type == NULL)
            continue;
        while (1) {
            d = strtok(NULL, " \t\n\r");
            if (d == NULL)
                break;
            mtypes[i].ext = Strnew_charp(d)->ptr;
            mtypes[i].content_type = Strnew_charp(type)->ptr;
            i++;
        }
    }
    mtypes[i].ext = NULL;
    mtypes[i].content_type = NULL;
    fclose(f);
    return mtypes;
}

void initMimeTypes(void)
{
    if (non_null(mimetypes_files))
        mimetypes_list = make_domain_list(mimetypes_files);
    else
        mimetypes_list = NULL;
    if (mimetypes_list == NULL)
        return;
    UserMimeTypes = New_N(struct ext_map*, mimetypes_list->nitem);
    int i = 0;
    for (TextListItem* tl = mimetypes_list->first; tl; i++, tl = tl->next)
        UserMimeTypes[i] = loadMimeTypes(tl->ptr);
}

static const char* guessContentTypeFromTable(struct ext_map* table, const char* filename)
{
    if (table == NULL)
        return NULL;
    const char* p = &filename[strlen(filename) - 1];
    while (filename < p && *p != '.')
        p--;
    if (p == filename)
        return NULL;
    p++;
    struct ext_map* t;
    for (t = table; t->ext; t++) {
        if (0 == strcasecmp(p, t->ext))
            return t->content_type;
    }
    return NULL;
}

const char* guessContentType(const char* filename)
{
    if (filename == NULL)
        return NULL;
    if (mimetypes_list == NULL)
        goto no_user_mimetypes;

    for (int i = 0; i < mimetypes_list->nitem; i++) {
        const char* ret;
        if ((ret = guessContentTypeFromTable(UserMimeTypes[i], filename)) != NULL)
            return ret;
    }

no_user_mimetypes:
    return guessContentTypeFromTable(DefaultGuess, filename);
}
