#include "mimetype.h"
#include "textlist.h"
#include "indep.h"
#include <gcstr/gcstr.h>
#include <string.h>

#define USER_MIMETYPES "~/.mime.types"
#define SYS_MIMETYPES ETC_DIR "/mime.types"
const char* mimetypes_files = (USER_MIMETYPES ", " SYS_MIMETYPES);

static TextList* mimetypes_list;

struct KeyValue {
    const char* item1;
    const char* item2;
};

static struct KeyValue** UserMimeTypes;

static struct KeyValue*
loadMimeTypes(char* filename)
{
    char *d, *type;
    int i, n;
    Str tmp;
    struct KeyValue* mtypes;

    FILE* f = fopen(expandPath(filename), "r");
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
    mtypes = New_N(struct KeyValue, n + 1);
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

void initMimeTypes(void)
{
    int i;
    TextListItem* tl;

    if (non_null(mimetypes_files))
        mimetypes_list = make_domain_list(mimetypes_files);
    else
        mimetypes_list = NULL;
    if (mimetypes_list == NULL)
        return;
    UserMimeTypes = New_N(struct KeyValue*, mimetypes_list->nitem);
    for (i = 0, tl = mimetypes_list->first; tl; i++, tl = tl->next)
        UserMimeTypes[i] = loadMimeTypes(tl->ptr);
}

static const char*
guessContentTypeFromTable(struct KeyValue* table, const char* filename)
{
    if (table == NULL)
        return NULL;
    const char* p = &filename[strlen(filename) - 1];
    while (filename < p && *p != '.')
        p--;
    if (p == filename)
        return NULL;
    p++;
    struct KeyValue* t;
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

static struct KeyValue DefaultGuess[] = {
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

const char* guessContentType(const char* filename)
{
    if (!filename)
        return NULL;
    if (mimetypes_list) {
        for (int i = 0; i < mimetypes_list->nitem; i++) {
            const char* ret = guessContentTypeFromTable(UserMimeTypes[i], filename);
            if (ret)
                return ret;
        }
    }
    return guessContentTypeFromTable(DefaultGuess, filename);
}
