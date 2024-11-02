#pragma once

extern bool UseDictCommand;
extern const char *DictCommand;

struct Document;
struct Document *execdict(const char *word);
