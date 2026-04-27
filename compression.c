#include "compression.h"
#include "Str.h"
#include <w3m.h>
#include "url.h"
#include "content_type.h"
#include "textlist.h"
#include <sys/stat.h>

static struct CompressionDecoder decoders[] = {
    { CMP_COMPRESS, ".gz", "application/x-gzip",
        0, GUNZIP_CMDNAME, GUNZIP_NAME, "gzip",
        { "gzip", "x-gzip", NULL }, 0 },
    { CMP_COMPRESS, ".Z", "application/x-compress",
        0, GUNZIP_CMDNAME, GUNZIP_NAME, "compress",
        { "compress", "x-compress", NULL }, 0 },
    { CMP_BZIP2, ".bz2", "application/x-bzip",
        0, BUNZIP2_CMDNAME, BUNZIP2_NAME, "bzip, bzip2",
        { "x-bzip", "bzip", "bzip2", NULL }, 0 },
    { CMP_DEFLATE, ".deflate", "application/x-deflate",
        1, INFLATE_CMDNAME, INFLATE_NAME, "deflate",
        { "deflate", "x-deflate", NULL }, 0 },
    { CMP_BROTLI, ".br", "application/x-br",
        0, BROTLI_CMDNAME, BROTLI_NAME, "br",
        { "br", "x-br", NULL }, 1 },
    { CMP_NOCOMPRESS, NULL, NULL, 0, NULL, NULL, NULL, { NULL }, 0 },
};

struct CompressionDecoder* compression_from_type(enum ContentCompression compression)
{
    for (struct CompressionDecoder* d = decoders; d->type != CMP_NOCOMPRESS; d++) {
        if (d->type == compression)
            return d;
    }
    return NULL;
}

struct CompressionDecoder* compression_from_encodings(const char* p)
{
    for (struct CompressionDecoder* d = decoders; d->type != CMP_NOCOMPRESS; d++) {
        for (const char** e = d->encodings; *e != NULL; e++) {
            if (strncasecmp(p, *e, strlen(*e)) == 0) {
                return d;
            }
        }
    }
    return NULL;
}

struct CompressionDecoder* compression_from_path(const char* path)
{
    if (path) {
        int len = strlen(path);
        for (struct CompressionDecoder* d = decoders; d->type != CMP_NOCOMPRESS; d++) {
            if (d->ext == NULL)
                continue;
            int elen = strlen(d->ext);
            if (len > elen && strcasecmp(&path[len - elen], d->ext) == 0) {
                return d;
            }
        }
    }
    return NULL;
}

struct ContentTypeWithExt compression_from_path_to_content_type(const char* path)
{
    struct ContentTypeWithExt ce = { 0 };
    const struct CompressionDecoder* d = compression_from_path(path);
    if (d) {
        Str fn = Strnew_charp(path);
        Strshrink(fn, strlen(d->ext));
        ce.ext = filename_extension(fn->ptr, 0);
        ce.content_type = guessContentType(fn->ptr);
        if (ce.content_type == NULL) {
            ce.content_type = "text/plain";
        }
    }
    return ce;
}

#define PATH_SEPARATOR ':'
#define S_IXANY (S_IXUSR | S_IXGRP | S_IXOTH)

static int
check_command(const char* cmd, int auxbin_p)
{
    static char* path = NULL;
    if (path == NULL)
        path = getenv("PATH");

    Str dirs;
    if (auxbin_p)
        dirs = Strnew_charp(w3m_auxbin_dir());
    else
        dirs = Strnew_charp(path);

    char* np;
    for (char* p = dirs->ptr; p != NULL; p = np) {
        char* np = strchr(p, PATH_SEPARATOR);
        if (np)
            *np++ = '\0';
        Str pathname = Strnew();
        Strcat_charp(pathname, p);
        Strcat_char(pathname, '/');
        Strcat_charp(pathname, cmd);
        struct stat st;
        if (stat(pathname->ptr, &st) == 0 && S_ISREG(st.st_mode)
            && (st.st_mode & S_IXANY) != 0)
            return 1;
    }
    return 0;
}

const char* acceptableEncoding(void)
{
    static Str encodings = NULL;
    if (encodings != NULL) {
        return encodings->ptr;
    }
    encodings = Strnew();

    TextList* l = newTextList();
    for (struct CompressionDecoder* d = decoders; d->type != CMP_NOCOMPRESS; d++) {
        if (check_command(d->cmd, d->auxbin_p)) {
            pushText(l, d->encoding);
        }
    }
    char* p;
    while ((p = popText(l)) != NULL) {
        if (encodings->length)
            Strcat_charp(encodings, ", ");
        Strcat_charp(encodings, p);
    }
    return encodings->ptr;
}
