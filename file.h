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

int _MoveFile(const char* path1, const char* path2);
int save2tmp(struct InputStream* stream, enum UrlScheme scheme, const char* tmpf);
int _doFileCopy(struct CmdArgs* args, const char* tmpf, const char* defstr, int download);
#define doFileCopy(args, tmpf, defstr) _doFileCopy(args, tmpf, defstr, FALSE);
int doFileMove(struct CmdArgs* args, const char* tmpf, const char* defstr);
int doFileSave(struct CmdArgs* args, struct URLFile uf, const char* defstr);
int checkCopyFile(const char* path1, const char* path2);
int checkOverWrite(struct CmdArgs* args, const char* path);
const char* guess_save_name(struct Buffer* buf, const char* file);

struct Buffer* loadcmdout(struct CmdArgs* args, const char* cmd,
    struct Buffer* (*loadproc)(struct CmdArgs* args, struct URLFile*, struct Buffer*),
    struct Buffer* defaultbuf);
struct Buffer* loadHTMLBuffer(struct CmdArgs* args, struct URLFile* f, struct Buffer* newBuf);
struct Buffer* loadHTMLString(Str page);
Str loadGopherDir(struct URLFile* uf, struct Url* pu, wc_ces* charset);
Str loadGopherSearch(struct URLFile* uf, struct Url* pu, wc_ces* charset);
struct Buffer* loadGeneralFile(struct CmdArgs* args, const char* path, struct Url* current, const char* referer, int flag, struct Form* request);
