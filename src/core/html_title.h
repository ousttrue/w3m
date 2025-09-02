#pragma once
#include <Str.h>

struct HtmlTagParsed;
void init_title();
Str process_title(struct HtmlTagParsed* tag);
Str process_n_title(struct HtmlTagParsed* tag);
void feed_title(char* str);
