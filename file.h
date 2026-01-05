#pragma once
#include "Str.h"
#include "content.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

struct Buffer* loadHTMLString(Str page);
int is_html_type(const char* type);

struct Url;
struct FormList;

int _doFileCopy(const char* tmpf, const char* defstr, bool download);
inline static int doFileCopy(const char* tmpf, const char* defstr)
{
    return _doFileCopy(tmpf, defstr, false);
}
int doFileMove(const char* tmpf, const char* defstr);
int checkCopyFile(const char* path1, const char* path2);

int checkOverWrite(const char* path);
struct input_stream;
int checkSaveFile(struct input_stream* stream, const char* path);

struct Buffer* doExternal(struct Url url, struct input_stream* stream,
    const char* type, struct Buffer* defaultbuf, bool internal);
struct Buffer* loadHTMLBuffer(struct Url url, struct input_stream* stream,
    const char* type, struct Buffer* newBuf, bool internal);
struct Buffer* loadBuffer(struct Url url, struct input_stream* stream,
    const char* type, struct Buffer* newBuf, bool internal);
struct Buffer* loadImageBuffer(struct Url url, struct input_stream* stream,
    const char* type, struct Buffer* newBuf, bool internal);
extern int getMetaRefreshParam(const char* q, Str* refresh_uri);
extern int is_boundary(unsigned char*, unsigned char*);
extern Str process_n_button(void);
struct Document;
extern char* convert_size(int64_t size, int usefloat);
extern char* convert_size2(int64_t size1, int64_t size2, int usefloat);
