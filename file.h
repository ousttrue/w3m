#pragma once

int is_html_type(const char *type);
extern int FollowRedirection;

struct Buffer;
struct ParsedURL;
struct FormList;

/* flags for loadGeneralFile */
#define RG_NOCACHE 1

Buffer *loadGeneralFile(char *path, ParsedURL *current, char *referer, int flag,
                        FormList *request);

void showProgress(long long *linelen, long long *trbyte,
                  long long current_content_length);

bool couldWrite(const char *path);

