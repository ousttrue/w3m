#pragma once
#include "http_request.h"
#include <stdio.h>
#include <time.h>
#include <optional>
#include <memory>

extern const char *DefaultType;

struct Url;
struct FormList;
struct HttpResponse;

std::shared_ptr<HttpResponse> loadGeneralFile(const char *path,
                                              std::optional<Url> current,
                                              const HttpOption &option,
                                              FormList *request = {});

struct Buffer;
Buffer *getshell(const char *cmd);
struct Url;
int _doFileCopy(const char *tmpf, const char *defstr, int download);
#define doFileCopy(tmpf, defstr) _doFileCopy(tmpf, defstr, false);
void saveBuffer(Buffer *buf, FILE *f, int cont);
bool couldWrite(const char *path);
int setModtime(const char *path, time_t modtime);
const char *shell_quote(const char *str);
struct LineLayout;
void loadBuffer(HttpResponse *res, LineLayout *layout);
Buffer *loadHTMLString(Str *page);
void cmd_loadBuffer(Buffer *buf, int linkid);
void cmd_loadfile(const char *fn);
void cmd_loadURL(const char *url, std::optional<Url> current,
                 const char *referer, FormList *request);
