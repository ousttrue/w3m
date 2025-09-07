#include "file_copy.h"
#include "etc.h"
#include "downloadlist.h"
#include "tmpfile.h"
#include "ssl_util.h"
#include "mysignal.h"
#include "mimehead.h"
#include "istream.h"
#include "keymap.h"
#include "linein.h"
#include "history.h"
#include "tty.h"
#include "progress.h"
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <setjmp.h>

char AutoUncompress = (false);
char PreserveTimestamp = (true);

#define uchar unsigned char

#define STREAM_BUF_SIZE 8192
#define SSL_BUF_SIZE 1536

#define MUST_BE_UPDATED(bs) ((bs)->stream.cur == (bs)->stream.next)

#define POP_CHAR(bs) ((bs)->iseos ? '\0' : (bs)->stream.buf[(bs)->stream.cur++])

static void basic_close(int* handle);
static int basic_read(int* handle, char* buf, int len);

static void file_close(struct io_file_handle* handle);
static int file_read(struct io_file_handle* handle, char* buf, int len);

static int str_read(Str handle, char* buf, int len);

static int ens_read(struct ens_handle* handle, char* buf, int len);
static void ens_close(struct ens_handle* handle);

static void memchop(char* p, int* len);

static void
do_update(BaseStream base)
{
    int len;
    base->stream.cur = base->stream.next = 0;
    len = (*base->read)(base->handle, base->stream.buf, base->stream.size);
    if (len <= 0)
        base->iseos = true;
    else
        base->stream.next += len;
}

static int
buffer_read(StreamBuffer sb, char* obuf, int count)
{
    int len = sb->next - sb->cur;
    if (len > 0) {
        if (len > count)
            len = count;
        memcpy(obuf, &sb->buf[sb->cur], len);
        sb->cur += len;
    }
    return len;
}

static void
init_buffer(BaseStream base, char* buf, int bufsize)
{
    StreamBuffer sb = &base->stream;
    sb->size = bufsize;
    sb->cur = 0;
    sb->buf = NewWithoutGC_N(uchar, bufsize);
    if (buf) {
        memcpy(sb->buf, buf, bufsize);
        sb->next = bufsize;
    } else {
        sb->next = 0;
    }
    base->iseos = false;
}

static void
init_base_stream(BaseStream base, int bufsize)
{
    init_buffer(base, NULL, bufsize);
}

static void
init_str_stream(BaseStream base, Str s)
{
    init_buffer(base, s->ptr, s->length);
}

InputStream
newInputStream(int des)
{
    InputStream stream;
    if (des < 0)
        return NULL;
    stream = NewWithoutGC(union input_stream);
    init_base_stream(&stream->base, STREAM_BUF_SIZE);
    stream->base.type = IST_BASIC;
    stream->base.handle = NewWithoutGC(int);
    *(int*)stream->base.handle = des;
    stream->base.read = (int (*)(void*, void*, int))basic_read;
    stream->base.close = (void (*)(void*))basic_close;
    return stream;
}

InputStream newFileStream(FILE* f, FileCloseFunc closep)
{
    if (!f) {
        return NULL;
    }

    InputStream stream = NewWithoutGC(union input_stream);
    init_base_stream(&stream->base, STREAM_BUF_SIZE);
    stream->file.type = IST_FILE;
    stream->file.handle = NewWithoutGC(struct io_file_handle);
    stream->file.handle->f = f;
    if (closep)
        stream->file.handle->close = closep;
    else
        stream->file.handle->close = fclose;
    stream->file.read = (int (*)())file_read;
    stream->file.close = (void (*)())file_close;
    return stream;
}

InputStream
newStrStream(Str s)
{
    InputStream stream;
    if (s == NULL)
        return NULL;
    stream = NewWithoutGC(union input_stream);
    init_str_stream(&stream->base, s);
    stream->str.type = IST_STR;
    stream->str.handle = NULL;
    stream->str.read = (int (*)())str_read;
    stream->str.close = NULL;
    return stream;
}

InputStream
newSSLStream(SSL* ssl, int sock)
{
    InputStream stream;
    if (sock < 0)
        return NULL;
    stream = NewWithoutGC(union input_stream);
    init_base_stream(&stream->base, SSL_BUF_SIZE);
    stream->ssl.type = IST_SSL;
    stream->ssl.handle = NewWithoutGC(struct ssl_handle);
    stream->ssl.handle->ssl = ssl;
    stream->ssl.handle->sock = sock;
    stream->ssl.read = (int (*)())ssl_read;
    stream->ssl.close = (void (*)())ssl_close;
    return stream;
}

InputStream
newEncodedStream(InputStream is, enum StreamEncoding encoding)
{
    InputStream stream;
    if (is == NULL || (encoding != ENC_QUOTE && encoding != ENC_BASE64 && encoding != ENC_UUENCODE))
        return is;
    stream = NewWithoutGC(union input_stream);
    init_base_stream(&stream->base, STREAM_BUF_SIZE);
    stream->ens.type = IST_ENCODED;
    stream->ens.handle = NewWithoutGC(struct ens_handle);
    stream->ens.handle->is = is;
    stream->ens.handle->pos = 0;
    stream->ens.handle->encoding = encoding;
    growbuf_init_without_GC(&stream->ens.handle->gb);
    stream->ens.read = (int (*)())ens_read;
    stream->ens.close = (void (*)())ens_close;
    return stream;
}

int ISclose(InputStream stream)
{
    MySignalFunc prevtrap = 0;
    if (stream == NULL)
        return -1;
    if (stream->base.close != NULL) {
        if (stream->base.type & IST_UNCLOSE) {
            return -1;
        }
        prevtrap = mySignal(SIGINT, SIG_IGN);
        stream->base.close(stream->base.handle);
        mySignal(SIGINT, prevtrap);
    }
    xfree(stream->base.stream.buf);
    xfree(stream);
    return 0;
}

int ISgetc(InputStream stream)
{
    BaseStream base;
    if (stream == NULL)
        return '\0';
    base = &stream->base;
    if (!base->iseos && MUST_BE_UPDATED(base))
        do_update(base);
    return POP_CHAR(base);
}

int ISundogetc(InputStream stream)
{
    StreamBuffer sb;
    if (stream == NULL)
        return -1;
    sb = &stream->base.stream;
    if (sb->cur > 0) {
        sb->cur--;
        return 0;
    }
    return -1;
}

Str StrISgets2(InputStream stream, char crnl)
{
    struct growbuf gb;

    if (stream == NULL)
        return NULL;
    growbuf_init(&gb);
    ISgets_to_growbuf(stream, &gb, crnl);
    return growbuf_to_Str(&gb);
}

void ISgets_to_growbuf(InputStream stream, struct growbuf* gb, char crnl)
{
    BaseStream base = &stream->base;
    StreamBuffer sb = &base->stream;
    int i;

    gb->length = 0;

    while (!base->iseos) {
        if (MUST_BE_UPDATED(base)) {
            do_update(base);
            continue;
        }
        if (crnl && gb->length > 0 && gb->ptr[gb->length - 1] == '\r') {
            if (sb->buf[sb->cur] == '\n') {
                GROWBUF_ADD_CHAR(gb, '\n');
                ++sb->cur;
            }
            break;
        }
        for (i = sb->cur; i < sb->next; ++i) {
            if (sb->buf[i] == '\n' || (crnl && sb->buf[i] == '\r')) {
                ++i;
                break;
            }
        }
        growbuf_append(gb, &sb->buf[sb->cur], i - sb->cur);
        sb->cur = i;
        if (gb->length > 0 && gb->ptr[gb->length - 1] == '\n')
            break;
    }

    growbuf_reserve(gb, gb->length + 1);
    gb->ptr[gb->length] = '\0';
    return;
}

#ifdef unused
int ISread(InputStream stream, Str buf, int count)
{
    int len;

    if (count + 1 > buf->area_size) {
        char* newptr = GC_MALLOC_ATOMIC(count + 1);
        memcpy(newptr, buf->ptr, buf->length);
        newptr[buf->length] = '\0';
        buf->ptr = newptr;
        buf->area_size = count + 1;
    }
    len = ISread_n(stream, buf->ptr, count);
    buf->length = (len > 0) ? len : 0;
    buf->ptr[buf->length] = '\0';
    return (len > 0) ? 1 : 0;
}
#endif

int ISread_n(InputStream stream, char* dst, int count)
{
    int len, l;
    BaseStream base;

    if (stream == NULL || count <= 0)
        return -1;
    if ((base = &stream->base)->iseos)
        return 0;

    len = buffer_read(&base->stream, dst, count);
    if (MUST_BE_UPDATED(base)) {
        l = (*base->read)(base->handle, &dst[len], count - len);
        if (l <= 0) {
            base->iseos = true;
        } else {
            len += l;
        }
    }
    return len;
}

int ISfileno(InputStream stream)
{
    if (stream == NULL)
        return -1;
    switch (IStype(stream) & ~IST_UNCLOSE) {
    case IST_BASIC:
        return *(int*)stream->base.handle;
    case IST_FILE:
        return fileno(stream->file.handle->f);
    case IST_SSL:
        return stream->ssl.handle->sock;
    case IST_ENCODED:
        return ISfileno(stream->ens.handle->is);
    default:
        return -1;
    }
}

int ISeos(InputStream stream)
{
    BaseStream base = &stream->base;
    if (!base->iseos && MUST_BE_UPDATED(base))
        do_update(base);
    return base->iseos;
}

/* Raw level input stream functions */

static void
basic_close(int* handle)
{
    close(*(int*)handle);
    xfree(handle);
}

static int
basic_read(int* handle, char* buf, int len)
{
    return read(*(int*)handle, buf, len);
}

static void
file_close(struct io_file_handle* handle)
{
    handle->close(handle->f);
    xfree(handle);
}

static int
file_read(struct io_file_handle* handle, char* buf, int len)
{
    return fread(buf, 1, len, handle->f);
}

static int
str_read(Str handle, char* buf, int len)
{
    return 0;
}

static void
ens_close(struct ens_handle* handle)
{
    ISclose(handle->is);
    growbuf_clear(&handle->gb);
    xfree(handle);
}

static int
ens_read(struct ens_handle* handle, char* buf, int len)
{
    if (handle->pos == handle->gb.length) {
        char* p;
        struct growbuf gbtmp;

        ISgets_to_growbuf(handle->is, &handle->gb, true);
        if (handle->gb.length == 0)
            return 0;
        if (handle->encoding == ENC_BASE64)
            memchop(handle->gb.ptr, &handle->gb.length);
        else if (handle->encoding == ENC_UUENCODE) {
            if (handle->gb.length >= 5 && !strncmp(handle->gb.ptr, "begin", 5))
                ISgets_to_growbuf(handle->is, &handle->gb, true);
            memchop(handle->gb.ptr, &handle->gb.length);
        }
        growbuf_init_without_GC(&gbtmp);
        p = handle->gb.ptr;
        if (handle->encoding == ENC_QUOTE)
            decodeQP_to_growbuf(&gbtmp, &p);
        else if (handle->encoding == ENC_BASE64)
            decodeB_to_growbuf(&gbtmp, &p);
        else if (handle->encoding == ENC_UUENCODE)
            decodeU_to_growbuf(&gbtmp, &p);
        growbuf_clear(&handle->gb);
        handle->gb = gbtmp;
        handle->pos = 0;
    }

    if (len > handle->gb.length - handle->pos)
        len = handle->gb.length - handle->pos;

    memcpy(buf, &handle->gb.ptr[handle->pos], len);
    handle->pos += len;
    return len;
}

static void
memchop(char* p, int* len)
{
    char* q;

    for (q = p + *len; q > p; --q) {
        if (q[-1] != '\n' && q[-1] != '\r')
            break;
    }
    if (q != p + *len)
        *q = '\0';
    *len = q - p;
    return;
}

void UFhalfclose(struct URLFile* f)
{
    switch (f->scheme) {
    case SCM_FTP:
        break;
    default:
        UFclose(f);
        break;
    }
}

int checkSaveFile(InputStream stream, char* path2)
{
    int des = ISfileno(stream);
    if (des < 0)
        return 0;

    if (*path2 == '|' && PermitSaveToPipe)
        return 0;

    struct stat st1, st2;
    if ((fstat(des, &st1) == 0) && (stat(path2, &st2) == 0))
        if (st1.st_ino == st2.st_ino)
            return -1;
    return 0;
}

int doFileSave(struct URLFile uf, const char* defstr, int current_content_length)
{
    Str msg;
    // Str filen;
    char* p;
    pid_t pid;
    char* lock;
    char* tmpf = NULL;

    // if (fmInitialized)
    {
        p = searchKeyData();
        if (p == NULL || *p == '\0') {
            /* FIXME: gettextize? */
            p = inputLineHist(getUI(), "(Download)Save file to: ",
                defstr, IN_FILENAME, SaveHist);
            if (p == NULL || *p == '\0')
                return -1;
            p = conv_to_system(p);
        }
        if (!notExistsOrOverWrite(p))
            return -1;
        if (checkSaveFile(uf.stream, p) < 0) {
            /* FIXME: gettextize? */
            msg = Sprintf("Can't save. Load file and %s are identical.",
                conv_from_system(p));
            message(getUI(), MSG_ERR, msg->ptr);
            return -1;
        }
        /*
         * if (save2tmp(uf, p) < 0) {
         * msg = Sprintf("Can't save to %s", conv_from_system(p));
         * message(getUI(), MSG_ERR, msg->ptr);
         * }
         */
        lock = tmpfname(TMPF_DFL, ".lock")->ptr;
        symlink(p, lock);
        flush_tty();
        pid = fork();
        if (!pid) {
            int err;
            if ((uf.content_encoding != CMP_NOCOMPRESS) && AutoUncompress) {
                uncompress_stream(&uf, &tmpf);
                if (tmpf)
                    unlink(tmpf);
            }
            setup_child(false, 0, UFfileno(&uf));
            err = save2tmp(uf, p);
            if (err == 0 && PreserveTimestamp && uf.modtime != -1)
                setModtime(p, uf.modtime);
            UFclose(&uf);
            unlink(lock);
            if (err != 0)
                exit(-err);
            exit(0);
        }
        addDownloadList(pid, uf.url, p, lock, current_content_length);
    }
    // else {
    //     q = searchKeyData();
    //     if (q == NULL || *q == '\0') {
    //         /* FIXME: gettextize? */
    //         printf("(Download)Save file to: ");
    //         fflush(stdout);
    //         filen = Strfgets(stdin);
    //         if (filen->length == 0)
    //             return -1;
    //         q = filen->ptr;
    //     }
    //     for (p = q + strlen(q) - 1; IS_SPACE(*p); p--)
    //         ;
    //     *(p + 1) = '\0';
    //     if (*q == '\0')
    //         return -1;
    //     p = expandPath(q);
    //     if (!notExistsOrOverWrite(p))
    //         return -1;
    //     if (checkSaveFile(uf.stream, p) < 0) {
    //         /* FIXME: gettextize? */
    //         printf("Can't save. Load file and %s are identical.", p);
    //         return -1;
    //     }
    //     if (uf.content_encoding != CMP_NOCOMPRESS && AutoUncompress) {
    //         uncompress_stream(&uf, &tmpf);
    //         if (tmpf)
    //             unlink(tmpf);
    //     }
    //     if (save2tmp(uf, p) < 0) {
    //         /* FIXME: gettextize? */
    //         printf("Can't save to %s\n", p);
    //         return -1;
    //     }
    //     if (PreserveTimestamp && uf.modtime != -1)
    //         setModtime(p, uf.modtime);
    // }
    return 0;
}

static sigjmp_buf AbortLoading;
static MySignalHandler KeyAbort(int _dummy)
{
    siglongjmp(AbortLoading, 1);
}

#define SAVE_BUF_SIZE 1536

int save2tmp(struct URLFile uf, char* tmpf)
{
    // long long linelen = 0;
    // long long trbyte = 0;
    MySignalHandler (*prevtrap)(int _dummy) = NULL;
    static sigjmp_buf env_bak;
    int retval = 0;
    char* buf = NULL;

    FILE* ff = fopen(tmpf, "wb");
    if (ff == NULL) {
        /* fclose(f); */
        return -1;
    }
    memcpy(env_bak, AbortLoading, sizeof(sigjmp_buf));
    if (sigsetjmp(AbortLoading, 1) != 0) {
        goto _end;
    }
    TRAP_ON;
    {
        int count;

        buf = NewWithoutGC_N(char, SAVE_BUF_SIZE);
        while ((count = ISread_n(uf.stream, buf, SAVE_BUF_SIZE)) > 0) {
            if (fwrite(buf, 1, count, ff) != count) {
                retval = -2;
                goto _end;
            }
            // linelen += count;
            // showProgress(current_content_length, &linelen, &trbyte);
        }
    }
_end:
    memcpy(AbortLoading, env_bak, sizeof(sigjmp_buf));
    TRAP_OFF;
    xfree(buf);
    fclose(ff);
    // current_content_length = 0;
    return retval;
}

#define NOT_REGULAR(m) (((m) & S_IFMT) != S_IFREG)

void examineFile(struct URLFile* uf, const char* path)
{
    struct stat stbuf;

    uf->guess_type = NULL;
    if (path == NULL || *path == '\0' || stat(path, &stbuf) == -1 || NOT_REGULAR(stbuf.st_mode)) {
        uf->stream = NULL;
        return;
    }
    uf->stream = openIS(path);

    check_compression(uf, path);
    if (uf->compression != CMP_NOCOMPRESS) {
        char* ext = uf->ext;
        const char* t0 = uncompressed_file_type(path, &ext);
        uf->guess_type = (char*)t0;
        uf->ext = ext;
        uncompress_stream(uf, NULL);
        return;
    }
}

Str readAll(struct URLFile* f)
{
    Str html = Strnew();
    Str lineBuf2;
    while ((lineBuf2 = StrmyUFgets(f)) && lineBuf2->length) {
        Strcat(html, lineBuf2);
    }
    return html;
}
