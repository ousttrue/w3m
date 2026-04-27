#pragma once
#include "Str.h"
#include "url_scheme.h"
#include <libwc/wc_types.h>

struct InputStream;
struct CmdArgs;
struct URLFile;
struct _textlinelist;
struct Url;
struct Buffer;
struct Form;

bool doFileCopy(struct CmdArgs* args, const char* tmpf, const char* defstr);

struct Buffer* loadcmdout(struct CmdArgs* args, const char* cmd,
    struct Buffer* (*loadproc)(struct CmdArgs* args, struct URLFile*, struct Buffer*),
    struct Buffer* defaultbuf);
struct Buffer* loadHTMLBuffer(struct CmdArgs* args, struct URLFile* f, struct Buffer* newBuf);
struct Buffer* loadHTMLString(Str page);
struct Buffer* loadGeneralFile(struct CmdArgs* args, const char* path, struct Url* current, const char* referer, int flag, struct Form* request);
