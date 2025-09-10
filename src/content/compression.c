#include "compression.h"
#include "url.h"
#include "runtime.h"
#include "mimetypes.h"
#include "textlist.h"
#include <Str.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

#define GUNZIP_NAME "gunzip"
#define BUNZIP2_NAME "bunzip2"
#define INFLATE_NAME "inflate"
#define BROTLI_NAME "brotli"
#define BROTLI_CMDNAME "brotli"

struct compression_decoder {
    int type;
    char* ext;
    char* mime_type;
    int auxbin_p;
    char* cmd;
    char* name;
    char* encoding;
    char* encodings[4];
    int use_d_arg;
};

static struct compression_decoder compression_decoders[] = {
    { CMP_COMPRESS, ".gz", "application/x-gzip",
        0, GUNZIP_CMDNAME, GUNZIP_NAME, "gzip",
        { "gzip", "x-gzip", 0 }, 0 },
    { CMP_COMPRESS, ".Z", "application/x-compress",
        0, GUNZIP_CMDNAME, GUNZIP_NAME, "compress",
        { "compress", "x-compress", 0 }, 0 },
    { CMP_BZIP2, ".bz2", "application/x-bzip",
        0, BUNZIP2_CMDNAME, BUNZIP2_NAME, "bzip, bzip2",
        { "x-bzip", "bzip", "bzip2", 0 }, 0 },
    { CMP_DEFLATE, ".deflate", "application/x-deflate",
        1, INFLATE_CMDNAME, INFLATE_NAME, "deflate",
        { "deflate", "x-deflate", 0 }, 0 },
    { CMP_BROTLI, ".br", "application/x-br",
        0, BROTLI_CMDNAME, BROTLI_NAME, "br",
        { "br", "x-br", 0 }, 1 },
    { CMP_NOCOMPRESS, 0, 0, 0, 0, 0, 0, { 0 }, 0 },
};

#define S_IXANY (S_IXUSR | S_IXGRP | S_IXOTH)
#define PATH_SEPARATOR ':'

static int
check_command(char* cmd, int auxbin_p)
{
    static char* path = NULL;
    Str dirs;
    char *p, *np;
    Str pathname;
    struct stat st;

    if (path == NULL)
        path = getenv("PATH");
    if (auxbin_p)
        dirs = Strnew_charp(w3m_auxbin_dir());
    else
        dirs = Strnew_charp(path);
    for (p = dirs->ptr; p != NULL; p = np) {
        np = strchr(p, PATH_SEPARATOR);
        if (np)
            *np++ = '\0';
        pathname = Strnew();
        Strcat_charp(pathname, p);
        Strcat_char(pathname, '/');
        Strcat_charp(pathname, cmd);
        if (stat(pathname->ptr, &st) == 0 && S_ISREG(st.st_mode)
            && (st.st_mode & S_IXANY) != 0)
            return 1;
    }
    return 0;
}

const char* acceptableEncoding(void)
{
    static Str encodings = 0;
    struct compression_decoder* d;
    TextList* l;
    char* p;

    if (encodings != NULL)
        return encodings->ptr;
    l = newTextList();
    for (d = compression_decoders; d->type != CMP_NOCOMPRESS; d++) {
        if (check_command(d->cmd, d->auxbin_p)) {
            pushText(l, d->encoding);
        }
    }
    encodings = Strnew();
    while ((p = popText(l)) != NULL) {
        if (encodings->length)
            Strcat_charp(encodings, ", ");
        Strcat_charp(encodings, p);
    }
    return encodings->ptr;
}

// void check_compression(struct URLFile* uf, const char* path)
// {
//     if (!path)
//         return;
//
//     int len = strlen(path);
//     uf->compression = CMP_NOCOMPRESS;
//     struct compression_decoder* d;
//     for (d = compression_decoders; d->type != CMP_NOCOMPRESS; d++) {
//         int elen;
//         if (d->ext == NULL)
//             continue;
//         elen = strlen(d->ext);
//         if (len > elen && strcasecmp(&path[len - elen], d->ext) == 0) {
//             uf->compression = d->type;
//             uf->guess_type = d->mime_type;
//             break;
//         }
//     }
// }

const char* compress_application_type(enum CompressionTyep compression)
{
    for (struct compression_decoder* d = compression_decoders; d->type != CMP_NOCOMPRESS; d++) {
        if (d->type == compression)
            return d->mime_type;
    }
    return NULL;
}

const char* uncompressed_file_type(const char* path, const char** ext)
{
    int len, slen;
    Str fn;
    struct compression_decoder* d;

    if (path == NULL)
        return NULL;

    slen = 0;
    len = strlen(path);
    for (d = compression_decoders; d->type != CMP_NOCOMPRESS; d++) {
        if (d->ext == NULL)
            continue;
        slen = strlen(d->ext);
        if (len > slen && strcasecmp(&path[len - slen], d->ext) == 0)
            break;
    }
    if (d->type == CMP_NOCOMPRESS)
        return NULL;

    fn = Strnew_charp(path);
    Strshrink(fn, slen);
    if (ext)
        *ext = filename_extension(fn->ptr, 0);

    const char* t0 = guessContentType(fn->ptr);
    if (t0 == NULL)
        t0 = "text/plain";
    return t0;
}

void set_compression(const char* p, enum CompressionTyep* pCompression)
{
    *pCompression = CMP_NOCOMPRESS;

    for (struct compression_decoder* d = compression_decoders; d->type != CMP_NOCOMPRESS; d++) {
        char** e;
        for (e = d->encodings; *e != NULL; e++) {
            if (strncasecmp(p, *e, strlen(*e)) == 0) {
                *pCompression = d->type;
                break;
            }
        }
        if (*pCompression != CMP_NOCOMPRESS)
            break;
    }
}
