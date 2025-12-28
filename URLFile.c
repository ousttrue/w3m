#include "URLFile.h"
#include "compression.h"
#include "alloc.h"
#include "file.h"
#include "fm.h"
#include "ftp.h"
#include "w3m_rc.h"
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <setjmp.h>
#include <signal.h>

void init_stream(struct URLFile* uf, int scheme, struct input_stream* stream)
{
    memset(uf, 0, sizeof(struct URLFile));
    uf->stream = stream;
    uf->scheme = scheme;
    uf->is_cgi = false;
    uf->ext = NULL;
    uf->modtime = -1;
}

void UFhalfclose(struct URLFile* f)
{
    switch (f->scheme) {
    case SCM_FTP:
        closeFTP();
        break;
    default:
        UFclose(f);
        break;
    }
}

#define NOT_REGULAR(m) (((m) & S_IFMT) != S_IFREG)

struct URLFile examineFile(const char* path, bool do_download)
{
    struct URLFile uf = { 0 };
    if (path == NULL || *path == '\0') {
        return uf;
    }

    struct stat stbuf;
    if (stat(path, &stbuf) != 0) {
        return uf;
    }
    if (NOT_REGULAR(stbuf.st_mode)) {
        return uf;
    }

    uf.stream = is_from_fd(open(path, O_RDONLY));
    if (!do_download) {
        enum CompressionType compression = check_compression(path);
        if (compression != CMP_NOCOMPRESS) {
            const char* ext = uf.ext;
            // const char* t0 =
            uncompressed_file_type(path, &ext);
            // uf->guess_type = t0;
            // uf->ext = ext;
            uncompress_stream(&uf, compression, NULL);
        }
    }
    return uf;
}

static JMP_BUF AbortLoading;
static MySignalHandler
KeyAbort(SIGNAL_ARG)
{
    LONGJMP(AbortLoading, 1);
    SIGNAL_RETURN;
}
bool uf_save2tmp(struct URLFile uf, const char* tmpf)
{
    FILE* ff = fopen(tmpf, "wb");
    if (ff == NULL) {
        return false;
    }
    static JMP_BUF env_bak;
    memcpy(env_bak, AbortLoading, sizeof(JMP_BUF));
    if (SETJMP(AbortLoading) != 0) {
        goto _end;
    }

    int retval = 0;
    int64_t linelen = 0;
    int64_t trbyte = 0;
    MySignalHandler (*volatile prevtrap)(SIGNAL_ARG) = NULL;
    char* buf = NULL;
    TRAP_ON;
    {
        buf = NewWithoutGC_N(char, SAVE_BUF_SIZE);
        int count;
        while ((count = is_read(uf.stream, buf, SAVE_BUF_SIZE)) > 0) {
            if (fwrite(buf, 1, count, ff) != count) {
                retval = -2;
                goto _end;
            }
            linelen += count;
            showProgress(&linelen, &trbyte, 0);
        }
    }
_end:
    bcopy(env_bak, AbortLoading, sizeof(JMP_BUF));
    TRAP_OFF;
    xfree(buf);
    fclose(ff);
    return retval;
}
