#include "input_stream_impl.h"
#include "growbuf.h"
#include "mimehead.h"
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

struct input_stream_base base_init(const uint8_t* p, size_t len)
{
    struct input_stream_base base = {
        .iseos = false,
        .unclose = false,
        .linebuf = growbuf_create(),
    };
    alloc_buffer(&base.stream, p, len);
    return base;
}

struct InputStream*
ist_from_buffer(const char* s, int len)
{
    if (!s) {
        return NULL;
    }

    struct InputStream* ist = malloc(sizeof(struct InputStream));
    *ist = (struct InputStream) {
        .type = IST_BUFFER,
        .base = base_init((const uint8_t*)s, len),
        .handle = 0,
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
        .base = base_init(NULL, STREAM_BUF_SIZE),
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
    FILE* fp;
    FpCloseFunc close_func;
};

void fp_close(struct input_stream_fp* handle)
{
    if (handle->close_func) {
        handle->close_func(handle->fp);
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

struct InputStream* ist_from_fp(FILE* fp, FpCloseFunc func)
{
    if (fp == NULL)
        return NULL;

    struct input_stream_fp* handle = malloc(sizeof(struct input_stream_fp));
    *handle = (struct input_stream_fp) {
        .fp = fp,
        .close_func = func,
    };

    struct InputStream* ist = malloc(sizeof(struct InputStream));
    *ist = (struct InputStream) {
        .type = IST_FILE_PIPE,
        .base = base_init(NULL, STREAM_BUF_SIZE),
        .handle = handle,
    };
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
        .base = base_init(NULL, SSL_BUF_SIZE),
        .handle = handle,
    };
    return ist;
}
