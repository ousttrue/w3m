#pragma once
#include "growbuf.h"
#include "input_stream.h"
#include "StreamBuffer.h"
#include <fcntl.h>
// #include <openssl/crypto.h>
#include <openssl/ssl.h>
#include <stdlib.h>
#include <unistd.h>

struct InputStream {
    enum InputStreamType type;
    void* handle;
};

//
// BUFFER
//
struct input_stream_base {
    struct StreamBuffer stream;
    bool iseos;
    bool unclose;
};
struct InputStream*
ist_from_buffer(const char* s, int len)
{
    if (s == NULL)
        return NULL;

    struct input_stream_base* handle = malloc(sizeof(struct input_stream_base));
    *handle = (struct input_stream_base) {
        .iseos = false,
        .unclose = false,
    };
    alloc_buffer(&handle->stream, (const uint8_t*)s, len);

    struct InputStream* ist = malloc(sizeof(struct InputStream));
    *ist = (struct InputStream) {
        .type = IST_BUFFER,
        .handle = handle,
    };
    return ist;
}

//
// fd
//
struct input_stream_fd {
    struct input_stream_base base;
    int fd;
};
static void fd_close(struct input_stream_fd* handle)
{
    close(handle->fd);
}
static int fd_read(struct input_stream_fd* handle, uint8_t* buf, int len)
{
    return read(handle->fd, buf, len);
}
struct InputStream*
ist_from_fd(int des)
{
    if (des < 0)
        return NULL;

    struct input_stream_fd* handle = malloc(sizeof(struct input_stream_fd));
    *handle = (struct input_stream_fd) {
        .base = (struct input_stream_base) {
            .iseos = false,
            .unclose = false,
        },
        .fd = des,
    };
    alloc_buffer(&handle->base.stream, NULL, STREAM_BUF_SIZE);

    struct InputStream* ist = malloc(sizeof(struct InputStream));
    *ist = (struct InputStream) {
        .type = IST_FILE_DESC,
        .handle = handle,
    };
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
    struct input_stream_base base;
    FILE* f;
    bool use_pipe;
};
static void fp_close(struct input_stream_fp* handle)
{
    if (handle->use_pipe) {
        pclose(handle->f);
    } else {
        fclose(handle->f);
    }
}
static int fp_read(struct input_stream_fp* handle, uint8_t* buf, int len)
{
    return fread(buf, 1, len, handle->f);
}
struct InputStream* ist_from_fp(FILE* f, bool use_pipe)
{
    if (f == NULL)
        return NULL;

    struct input_stream_fp* handle = malloc(sizeof(struct input_stream_fp));
    *handle = (struct input_stream_fp) {
        .base = (struct input_stream_base) {
            .iseos = false,
            .unclose = false,
        },
        .f = f,
        .use_pipe = use_pipe,
    };
    alloc_buffer(&handle->base.stream, NULL, STREAM_BUF_SIZE);

    struct InputStream* ist = malloc(sizeof(struct InputStream));
    *ist = (struct InputStream) {
        .type = IST_FILE_PIPE,
        .handle = handle,
    };
    return ist;
}

//
// SOCK
//
struct input_stream_sock {
    struct input_stream_base base;
    int sock;
    SSL* ssl;
};
static void sock_close(struct input_stream_sock* handle)
{
    if (handle->ssl) {
        SSL_free(handle->ssl);
    }
    close(handle->sock);
}
static int sock_read(struct input_stream_sock* handle, uint8_t* buf, int len)
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

#define SSL_BUF_SIZE 1536
struct InputStream*
ist_from_socket(int sock, SSL* ssl)
{
    if (sock < 0)
        return NULL;

    struct input_stream_sock* handle = malloc(sizeof(struct input_stream_sock));
    *handle = (struct input_stream_sock) {
        .base = (struct input_stream_base) {
            .iseos = false,
            .unclose = false,
        },
        .ssl = ssl,
        .sock = sock,
    };
    alloc_buffer(&handle->base.stream, NULL, SSL_BUF_SIZE);

    struct InputStream* ist = malloc(sizeof(struct InputStream));
    *ist = (struct InputStream) {
        .type = IST_SOCK,
        .handle = handle,
    };
    return ist;
}

//
// ENCODED
//
struct input_stream_encoded {
    struct input_stream_base base;
    struct InputStream* is;
    struct growbuf* gb;
    int pos;
    enum StreamEncoding encoding;
};
static void
ens_close(struct input_stream_encoded* handle)
{
    if (ist_destroy(handle->is)) {
        handle->is = NULL;
    }
    growbuf_destroy(handle->gb);
}

static int
ens_read(struct input_stream_encoded* handle, uint8_t* buf, int len)
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

struct InputStream* ist_decode(struct InputStream* is, enum StreamEncoding encoding)
{
    if (is == NULL || (encoding != ENC_QUOTE && encoding != ENC_BASE64 && encoding != ENC_UUENCODE))
        return is;

    struct input_stream_encoded* handle = malloc(sizeof(struct input_stream_encoded));
    *handle = (struct input_stream_encoded) {
        .base = (struct input_stream_base) {
            .iseos = false,
            .unclose = false,
        },
        .is = is,
        .pos = 0,
        .encoding = encoding,
        .gb = growbuf_create(),
    };
    alloc_buffer(&handle->base.stream, NULL, STREAM_BUF_SIZE);

    struct InputStream* stream = malloc(sizeof(struct InputStream));
    *stream = (struct InputStream) {
        .type = IST_ENCODED,
        .handle = handle,
    };

    return stream;
}
