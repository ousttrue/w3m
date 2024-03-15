#pragma once

extern const char *mimetypes_files;

void initMimeTypes();
const char *guessContentType(const char *filename);
