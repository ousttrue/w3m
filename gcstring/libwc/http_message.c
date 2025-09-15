#include "http_message.h"
#include "myctype.h"
#include "quote.h"
#include <string.h>
#include <strings.h>

const char* getHttpHeaderValue(TextList* document_header, const char* field)
{
    if (!field) {
        return NULL;
    }
    if (!document_header) {
        return NULL;
    }

    int len = strlen(field);
    for (TextListItem* i = document_header->first; i != NULL; i = i->next) {
        if (!strncasecmp(i->ptr, field, len)) {
            char* p = i->ptr + len;
            return remove_space(p);
        }
    }
    return NULL;
}

struct CharSlice extractSemiColon(const char* p, struct CharSlice attr)
{
    if (!startswith(p, attr)) {
        // not found
        return (struct CharSlice) { 0, 0 };
    }

    p += attr.len;
    SKIP_BLANKS(p);

    if (IS_ENDT(*p)) {
        // found and empty value
        return (struct CharSlice) { p, 0 };
    }

    if (*p != '=') {
        // not found
        return (struct CharSlice) { 0, 0 };
    }
    p++;
    SKIP_BLANKS(p);

    if (*p == '"') {
        // quoted. search "
        p++;
        struct CharSlice slice = {
            .p = p,
            .len = 0,
        };
        for (; !IS_ENDL(*p) && *p != '"'; ++p) {
            ++slice.len;
        }
        return slice;
    } else {
        // not quoted search ;
        struct CharSlice slice = {
            .p = p,
            .len = 0,
        };
        for (; !IS_ENDL(*p) && *p != ';'; ++p) {
            ++slice.len;
        }
        return slice;
    }
}
