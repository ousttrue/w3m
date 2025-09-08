#include "mimetypes.h"
#include "expandpath.h"
#include <Str.h>
#include <alloc.h>
#include <myctype.h>
#include <stdio.h>
#include <string.h>

#define SYS_MIMETYPES ETC_DIR "/mime.types"
#define USER_MIMETYPES "~/.mime.types"

char* mimetypes_files = (USER_MIMETYPES ", " SYS_MIMETYPES);

struct table2 {
    char* item1;
    char* item2;
};

static struct table2 DefaultGuess[] = {
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
    { NULL, NULL }
};

static char*
guessContentTypeFromTable(struct table2* table, char* filename)
{
    struct table2* t;
    char* p;
    if (table == NULL)
        return NULL;
    p = &filename[strlen(filename) - 1];
    while (filename < p && *p != '.')
        p--;
    if (p == filename)
        return NULL;
    p++;
    for (t = table; t->item1; t++) {
        if (!strcmp(p, t->item1))
            return t->item2;
    }
    for (t = table; t->item1; t++) {
        if (!strcasecmp(p, t->item1))
            return t->item2;
    }
    return NULL;
}

static struct table2*
loadMimeTypes(const char* filename)
{
    char* type;
    int i;

    FILE* f = fopen(expandPath(filename), "r");
    if (f == NULL)
        return NULL;

    int n = 0;
    Str tmp;
    while (tmp = Strfgets(f), tmp->length > 0) {
        char* d = tmp->ptr;
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
    struct table2* mtypes;
    mtypes = New_N(struct table2, n + 1);
    i = 0;
    while (tmp = Strfgets(f), tmp->length > 0) {
        char* d = tmp->ptr;
        if (d[0] == '#')
            continue;
        type = strtok(d, " \t\n\r");
        if (type == NULL)
            continue;
        while (1) {
            d = strtok(NULL, " \t\n\r");
            if (d == NULL)
                break;
            mtypes[i].item1 = Strnew_charp(d)->ptr;
            mtypes[i].item2 = Strnew_charp(type)->ptr;
            i++;
        }
    }
    mtypes[i].item1 = NULL;
    mtypes[i].item2 = NULL;
    fclose(f);
    return mtypes;
}

static struct table2** UserMimeTypes;

static TextList* mimetypes_list;

void initMimeTypes(void)
{
    int i;
    TextListItem* tl;

    if (non_null(mimetypes_files))
        mimetypes_list = make_domain_list(mimetypes_files);
    else
        mimetypes_list = 0;
    if (mimetypes_list == 0)
        return;
    UserMimeTypes = New_N(struct table2*, mimetypes_list->nitem);
    for (i = 0, tl = mimetypes_list->first; tl; i++, tl = tl->next)
        UserMimeTypes[i] = loadMimeTypes(tl->ptr);
}

const char* guessContentType(const char* filename)
{
    char* ret;
    int i;

    if (filename == NULL)
        return NULL;
    if (mimetypes_list == NULL)
        goto no_user_mimetypes;

    for (i = 0; i < mimetypes_list->nitem; i++) {
        if ((ret = guessContentTypeFromTable(UserMimeTypes[i], filename)) != NULL)
            return ret;
    }

no_user_mimetypes:
    return guessContentTypeFromTable(DefaultGuess, filename);
}
