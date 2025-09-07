#include "convertline.h"

void cleanup_line(Str s)
{
    if (s->length >= 2 && s->ptr[s->length - 2] == '\r' && s->ptr[s->length - 1] == '\n') {
        // ends CRLF
        Strshrink(s, 2);
        Strcat_char(s, '\n');
    } else if (Strlastchar(s) == '\r') {
        // ends CR
        s->ptr[s->length - 1] = '\n';
    } else if (Strlastchar(s) != '\n') {
        // ends NOT LF / CR / CRLF
        Strcat_char(s, '\n');
    }
    for (int i = 0; i < s->length; i++) {
        if (s->ptr[i] == '\0') {
            s->ptr[i] = ' ';
        }
    }
}

Str convertLine(struct URLFile* uf, Str line, enum ConvertLineMode mode,
    wc_ces* pOutCharset, wc_ces from, wc_ces to)
{
    line = wc_Str_conv_with_detect(line, pOutCharset, from, to);
    if (mode != RAW_MODE) {
        cleanup_line(line);
    }
    return line;
}
