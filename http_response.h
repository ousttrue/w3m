#pragma once
#include "Str.h"
#include <stdbool.h>

extern int http_response_code;

struct CmdArgs;
struct URLFile;
struct Buffer;
struct Url;
void readHeader(struct CmdArgs* args, struct URLFile* uf, struct Buffer* newBuf, bool thru, struct Url* pu);

bool matchattr(const char* p, const char* attr, int len, Str* value);
