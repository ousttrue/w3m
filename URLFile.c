#include "URLFile.h"
#include "ftp.h"
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>

void init_stream(struct URLFile* uf, int scheme, union input_stream* stream)
{
    memset(uf, 0, sizeof(struct URLFile));
    uf->stream = stream;
    uf->scheme = scheme;
    uf->is_cgi = false;
    uf->compression = CMP_NOCOMPRESS;
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

    uf.stream = newInputStream(open(path, O_RDONLY));
    if (!do_download) {
        check_compression(path, &uf);
        if (uf.compression != CMP_NOCOMPRESS) {
            const char* ext = uf.ext;
            // const char* t0 =
            uncompressed_file_type(path, &ext);
            // uf->guess_type = t0;
            // uf->ext = ext;
            uncompress_stream(&uf, NULL);
        }
    }
    return uf;
}
