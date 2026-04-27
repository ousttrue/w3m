#include "compression.h"
#include "global.h"
#include "etc.h"
#include <w3m.h>
#include "UrlFile.h"
#include "url.h"
#include "content_type.h"
#include "textlist.h"
#include "input_stream.h"
#include "indep.h"

#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define GUNZIP_NAME "gunzip"
#define BUNZIP2_NAME "bunzip2"
#define INFLATE_NAME "inflate"
#define BROTLI_NAME "brotli"

#define GUNZIP_CMDNAME "gunzip"
#define BUNZIP2_CMDNAME "bunzip2"
#define INFLATE_CMDNAME "inflate"
#define BROTLI_CMDNAME "brotli"

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

const char* uncompressed_file_type(const char* path, const char** ext)
{

    if (path == NULL)
        return NULL;

    int slen = 0;
    int len = strlen(path);
    struct CompressionDecoder* d;
    for (d = decoders; d->type != CMP_NOCOMPRESS; d++) {
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

static const char* auxbinFile(const char* base)
{
    return expandPath(Strnew_m_charp(w3m_auxbin_dir(), "/", base, NULL)->ptr);
}

#define SAVE_BUF_SIZE 1536

void uncompress_stream(struct URLFile* uf, const char** src)
{
    if (ist_type(uf->stream) != IST_ENCODED) {
        uf->stream = ist_decode(uf->stream, uf->encoding);
        uf->encoding = ENC_7BIT;
    }

    const char* expand_cmd = GUNZIP_CMDNAME;
    const char* expand_name = GUNZIP_NAME;
    const char* tmpf = NULL;
    const char* ext = NULL;
    int use_d_arg = 0;
    for (struct CompressionDecoder* d = decoders; d->type != CMP_NOCOMPRESS; d++) {
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

    if (uf->scheme != SCM_FILE
        && !image_source) {
        tmpf = tmpfname(TMPF_DFL, ext);
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
            // child2
            uint8_t* buf = NewWithoutGC_N(uint8_t, SAVE_BUF_SIZE);

            setup_child(true, 2, ist_fd(uf->stream));

            FILE* f = NULL;
            if (tmpf)
                f = fopen(tmpf, "wb");

            int count;
            while ((count = ist_read(uf->stream, buf, SAVE_BUF_SIZE)) > 0) {
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
        // child1
        dup2(1, 2); /* stderr>&stdout */
        setup_child(true, -1, -1);
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
            uf->scheme = SCM_FILE;
    }
    UFhalfclose(uf);
    uf->stream = ist_from_fp(f1, fclose);
}
