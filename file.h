#pragma once
#include "Str.h"
#include <stdbool.h>

struct Buffer* loadHTMLString(Str page);
int is_html_type(const char* type);

struct Url;
struct FormList;
struct Buffer* loadGeneralFile(const char* path, struct Url* current, const char* referer, int flag, struct FormList* request, bool do_download);
struct URLFile;
struct TextList* readHeader(struct URLFile* uf, struct Url* pu);

int _doFileCopy(const char* tmpf, const char* defstr, bool download);
inline static int doFileCopy(const char* tmpf, const char* defstr)
{
    return _doFileCopy(tmpf, defstr, false);
}
int doFileMove(const char* tmpf, const char* defstr);
void examineFile(const char* path, struct URLFile* uf, bool do_download);
int dir_exist(const char* path);
