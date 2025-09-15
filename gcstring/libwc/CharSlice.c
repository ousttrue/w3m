#include "CharSlice.h"
#include <string.h>

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
