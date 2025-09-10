#pragma once
#include <textlist.h>

extern char* mimetypes_files;

void initMimeTypes();
const char* guessContentType(const char* filename);
