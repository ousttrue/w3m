#pragma once
#include <Str.h>

struct parsed_tag;
void init_title();
Str process_title(struct parsed_tag* tag);
Str process_n_title(struct parsed_tag* tag);
void feed_title(char* str);
