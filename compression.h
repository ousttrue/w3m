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

struct CompressionDecoder* compression_from_type(enum CompressionType compression);
struct CompressionDecoder* compression_from_path(const char* path);

const char* uncompressed_file_type(const char* path, const char** ext);

struct input_stream* uncompress_stream(struct input_stream* stream,
    enum CompressionType compression, const char** out_tmpf);
const char* acceptableEncoding(void);
enum CompressionType get_compression(const char* p);
const char* compress_application_type(enum CompressionType compression);
