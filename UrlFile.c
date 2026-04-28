#include "UrlFile.h"
#include <w3m.h>
#include "line_input.h"
#include "display.h"
#include "form.h"
#include "local_cgi.h"
#include "http_request.h"
#include "global.h"
#include "url.h"
#include "content_type.h"
#include "input_stream.h"
#include "indep.h"
#include "textlist.h"
#include "etc.h"
#include "myctype.h"
#include "proxy.h"
#include <fcntl.h>
#include <strings.h>
#include <unistd.h>

#include <openssl/ssl.h>
#ifndef SSLEAY_VERSION_NUMBER
#include <openssl/crypto.h> /* SSLEAY_VERSION_NUMBER may be here */
#endif
#include <openssl/err.h>
#include <openssl/x509v3.h>
#include <openssl/bio.h>
#include <openssl/x509.h>

struct URLFile init_stream(struct Url url, struct InputStream* stream)
{
    return (struct URLFile) {
        .url = url,
        .stream = stream,
        .is_cgi = false,
        .compression = CMP_NOCOMPRESS,
        .guess_type = NULL,
        .modtime = -1,
        .ssl_certificate = NULL,
    };
}

static FILE*
lessopen_stream(const char* path)
{
    char* lessopen;
    FILE* fp;
    Str tmpf;
    int c, n = 0;

    lessopen = getenv("LESSOPEN");
    if (lessopen == NULL || lessopen[0] == '\0')
        return NULL;

    if (lessopen[0] != '|') /* filename mode, not supported m(__)m */
        return NULL;

    /* pipe mode */
    ++lessopen;

    /* LESSOPEN must contain one conversion specifier for strings ('%s'). */
    for (const char* f = lessopen; *f; f++) {
        if (*f == '%') {
            if (f[1] == '%') /* Literal % */
                f++;
            else if (*++f == 's') {
                if (n)
                    return NULL;
                n++;
            } else
                return NULL;
        }
    }
    if (!n)
        return NULL;

    tmpf = Sprintf(lessopen, shell_quote(path));
    fp = popen(tmpf->ptr, "r");
    if (fp == NULL) {
        return NULL;
    }
    c = getc(fp);
    if (c == EOF) {
        pclose(fp);
        return NULL;
    }
    ungetc(c, fp);
    return fp;
}

#define NOT_REGULAR(m) (((m) & S_IFMT) != S_IFREG)

struct URLFile examineFile(const char* path)
{
    struct URLFile uf = init_stream((struct Url) { 0 }, NULL);

    struct stat stbuf;
    if (path == NULL || *path == '\0' || stat(path, &stbuf) == -1 || NOT_REGULAR(stbuf.st_mode)) {
        uf.stream = NULL;
        return uf;
    }

    uf.stream = ist_from_path(path);
    if (!do_download) {
        if (use_lessopen && getenv("LESSOPEN") != NULL) {
            uf.guess_type = guessContentType(path);
            if (uf.guess_type == NULL)
                uf.guess_type = "text/plain";
            if (is_html_type(uf.guess_type))
                return uf;
            FILE* fp;
            if ((fp = lessopen_stream(path))) {
                UFclose(&uf);
                uf.stream = ist_from_fp(fp, pclose);
                uf.guess_type = "text/plain";
                return uf;
            }
        }

        // check_compression(&uf, path);
        uf.compression = CMP_NOCOMPRESS;
        struct CompressionDecoder* d = compression_from_path(path);
        if (d) {
            uf.compression = d->type;
            uf.guess_type = d->mime_type;
        }

        if (uf.compression != CMP_NOCOMPRESS) {
            struct ContentTypeWithExt ce = compression_from_path_to_content_type(path);
            uf.guess_type = ce.content_type;
            struct Uncompressed uncompressed = uncompressed_pipe(&uf, compression_from_type(uf.compression));
            if (uncompressed.pipe) {
                // if (uncompressed.tmpf) {
                //     // if (src)
                //     //     *src = tmpf;
                //     // else
                // }
                uf.stream = ist_from_fp(uncompressed.pipe, fclose);
                uf.url.scheme = SCM_FILE;
            }
            return uf;
        }
    }

    return uf;
}

void UFclose(struct URLFile* f)
{
    if (ist_destroy(f->stream)) {
        f->stream = NULL;
    }
}

static const char* auxbinFile(const char* base)
{
    return expandPath(Strnew_m_charp(w3m_auxbin_dir(), "/", base, NULL)->ptr);
}

#define SAVE_BUF_SIZE 1536

struct Uncompressed uncompressed_pipe(struct URLFile* uf, struct CompressionDecoder* d)
{
    if (!d) {
        return (struct Uncompressed) { 0 };
    }

    const char* tmpf = NULL;
    if (uf->url.scheme != SCM_FILE
        && !image_source) {
        tmpf = tmpfname(TMPF_DFL, d->ext);
    }

    // child1 --> stdout(f1)
    FILE* f1;
    pid_t pid1 = open_pipe_rw(&f1, NULL);
    if (pid1 < 0) {
        UFclose(uf);
        return (struct Uncompressed) { 0 };
    } else if (pid1 == 0) {
        // child
        // uf -> child2 -- stdout|stdin -> child1
        FILE* f2 = stdin;
        pid_t pid2 = open_pipe_rw(&f2, NULL);
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
                // to pipe
                if (fwrite(buf, 1, count, stdout) != count)
                    break;
                // to tmpf
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

        const char* expand_cmd = GUNZIP_CMDNAME;
        if (d->auxbin_p)
            expand_cmd = auxbinFile(d->cmd);
        else
            expand_cmd = d->cmd;

        if (d->use_d_arg)
            execlp(expand_cmd, d->name, "-d", NULL);
        else
            execlp(expand_cmd, d->name, NULL);
        exit(1);
    } else {
        UFclose(uf);
        return (struct Uncompressed) {
            .tmpf = tmpf,
            .pipe = f1,
        };
    }
}
