#pragma once
#include "textlist.h"
#include <wc.h>
#include <stdbool.h>

extern const char* mimetypes_files;

// void initMimeTypes();

enum ContentType {
    CONTENTTYPE_UNKNOWN,
    CONTENTTYPE_TEXT_PLAIN,
    CONTENTTYPE_TEXT_HTML,
    CONTENTTYPE_APPLICATION_OCTET_STREAM,
    CONTENTTYPE_IMAGE_PNG,
};
inline static bool contentTypeIsImage(enum ContentType content_type)
{
    switch (content_type) {
    case CONTENTTYPE_IMAGE_PNG:
        return true;
    default:
        break;
    }
    return false;
}
const char* contentTypeStr(enum ContentType content_type);
// bool contentTypeIsText(enum ContentType content_type);
// bool is_text_type(const char* type);
// bool is_text_type(const char* type)
// {
//     return (type == NULL || type[0] == '\0' || strncasecmp(type, "text/", 5) == 0 || (strncasecmp(type, "application/", 12) == 0 && strstr(type, "xhtml") != NULL) || strncasecmp(type, "message/", sizeof("message/") - 1) == 0);
// }
enum ContentType guessContentType(const char* filename);
const char* guessFileName(const char* file);
const char* guessSaveName(TextList* document_header, const char* file);

struct ContentTypeCharset {
    enum ContentType content_type;
    wc_ces charset;
};

struct ContentTypeCharset getContentType(TextList* document_header);
