#include "http_message.h"
#include "myctype.h"
#include <string.h>
#include <strings.h>

struct CharSlice makeSlice(const char* p)
{
    return (struct CharSlice) {
        p,
        strlen(p),
    };
}

bool startswith(const char* p, struct CharSlice slice)
{
    return strncasecmp(p, slice.p, slice.len) == 0;
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
