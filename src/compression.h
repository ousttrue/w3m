#pragma once
#include <string>

enum CompressionType {
  CMP_NOCOMPRESS = 0,
  CMP_COMPRESS = 1,
  CMP_GZIP = 2,
  CMP_BZIP2 = 3,
  CMP_DEFLATE = 4,
  CMP_BROTLI = 5,
};

struct compression_decoder {
  CompressionType type;
  std::string ext;
  const char *mime_type;
  int auxbin_p;
  const char *cmd;
  const char *name;
  const char *encoding;
  const char *encodings[4];
  int use_d_arg;
};

struct UrlStream;
void check_compression(const char *path, UrlStream *uf);
std::tuple<const char *, std::string> uncompressed_file_type(const char *path);
char *acceptableEncoding(void);
struct Str;
void process_compression(Str *lineBuf2, UrlStream *uf);
const char *compress_application_type(CompressionType compression);
const char *filename_extension(const char *patch, int is_url);
