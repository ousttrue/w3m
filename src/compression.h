#pragma once
#include <stdint.h>
#include <string>
#include <string_view>
#include <vector>
#include <span>

#define GUNZIP_CMDNAME "gunzip"
#define BUNZIP2_CMDNAME "bunzip2"
#define INFLATE_CMDNAME "inflate"
#define BROTLI_CMDNAME "brotli"

#define GUNZIP_NAME "gunzip"
#define BUNZIP2_NAME "bunzip2"
#define INFLATE_NAME "inflate"
#define BROTLI_NAME "brotli"

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
  std::string mime_type;
  int auxbin_p;
  const char *cmd;
  const char *name;
  const char *encoding;
  const char *encodings[4];
  int use_d_arg;
};

extern std::vector<compression_decoder> compression_decoders;

const compression_decoder *
compress_application_type(CompressionType compression);
const compression_decoder *check_compression(const std::string &path);
std::tuple<std::string, std::string>
uncompressed_file_type(const std::string &path);
const char *acceptableEncoding();
std::string filename_extension(const std::string_view patch, bool is_url);
std::vector<uint8_t> decode_gzip(std::span<uint8_t> src);
