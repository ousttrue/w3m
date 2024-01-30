#pragma once

int is_html_type(const char *type);

struct Buffer;
struct ParsedURL;
struct FormList;

/* flags for loadGeneralFile */
#define RG_NOCACHE 1

Buffer *loadGeneralFile(char *path, ParsedURL *current, char *referer, int flag,
                        FormList *request);
