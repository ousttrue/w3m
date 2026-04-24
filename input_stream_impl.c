#include "input_stream_impl.h"
#include "growbuf.h"
#include "mimehead.h"
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

struct InputStream*
ist_from_buffer(const char* s, int len)
{
    if (s == NULL)
        return NULL;

    struct input_stream_base* base = malloc(sizeof(struct input_stream_base));
    *base = (struct input_stream_base) {
        .iseos = false,
        .unclose = false,
    };
    alloc_buffer(&base->stream, (const uint8_t*)s, len);

    struct InputStream* ist = malloc(sizeof(struct InputStream));
    *ist = (struct InputStream) {
        .type = IST_BUFFER,
        .handle = base,
    };
    return ist;
}

//
// fd
//
struct input_stream_fd {
    int fd;
};

void fd_close(struct input_stream_fd* handle)
{
    close(handle->fd);
}

int fd_read(struct input_stream_fd* handle, uint8_t* buf, int len)
{
    return read(handle->fd, buf, len);
}

int fd_fd(struct input_stream_fd* handle)
{
    return handle->fd;
}

struct InputStream*
ist_from_fd(int des)
{
    if (des < 0)
        return NULL;

    struct input_stream_fd* handle = malloc(sizeof(struct input_stream_fd));
    *handle = (struct input_stream_fd) {
        .fd = des,
    };

    struct InputStream* ist = malloc(sizeof(struct InputStream));
    *ist = (struct InputStream) {
        .type = IST_FILE_DESC,
        .base = (struct input_stream_base) {
            .iseos = false,
            .unclose = false,
        },
        .handle = handle,
    };
    alloc_buffer(&ist->base.stream, NULL, STREAM_BUF_SIZE);
    return ist;
}

struct InputStream*
ist_from_path(const char* path)
{
    return ist_from_fd(open((path), O_RDONLY));
}

//
// FILE
//
struct input_stream_fp {
    FILE* fp;
    bool use_pipe;
};

void fp_close(struct input_stream_fp* handle)
{
    if (handle->use_pipe) {
        pclose(handle->fp);
    } else {
        fclose(handle->fp);
    }
}

int fp_read(struct input_stream_fp* handle, uint8_t* buf, int len)
{
    return fread(buf, 1, len, handle->fp);
}

int fp_fd(struct input_stream_fp* handle)
{
    return fileno(handle->fp);
}

struct InputStream* ist_from_fp(FILE* fp, bool use_pipe)
{
    if (fp == NULL)
        return NULL;

    struct input_stream_fp* handle = malloc(sizeof(struct input_stream_fp));
    *handle = (struct input_stream_fp) {
        .fp = fp,
        .use_pipe = use_pipe,
    };

    struct InputStream* ist = malloc(sizeof(struct InputStream));
    *ist = (struct InputStream) {
        .type = IST_FILE_PIPE,
        .base = (struct input_stream_base) {
            .iseos = false,
            .unclose = false,
        },
        .handle = handle,
    };
    alloc_buffer(&ist->base.stream, NULL, STREAM_BUF_SIZE);
    return ist;
}

//
// SOCK
//
struct input_stream_sock {
    int sock;
    SSL* ssl;
};

void sock_close(struct input_stream_sock* handle)
{
    if (handle->ssl) {
        SSL_free(handle->ssl);
    }
    close(handle->sock);
}

int sock_read(struct input_stream_sock* handle, uint8_t* buf, int len)
{
    int status;
    if (handle->ssl) {
        for (;;) {
            status = SSL_read(handle->ssl, buf, len);
            if (status > 0)
                break;
            switch (SSL_get_error(handle->ssl, status)) {
            case SSL_ERROR_WANT_READ:
            case SSL_ERROR_WANT_WRITE: /* reads can trigger write errors; see SSL_get_error(3) */
                continue;
            default:
                break;
            }
            break;
        }
    } else {
        status = read(handle->sock, buf, len);
    }
    return status;
}

int sock_fd(struct input_stream_sock* handle)
{
    return handle->sock;
}

#define SSL_BUF_SIZE 1536
struct InputStream*
ist_from_socket(int sock, SSL* ssl)
{
    if (sock < 0)
        return NULL;

    struct input_stream_sock* handle = malloc(sizeof(struct input_stream_sock));
    *handle = (struct input_stream_sock) {
        .ssl = ssl,
        .sock = sock,
    };

    struct InputStream* ist = malloc(sizeof(struct InputStream));
    *ist = (struct InputStream) {
        .type = IST_SOCK,
        .base = (struct input_stream_base) {
            .iseos = false,
            .unclose = false,
        },
        .handle = handle,
    };
    alloc_buffer(&ist->base.stream, NULL, SSL_BUF_SIZE);
    return ist;
}

//
// ENCODED
//
struct input_stream_encoded {
    struct InputStream* is;
    struct growbuf* gb;
    int pos;
    enum StreamEncoding encoding;
};

void ens_close(struct input_stream_encoded* handle)
{
    if (ist_destroy(handle->is)) {
        handle->is = NULL;
    }
    growbuf_destroy(handle->gb);
}

int ens_read(struct input_stream_encoded* handle, uint8_t* buf, int len)
{
    struct str_view gv = growbuf_str_view(handle->gb);
    if (handle->pos == growbuf_str_view(handle->gb).len) {
        ist_gets_to_growbuf(handle->is, handle->gb, true);
        gv = growbuf_str_view(handle->gb);
        if (gv.len == 0)
            return 0;

        if (handle->encoding == ENC_BASE64) {
            gv = sv_chop(gv);
        } else if (handle->encoding == ENC_UUENCODE) {
            if (gv.len >= 5 && !strncmp(gv.ptr, "begin", 5))
                ist_gets_to_growbuf(handle->is, handle->gb, true);
            gv = sv_chop(growbuf_str_view(handle->gb));
        }

        struct growbuf* gbtmp = growbuf_create();
        char* p = (char*)gv.ptr;
        if (handle->encoding == ENC_QUOTE)
            decodeQP_to_growbuf(gbtmp, &p);
        else if (handle->encoding == ENC_BASE64)
            decodeB_to_growbuf(gbtmp, &p);
        else if (handle->encoding == ENC_UUENCODE)
            decodeU_to_growbuf(gbtmp, &p);
        growbuf_destroy(handle->gb);
        handle->gb = gbtmp;
        handle->pos = 0;
    }

    if (len > gv.len - handle->pos)
        len = gv.len - handle->pos;

    memcpy(buf, &gv.ptr[handle->pos], len);
    handle->pos += len;
    return len;
}

int ens_fd(struct input_stream_encoded* handle)
{
    return ist_fd(handle->is);
}

struct InputStream* ist_decode(struct InputStream* is, enum StreamEncoding encoding)
{
    if (is == NULL || (encoding != ENC_QUOTE && encoding != ENC_BASE64 && encoding != ENC_UUENCODE))
        return is;

    struct input_stream_encoded* handle = malloc(sizeof(struct input_stream_encoded));
    *handle = (struct input_stream_encoded) {
        .is = is,
        .pos = 0,
        .encoding = encoding,
        .gb = growbuf_create(),
    };

    struct InputStream* ist = malloc(sizeof(struct InputStream));
    *ist = (struct InputStream) {
        .type = IST_ENCODED,
        .base = (struct input_stream_base) {
            .iseos = false,
            .unclose = false,
        },
        .handle = handle,
    };
    alloc_buffer(&ist->base.stream, NULL, STREAM_BUF_SIZE);

    return ist;
}
