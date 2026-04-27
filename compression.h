#pragma once

enum ContentCompression {
    CMP_NOCOMPRESS = 0,
    CMP_COMPRESS = 1,
    CMP_GZIP = 2,
    CMP_BZIP2 = 3,
    CMP_DEFLATE = 4,
    CMP_BROTLI = 5,
};

struct URLFile;
void check_compression(struct URLFile* uf, const char* path);
const char* uncompressed_file_type(const char* path, const char** ext);
const char* compress_application_type(enum ContentCompression compression);
const char* acceptableEncoding(void);
void parseCompression(struct URLFile* uf, const char* p);
void uncompress_stream(struct URLFile* uf, const char** src);
