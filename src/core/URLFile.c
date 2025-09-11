#include "URLFile.h"
#include "runtime.h"
#include "buffer_loader.h"
#include "compression.h"
#include "downloadlist.h"
#include "istream.h"
#include "keymap.h"
#include "linein.h"
#include "history.h"
#include "subprocess.h"
#include "tty.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void init_stream(struct URLFile* uf, int scheme, InputStream stream)
{
    memset(uf, 0, sizeof(struct URLFile));
    uf->stream = stream;
    uf->scheme = scheme;
    uf->encoding = ENC_7BIT;
    uf->is_cgi = false;
    uf->compression = CMP_NOCOMPRESS;
    uf->content_encoding = CMP_NOCOMPRESS;
    uf->guess_type = NULL;
    uf->ext = NULL;
    uf->modtime = -1;
}

static bool canSaveFile(InputStream stream, const char* path2)
{
    int des = ISfileno(stream);
    if (des < 0)
        return true;

    if (*path2 == '|' && PermitSaveToPipe)
        return true;

    struct stat st1, st2;
    if ((fstat(des, &st1) == 0) && (stat(path2, &st2) == 0))
        if (st1.st_ino == st2.st_ino)
            return false;
    return true;
}

int doFileSave(struct URLFile uf, const char* defstr, int current_content_length)
{
    // if (fmInitialized)
    {
        Str msg;
        // Str filen;
        pid_t pid;
        char* lock;
        char* tmpf = NULL;
        const char* p = searchKeyData();
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
        if (!canSaveFile(uf.stream, p)) {
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
            if ((uf.content_encoding != CMP_NOCOMPRESS) /*&& AutoUncompress*/) {
                abort();
                // uncompress_stream(&uf, &tmpf);
                // if (tmpf)
                //     unlink(tmpf);
            }
            setup_child(false, 0, ISfileno(uf.stream));
            err = save2tmp(uf.stream, p);
            if (err == 0 && PreserveTimestamp && uf.modtime != -1)
                setModtime(p, uf.modtime);
            if (ISclose(uf.stream) == 0) {
                uf.stream = NULL;
            }
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

// #define SAVE_BUF_SIZE 1536
//
// void uncompress_stream(struct URLFile* uf, char** src)
// {
//     pid_t pid1;
//     FILE* f1;
//     char* expand_cmd = GUNZIP_CMDNAME;
//     char* expand_name = GUNZIP_NAME;
//     char* tmpf = NULL;
//     char* ext = NULL;
//     struct compression_decoder* d;
//     int use_d_arg = 0;
//
//     if (IStype(uf->stream) != IST_ENCODED) {
//         uf->stream = newEncodedStream(uf->stream, uf->encoding);
//         uf->encoding = ENC_7BIT;
//     }
//     for (d = compression_decoders; d->type != CMP_NOCOMPRESS; d++) {
//         if (uf->compression == d->type) {
//             if (d->auxbin_p)
//                 expand_cmd = auxbinFile(d->cmd);
//             else
//                 expand_cmd = d->cmd;
//             expand_name = d->name;
//             ext = d->ext;
//             use_d_arg = d->use_d_arg;
//             break;
//         }
//     }
//     uf->compression = CMP_NOCOMPRESS;
//
//     if (uf->scheme != SCM_LOCAL
//         && !image_source) {
//         tmpf = tmpfname(TMPF_DFL, ext)->ptr;
//     }
//
//     /* child1 -- stdout|f1=uf -> parent */
//     pid1 = open_pipe_rw(&f1, NULL);
//     if (pid1 < 0) {
//         if (ISclose(uf->stream) == 0) {
//             uf->stream = NULL;
//         }
//         return;
//     }
//     if (pid1 == 0) {
//         /* child */
//         pid_t pid2;
//         FILE* f2 = stdin;
//
//         /* uf -> child2 -- stdout|stdin -> child1 */
//         pid2 = open_pipe_rw(&f2, NULL);
//         if (pid2 < 0) {
//             if (ISclose(uf->stream) == 0) {
//                 uf->stream = NULL;
//             }
//             exit(1);
//         }
//         if (pid2 == 0) {
//             /* child2 */
//             char* buf = NewWithoutGC_N(char, SAVE_BUF_SIZE);
//             int count;
//             FILE* f = NULL;
//
//             setup_child(true, 2, ISfileno(uf->stream));
//             if (tmpf)
//                 f = fopen(tmpf, "wb");
//             while ((count = ISread_n(uf->stream, buf, SAVE_BUF_SIZE)) > 0) {
//                 if (fwrite(buf, 1, count, stdout) != count)
//                     break;
//                 if (f && fwrite(buf, 1, count, f) != count)
//                     break;
//             }
//             if (ISclose(uf->stream) == 0) {
//                 uf->stream = NULL;
//             }
//             if (f)
//                 fclose(f);
//             xfree(buf);
//             exit(0);
//         }
//         /* child1 */
//         dup2(1, 2); /* stderr>&stdout */
//         setup_child(true, -1, -1);
//         if (use_d_arg)
//             execlp(expand_cmd, expand_name, "-d", NULL);
//         else
//             execlp(expand_cmd, expand_name, NULL);
//         exit(1);
//     }
//     if (tmpf) {
//         if (src)
//             *src = tmpf;
//         else
//             uf->scheme = SCM_LOCAL;
//     }
//     UFhalfclose(uf);
//     uf->stream = newFileStream(f1, &fclose);
// }
