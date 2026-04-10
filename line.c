#include "line.h"

void cleanup_line(Str s, enum LineMode mode)
{
    if (s->length >= 2 && s->ptr[s->length - 2] == '\r' && s->ptr[s->length - 1] == '\n') {
        Strshrink(s, 2);
        Strcat_char(s, '\n');
    } else if (Strlastchar(s) == '\r')
        s->ptr[s->length - 1] = '\n';
    else if (Strlastchar(s) != '\n')
        Strcat_char(s, '\n');
    if (mode != PAGER_MODE) {
        int i;
        for (i = 0; i < s->length; i++) {
            if (s->ptr[i] == '\0')
                s->ptr[i] = ' ';
        }
    }
}
