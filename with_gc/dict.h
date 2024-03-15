#pragma once
#include <memory>

extern int UseDictCommand;
extern const char *DictCommand;

std::shared_ptr<struct Buffer> execdict(const char *word);
