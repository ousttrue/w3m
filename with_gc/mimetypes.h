#pragma once
#include <string>

extern std::string mimetypes_files;

void initMimeTypes();
const char *guessContentType(const char *filename);
