#pragma once

enum CompressionType {
    CMP_NOCOMPRESS = 0,
    CMP_COMPRESS = 1,
    CMP_GZIP = 2,
    CMP_BZIP2 = 3,
    CMP_DEFLATE = 4,
    CMP_BROTLI = 5,
};

struct compression_decoder {
    enum CompressionType type;
    const char* ext;
    const char* mime_type;
    bool auxbin_p;
    const char* cmd;
    const char* name;
    const char* encoding;
    const char* encodings[4];
    bool use_d_arg;
};

struct UrlStream;
struct compression_decoder* getDecorder(int i);
void check_compression(const char* path, struct UrlStream* uf);
const char* uncompressed_file_type(const char* path, const char** ext);
const char* compress_application_type(enum CompressionType compression);
void uncompress_stream(struct UrlStream* uf, const char** src, int SAVE_BUF_SIZE);
