#pragma once
#include <optional>
#include <string>
#include <string_view>

std::optional<std::string> matchattr(std::string_view pconst,
                                     std::string_view attr);
