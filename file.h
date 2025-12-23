#pragma once
#include "Str.h"

struct Buffer* loadHTMLString(Str page);
extern int is_html_type(char* type);
struct Line* getNextPage(struct Buffer* buf, int plen);

