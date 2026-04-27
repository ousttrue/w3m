#pragma once

enum ContentCompression {
    CMP_NOCOMPRESS = 0,
    CMP_COMPRESS = 1,
    CMP_GZIP = 2,
    CMP_BZIP2 = 3,
    CMP_DEFLATE = 4,
    CMP_BROTLI = 5,
};

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
// void check_compression(struct URLFile* uf, const char* path);
// void check_compression(struct URLFile* uf, const char* path)
// {
//     if (path == NULL)
//         return;
//
//     int len = strlen(path);
//     uf->compression = CMP_NOCOMPRESS;
//     for (struct CompressionDecoder* d = decoders; d->type != CMP_NOCOMPRESS; d++) {
//         if (d->ext == NULL)
//             continue;
//         int elen = strlen(d->ext);
//         if (len > elen && strcasecmp(&path[len - elen], d->ext) == 0) {
//             uf->compression = d->type;
//             uf->guess_type = d->mime_type;
//             break;
//         }
//     }
// }

struct URLFile;

const char* uncompressed_file_type(const char* path, const char** ext);

const char* acceptableEncoding(void);
void uncompress_stream(struct URLFile* uf, const char** src);
