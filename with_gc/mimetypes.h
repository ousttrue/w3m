#pragma once
#include <string>

extern std::string mimetypes_files;

void initMimeTypes();
std::string guessContentType(std::string_view filename);
