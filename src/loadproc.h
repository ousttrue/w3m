#pragma once
#include "http_option.h"
#include <stdio.h>
#include <time.h>

extern const char *DefaultType;
extern int FollowRedirection;
extern bool DecodeCTE;

struct Buffer;
struct Url;
struct FormList;
struct UrlStream;

using LoadProc = Buffer *(*)(UrlStream *, Buffer *);

Buffer *loadGeneralFile(const char *path, Url *current,
                        const HttpOption &option, FormList *request = {});

const char *checkHeader(Buffer *buf, const char *field);
const char *guess_save_name(Buffer *buf, const char *file);
Buffer *getshell(const char *cmd);
struct Url;
extern void readHeader(UrlStream *uf, Buffer *newBuf, int thru, Url *pu);
int _doFileCopy(const char *tmpf, const char *defstr, int download);
#define doFileCopy(tmpf, defstr) _doFileCopy(tmpf, defstr, false);
int is_html_type(const char *type);
void saveBuffer(Buffer *buf, FILE *f, int cont);
bool couldWrite(const char *path);
int setModtime(const char *path, time_t modtime);
const char *shell_quote(const char *str);
struct UrlStream;
Buffer *loadBuffer(UrlStream *uf, Buffer *newBuf);
