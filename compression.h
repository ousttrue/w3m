#pragma once
#include <stdbool.h>

enum CompressionType {
    CMP_NOCOMPRESS = 0,
    CMP_COMPRESS = 1,
    CMP_GZIP = 2,
    CMP_BZIP2 = 3,
    CMP_DEFLATE = 4,
    CMP_BROTLI = 5,
};

struct CompressionDecoder {
    enum CompressionType type;
    const char* ext;
    const char* mime_type;
    bool auxbin_p;
    const char* cmd;
    const char* name;
    char* encoding;
    char* encodings[4];
    int use_d_arg;
};

struct URLFile;
void check_compression(const char* path, struct URLFile* uf);
const char* uncompressed_file_type(const char* path, const char** ext);
void uncompress_stream(struct URLFile* uf, const char** src);
const char* acceptableEncoding(void);
enum CompressionType get_compression(const char* p);
const char* compress_application_type(enum CompressionType compression);
