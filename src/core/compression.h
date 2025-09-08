#pragma once

#if defined(__EMX__) /* use $extension? */
#define GUNZIP_CMDNAME "gzip"
#define BUNZIP2_CMDNAME "bzip2"
#define INFLATE_CMDNAME "inflate.exe"
#define W3MHELPERPANEL_CMDNAME "w3mhelperpanel.exe"
#define USE_PATH_ENVVAR
#else
#define GUNZIP_CMDNAME "gunzip"
#define BUNZIP2_CMDNAME "bunzip2"
#define INFLATE_CMDNAME "inflate"
#define W3MHELPERPANEL_CMDNAME "w3mhelperpanel"
#endif

enum CompressionTyep {
    CMP_NOCOMPRESS = 0,
    CMP_COMPRESS = 1,
    CMP_GZIP = 2,
    CMP_BZIP2 = 3,
    CMP_DEFLATE = 4,
    CMP_BROTLI = 5,
};

const char* compress_application_type(enum CompressionTyep compression);
const char* uncompressed_file_type(const char* path, const char** ext);
const char* acceptableEncoding(void);

struct URLFile;
// void check_compression(struct URLFile* uf, const char* path);
void set_compression(struct URLFile* uf, const char* p);
