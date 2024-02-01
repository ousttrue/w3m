#pragma once

#define CMP_NOCOMPRESS 0
#define CMP_COMPRESS 1
#define CMP_GZIP 2
#define CMP_BZIP2 3
#define CMP_DEFLATE 4
#define CMP_BROTLI 5

struct UrlStream;
void check_compression(const char *path, UrlStream *uf);
const char *uncompressed_file_type(const char *path, const char **ext);
void uncompress_stream(UrlStream *uf, const char **src);
char *acceptableEncoding(void);
struct Str;
void process_compression(Str *lineBuf2, UrlStream *uf);
const char *compress_application_type(int compression);
const char *filename_extension(const char *patch, int is_url);
