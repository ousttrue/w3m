#include "ContentType.h"
#include "http_message.h"
#include "myctype.h"
#include "runtime.h"
#include "str_util.h"
#include <string.h>

#define SYS_MIMETYPES ETC_DIR "/mime.types"
#define USER_MIMETYPES "~/.mime.types"

const char* mimetypes_files = (USER_MIMETYPES ", " SYS_MIMETYPES);
// static struct table2** UserMimeTypes;
// static TextList* mimetypes_list;

struct ContentTypeTable {
    enum ContentType content_type;
    const char* file_extension;
    const char* mimetype;
};

static struct ContentTypeTable CONTENTTYPE_TABLE[] = {
    { CONTENTTYPE_TEXT_HTML, "html", "text/html" },
    // { "htm", "text/html" },
    // { "shtml", "text/html" },
    // { "xhtml", "application/xhtml+xml" },
    // { "gif", "image/gif" },
    // { "jpeg", "image/jpeg" },
    // { "jpg", "image/jpeg" },
    { CONTENTTYPE_IMAGE_PNG, "png", "image/png" },
    // { "xbm", "image/xbm" },
    // { "au", "audio/basic" },
    // { "gz", "application/x-gzip" },
    // { "Z", "application/x-compress" },
    // { "bz2", "application/x-bzip" },
    // { "tar", "application/x-tar" },
    // { "zip", "application/x-zip" },
    // { "lha", "application/x-lha" },
    // { "lzh", "application/x-lha" },
    // { "ps", "application/postscript" },
    // { "pdf", "application/pdf" },
    { 0 },
};

static enum ContentType
guessContentTypeFromTable(const char* filename, struct ContentTypeTable* table)
{
    const char* p = &filename[strlen(filename) - 1];
    char ext[6] = { 0 };
    int ext_len = 1;
    for (; filename < p && *p != '.' && ext_len < sizeof(ext); --p, ++ext_len) {
        ext[sizeof(ext) - 1 - ext_len] = TOLOWER(*p);
    }
    if (p == filename || ext_len > 5) {
        return CONTENTTYPE_UNKNOWN;
    }
    const char* begin = ext + sizeof(ext) - ext_len;

    for (struct ContentTypeTable* t = table; t->file_extension; ++t) {
        if (strcmp(begin, t->file_extension) == 0) {
            return t->content_type;
        }
    }

    return CONTENTTYPE_UNKNOWN;
}

// static struct table2*
// loadMimeTypes(const char* filename)
// {
//     char* type;
//     int i;
//
//     FILE* f = fopen(expandPath(filename), "r");
//     if (f == NULL)
//         return NULL;
//
//     int n = 0;
//     Str tmp;
//     while (tmp = Strfgets(f), tmp->length > 0) {
//         char* d = tmp->ptr;
//         if (d[0] != '#') {
//             d = strtok(d, " \t\n\r");
//             if (d != NULL) {
//                 d = strtok(NULL, " \t\n\r");
//                 for (i = 0; d != NULL; i++)
//                     d = strtok(NULL, " \t\n\r");
//                 n += i;
//             }
//         }
//     }
//     fseek(f, 0, 0);
//     struct table2* mtypes;
//     mtypes = New_N(struct table2, n + 1);
//     i = 0;
//     while (tmp = Strfgets(f), tmp->length > 0) {
//         char* d = tmp->ptr;
//         if (d[0] == '#')
//             continue;
//         type = strtok(d, " \t\n\r");
//         if (type == NULL)
//             continue;
//         while (1) {
//             d = strtok(NULL, " \t\n\r");
//             if (d == NULL)
//                 break;
//             mtypes[i].item1 = Strnew_charp(d)->ptr;
//             mtypes[i].item2 = Strnew_charp(type)->ptr;
//             i++;
//         }
//     }
//     mtypes[i].item1 = NULL;
//     mtypes[i].item2 = NULL;
//     fclose(f);
//     return mtypes;
// }
//
// void initMimeTypes(void)
// {
//     int i;
//     TextListItem* tl;
//
//     if (non_null(mimetypes_files))
//         mimetypes_list = make_domain_list(mimetypes_files);
//     else
//         mimetypes_list = 0;
//     if (mimetypes_list == 0)
//         return;
//     UserMimeTypes = New_N(struct table2*, mimetypes_list->nitem);
//     for (i = 0, tl = mimetypes_list->first; tl; i++, tl = tl->next)
//         UserMimeTypes[i] = loadMimeTypes(tl->ptr);
// }

const char* contentTypeStr(enum ContentType content_type)
{
    for (struct ContentTypeTable* t = CONTENTTYPE_TABLE; t->file_extension; ++t) {
        if (content_type == t->content_type) {
            return t->mimetype;
        }
    }
    return "unknown";
}

enum ContentType guessContentType(const char* filename)
{
    // char* ret;
    // int i;
    //
    // if (filename == NULL)
    //     return NULL;
    // if (mimetypes_list == NULL)
    //     goto no_user_mimetypes;
    //
    // for (i = 0; i < mimetypes_list->nitem; i++) {
    //     if ((ret = guessContentTypeFromTable(UserMimeTypes[i], filename)) != NULL)
    //         return ret;
    // }

no_user_mimetypes:
    return guessContentTypeFromTable(filename, CONTENTTYPE_TABLE);
}

struct ContentTypeCharset getContentType(TextList* document_header)
{
    const char* p = getHttpHeaderValue(document_header, "Content-Type:");
    if (!p) {
        return (struct ContentTypeCharset) { 0 };
    }
    int len = 0;
    for (const char* pp = p; *pp && *pp != ';' && !IS_SPACE(*pp); ++pp, ++len)
        ;

    struct ContentTypeCharset content_type_charset = {
        .content_type = 0,
        .charset = 0
    };

    for (struct ContentTypeTable* t = CONTENTTYPE_TABLE; t->content_type; ++t) {
        if (strncmp(p, t->mimetype, len) == 0) {
            content_type_charset.content_type = t->content_type;
            break;
        }
    }

    if ((p = strcasestr(p, "charset")) != NULL) {
        p += 7;
        SKIP_BLANKS(p);
        if (*p == '=') {
            p++;
            SKIP_BLANKS(p);
            if (*p == '"')
                p++;
            content_type_charset.charset = wc_guess_charset(p, 0);
        }
    }

    return content_type_charset;
}

#define DEF_SAVE_FILE "index.html"

const char* guessFileName(const char* file)
{
    char* p = 0;
    if (file)
        p = allocStr(mybasename(file), -1);
    if (!p || *p == '\0')
        return DEF_SAVE_FILE;
    const char* s = p;
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

const char* guessSaveName(TextList* document_header, const char* path)
{
    if (document_header) {
        struct CharSlice name;
        const char *p, *q;
        if ((p = getHttpHeaderValue(document_header, "Content-Disposition:")) != NULL && (q = strcasestr(p, "filename")) != NULL && (q == p || IS_SPACE(*(q - 1)) || *(q - 1) == ';') && matchattr(q, (struct CharSlice) { "filename", 8 }, &name))
            path = Strnew_charp_n(name.p, name.len)->ptr;
        else if ((p = getHttpHeaderValue(document_header, "Content-Type:")) != NULL && (q = strcasestr(p, "name")) != NULL && (q == p || IS_SPACE(*(q - 1)) || *(q - 1) == ';') && matchattr(q, (struct CharSlice) { "name", 4 }, &name))
            path = Strnew_charp_n(name.p, name.len)->ptr;
    }
    return guessFileName(path);
}
