#pragma once
#include "http_request.h"
#include <stdio.h>
#include <time.h>
#include <optional>

extern const char *DefaultType;
extern bool DecodeCTE;

struct Buffer;
struct Url;
struct FormList;
struct UrlStream;

Buffer *loadGeneralFile(const char *path, std::optional<Url> current,
                        const HttpOption &option, FormList *request = {});

const char *guess_save_name(Buffer *buf, const char *file);
Buffer *getshell(const char *cmd);
struct Url;
int _doFileCopy(const char *tmpf, const char *defstr, int download);
#define doFileCopy(tmpf, defstr) _doFileCopy(tmpf, defstr, false);
int is_html_type(const char *type);
void saveBuffer(Buffer *buf, FILE *f, int cont);
bool couldWrite(const char *path);
int setModtime(const char *path, time_t modtime);
const char *shell_quote(const char *str);
struct UrlStream;
Buffer *loadBuffer(UrlStream *uf, Buffer *newBuf);
void cmd_loadBuffer(Buffer *buf, int linkid);
void cmd_loadfile(const char *fn);
void cmd_loadURL(const char *url, std::optional<Url> current,
                 const char *referer, FormList *request);
