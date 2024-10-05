#pragma once

enum COMPRESSION_TYPE {
  CMP_NOCOMPRESS = 0,
  CMP_COMPRESS = 1,
  CMP_GZIP = 2,
  CMP_BZIP2 = 3,
  CMP_DEFLATE = 4,
  CMP_BROTLI = 5,
};

struct URLFile;
void check_compression(const char *path, struct URLFile *uf);
const char *filename_extension(const char *patch, bool is_url);
const char *uncompressed_file_type(const char *path, const char **ext);
char *acceptableEncoding();
enum COMPRESSION_TYPE compressionFromEncoding(const char *encoding);
const char *compress_application_type(enum COMPRESSION_TYPE compression);
void uncompress_stream(struct URLFile *uf, const char **src);
