#pragma once

extern bool UseDictCommand;
extern const char *DictCommand;

struct Buffer;
struct Buffer *execdict(const char *word);
