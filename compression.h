#pragma once

enum ContentCompression {
    CMP_NOCOMPRESS = 0,
    CMP_COMPRESS = 1,
    CMP_GZIP = 2,
    CMP_BZIP2 = 3,
    CMP_DEFLATE = 4,
    CMP_BROTLI = 5,
};

#define GUNZIP_NAME "gunzip"
#define BUNZIP2_NAME "bunzip2"
#define INFLATE_NAME "inflate"
#define BROTLI_NAME "brotli"

#define GUNZIP_CMDNAME "gunzip"
#define BUNZIP2_CMDNAME "bunzip2"
#define INFLATE_CMDNAME "inflate"
#define BROTLI_CMDNAME "brotli"

struct CompressionDecoder {
    enum ContentCompression type;
    const char* ext;
    const char* mime_type;
    int auxbin_p;
    const char* cmd;
    const char* name;
    const char* encoding;
    const char* encodings[4];
    int use_d_arg;
};

struct CompressionDecoder* compression_from_type(enum ContentCompression compression);
struct CompressionDecoder* compression_from_encodings(const char* p);
struct CompressionDecoder* compression_from_path(const char* path);
const char* uncompressed_file_type(const char* path, const char** ext);
const char* acceptableEncoding(void);
