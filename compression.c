#include "compression.h"
#include "etc.h"
#include "URLFile.h"
#include "url.h"
#include "w3m_rc.h"
#include <string.h>
#include <unistd.h>

#define GUNZIP_CMDNAME "gunzip"
#define BUNZIP2_CMDNAME "bunzip2"
#define INFLATE_CMDNAME "inflate"
#define BROTLI_CMDNAME "brotli"

#define GUNZIP_NAME "gunzip"
#define BUNZIP2_NAME "bunzip2"
#define INFLATE_NAME "inflate"
#define BROTLI_NAME "brotli"

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

static struct CompressionDecoder compression_decoders[] = {
    {
        .type = CMP_COMPRESS,
        .ext = ".gz",
        .mime_type = "application/x-gzip",
        .auxbin_p = 0,
        .cmd = GUNZIP_CMDNAME,
        .name = GUNZIP_NAME,
        .encoding = "gzip",
        .encodings = { "gzip", "x-gzip", 0, 0 },
        .use_d_arg = 0,
    },
    {
        .type = CMP_COMPRESS,
        .ext = ".Z",
        .mime_type = "application/x-compress",
        .auxbin_p = 0,
        .cmd = GUNZIP_CMDNAME,
        .name = GUNZIP_NAME,
        .encoding = "compress",
        .encodings = { "compress", "x-compress", 0, 0 },
        .use_d_arg = 0,
    },
    {
        .type = CMP_BZIP2,
        .ext = ".bz2",
        .mime_type = "application/x-bzip",
        .auxbin_p = 0,
        .cmd = BUNZIP2_CMDNAME,
        .name = BUNZIP2_NAME,
        .encoding = "bzip, bzip2",
        .encodings = { "x-bzip", "bzip", "bzip2", 0 },
        .use_d_arg = 0,
    },
    {
        .type = CMP_DEFLATE,
        .ext = ".deflate",
        .mime_type = "application/x-deflate",
        .auxbin_p = 1,
        .cmd = INFLATE_CMDNAME,
        .name = INFLATE_NAME,
        .encoding = "deflate",
        .encodings = { "deflate", "x-deflate", 0 },
        .use_d_arg = 0,
    },
    {
        .type = CMP_BROTLI,
        .ext = ".br",
        .mime_type = "application/x-br",
        .auxbin_p = 0,
        .cmd = BROTLI_CMDNAME,
        .name = BROTLI_NAME,
        .encoding = "br",
        .encodings = { "br", "x-br", 0 },
        .use_d_arg = 1,
    },
    {
        .type = CMP_NOCOMPRESS,
        .ext = 0,
        .mime_type = 0,
        .auxbin_p = 0,
        .cmd = 0,
        .name = 0,
        .encoding = 0,
        .encodings = { 0 },
        .use_d_arg = 0,
    },
};

void check_compression(const char* path, struct URLFile* uf)
{
    if (!path)
        return;

    int len = strlen(path);
    uf->compression = CMP_NOCOMPRESS;
    for (struct CompressionDecoder* d = compression_decoders; d->type != CMP_NOCOMPRESS; d++) {
        int elen;
        if (d->ext == NULL)
            continue;
        elen = strlen(d->ext);
        if (len > elen && strcasecmp(&path[len - elen], d->ext) == 0) {
            uf->compression = d->type;
            uf->guess_type = d->mime_type;
            break;
        }
    }
}

const char* uncompressed_file_type(const char* path, const char** ext)
{
    if (!path)
        return NULL;

    int len = strlen(path);
    int slen = 0;
    struct CompressionDecoder* d = compression_decoders;
    for (; d->type != CMP_NOCOMPRESS; d++) {
        if (d->ext == NULL)
            continue;
        slen = strlen(d->ext);
        if (len > slen && strcasecmp(&path[len - slen], d->ext) == 0)
            break;
    }
    if (d->type == CMP_NOCOMPRESS)
        return NULL;

    Str fn = Strnew_charp(path);
    Strshrink(fn, slen);
    if (ext)
        *ext = filename_extension(fn->ptr, 0);

    const char* t0 = guessContentType(fn->ptr);
    if (t0 == NULL)
        t0 = "text/plain";
    return t0;
}

const char* compress_application_type(enum CompressionType compression)
{
    for (struct CompressionDecoder* d = compression_decoders; d->type != CMP_NOCOMPRESS; d++) {
        if (d->type == compression)
            return d->mime_type;
    }
    return NULL;
}

static char* auxbinFile(const char* base)
{
    return expandPath(Strnew_m_charp(w3m_auxbin_dir(), "/", base, NULL)->ptr);
}

void uncompress_stream(struct URLFile* uf, const char** src)
{
    //  d;

    if (IStype(uf->stream) != IST_ENCODED) {
        uf->stream = newEncodedStream(uf->stream, uf->encoding);
        uf->encoding = ENC_7BIT;
    }

    const char* expand_name = GUNZIP_NAME;
    const char* ext = NULL;
    bool use_d_arg = 0;
    const char* expand_cmd = GUNZIP_CMDNAME;
    for (struct CompressionDecoder* d = compression_decoders; d->type != CMP_NOCOMPRESS; d++) {
        if (uf->compression == d->type) {
            if (d->auxbin_p)
                expand_cmd = auxbinFile(d->cmd);
            else
                expand_cmd = d->cmd;
            expand_name = d->name;
            ext = d->ext;
            use_d_arg = d->use_d_arg;
            break;
        }
    }
    uf->compression = CMP_NOCOMPRESS;

    const char* tmpf = NULL;
    if (uf->scheme != SCM_LOCAL && !getRuntime()->image_source) {
        tmpf = tmpfname(TMPF_DFL, ext)->ptr;
    }

    /* child1 -- stdout|f1=uf -> parent */
    FILE* f1;
    pid_t pid1 = open_pipe_rw(&f1, NULL);
    if (pid1 < 0) {
        UFclose(uf);
        return;
    }
    if (pid1 == 0) {
        /* child */
        pid_t pid2;
        FILE* f2 = stdin;

        /* uf -> child2 -- stdout|stdin -> child1 */
        pid2 = open_pipe_rw(&f2, NULL);
        if (pid2 < 0) {
            UFclose(uf);
            exit(1);
        }
        if (pid2 == 0) {
            /* child2 */
            char* buf = NewWithoutGC_N(char, SAVE_BUF_SIZE);
            int count;
            FILE* f = NULL;

            setup_child(TRUE, 2, UFfileno(uf));
            if (tmpf)
                f = fopen(tmpf, "wb");
            while ((count = ISread_n(uf->stream, buf, SAVE_BUF_SIZE)) > 0) {
                if (fwrite(buf, 1, count, stdout) != count)
                    break;
                if (f && fwrite(buf, 1, count, f) != count)
                    break;
            }
            UFclose(uf);
            if (f)
                fclose(f);
            xfree(buf);
            exit(0);
        }
        /* child1 */
        dup2(1, 2); /* stderr>&stdout */
        setup_child(TRUE, -1, -1);
        if (use_d_arg)
            execlp(expand_cmd, expand_name, "-d", NULL);
        else
            execlp(expand_cmd, expand_name, NULL);
        exit(1);
    }
    if (tmpf) {
        if (src)
            *src = tmpf;
        else
            uf->scheme = SCM_LOCAL;
    }
    UFhalfclose(uf);
    uf->stream = newFileStream(f1, (void (*)())fclose);
}

#define S_IXANY (S_IXUSR | S_IXGRP | S_IXOTH)

static int
check_command(const char* cmd, bool auxbin_p)
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

    static Str encodings = NULL;
    if (encodings != NULL)
        return encodings->ptr;

    struct TextList* l = newTextList();
    for (struct CompressionDecoder* d = compression_decoders; d->type != CMP_NOCOMPRESS; d++) {
        if (check_command(d->cmd, d->auxbin_p)) {
            pushText(l, d->encoding);
        }
    }
    encodings = Strnew();
    const char* p;
    while ((p = popText(l)) != NULL) {
        if (encodings->length)
            Strcat_charp(encodings, ", ");
        Strcat_charp(encodings, p);
    }
    return encodings->ptr;
}

enum CompressionType get_compression(const char* p)
{
    enum CompressionType compression = CMP_NOCOMPRESS;
    for (struct CompressionDecoder* d = compression_decoders; d->type != CMP_NOCOMPRESS; d++) {
        for (char** e = d->encodings; *e != NULL; e++) {
            if (strncasecmp(p, *e, strlen(*e)) == 0) {
                compression = d->type;
                break;
            }
        }
        if (compression != CMP_NOCOMPRESS)
            break;
    }
    return compression;
}
