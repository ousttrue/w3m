#pragma once

extern const char* mimetypes_files;

void initMimeTypes(void);
const char* guessContentType(const char* filename);
