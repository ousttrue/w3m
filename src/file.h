#pragma once
#include <time.h>

int is_html_type(const char *type);

extern bool SearchHeader;
extern int FollowRedirection;
extern const char *DefaultType;
extern bool DecodeCTE;

struct Buffer;
struct Url;
struct FormList;

#define RG_NOCACHE 1

Buffer *loadGeneralFile(const char *path, Url *current,
                        const char *referer, int flag, FormList *request);

void showProgress(long long *linelen, long long *trbyte,
                  long long current_content_length);

bool couldWrite(const char *path);

int setModtime(const char *path, time_t modtime);
Buffer *getshell(const char *cmd);

int _doFileCopy(const char *tmpf, const char *defstr, int download);
#define doFileCopy(tmpf, defstr) _doFileCopy(tmpf, defstr, FALSE);
int doFileMove(const char *tmpf, const char *defstr);
int checkCopyFile(const char *path1, const char *path2);
union input_stream;
int checkSaveFile(input_stream *stream, const char *path);
const char *guess_save_name(Buffer *buf, const char *file);
const char *checkHeader(Buffer *buf, const char *field);

