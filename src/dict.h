#pragma once

extern bool UseDictCommand;
extern const char *DictCommand;

struct Content;
struct Content *execdict(const char *word);
