#include "myctype.h"

bool non_null(const char* s)
{
    if (!s)
        return false;
    while (*s) {
        if (!IS_SPACE(*s))
            return true;
        s++;
    }
    return false;
}
