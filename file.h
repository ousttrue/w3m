#pragma once
#include <time.h>

int is_html_type(const char *type);
extern int FollowRedirection;

struct Buffer;
struct ParsedURL;
struct FormList;

#define RG_NOCACHE 1

Buffer *loadGeneralFile(const char *path, ParsedURL *current,
                        const char *referer, int flag, FormList *request);

void showProgress(long long *linelen, long long *trbyte,
                  long long current_content_length);

bool couldWrite(const char *path);

int setModtime(char *path, time_t modtime);
