#pragma once
#include <string>
#include <string_view>

namespace ioutil {

extern std::string personal_document_root;

void initialize();
std::string myEditor(std::string_view file, int line);
std::string pwd();
std::string file_quote(std::string_view str);
std::string file_unquote(std::string_view str);
std::string expandName(std::string_view name);
std::string hostname();
bool is_localhost(std::string_view host);

} // namespace ioutil
