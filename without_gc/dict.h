#pragma once
#include <memory>
#include <string>
#include <string_view>

extern bool UseDictCommand;
extern std::string DictCommand;

std::shared_ptr<struct HttpResponse> execdict(std::string_view word);
