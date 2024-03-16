#pragma once
#include <memory>
#include <string>

extern bool UseDictCommand;
extern std::string DictCommand;

std::shared_ptr<struct Buffer> execdict(const char *word);
