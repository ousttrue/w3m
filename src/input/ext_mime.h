#pragma once

extern const char *mimetypes_files;

struct ExtMime {
  const char *ext;
  const char *mime;
};

void initMimeTypes();
