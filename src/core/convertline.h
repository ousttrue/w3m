#pragma once
#include <Str.h>
#include <wc.h>

enum ConvertLineMode {
    RAW_MODE = 0,
    HTML_MODE = 1,
    HEADER_MODE = 2,
};
void cleanup_line(Str s);

struct URLFile;
Str convertLine(struct URLFile* uf, Str line, enum ConvertLineMode mode,
    wc_ces* pOutCharset, wc_ces from, wc_ces to);
