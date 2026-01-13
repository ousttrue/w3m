#include "file.h"
#include "w3m_rc.h"
#include "buffer.h"
#include "mysignal.h"
#include "input_stream.h"
#include "html_builder.h"
#include "indep.h"
#include "mailcap.h"
#include "symbol.h"
#include "message.h"
#include "linein.h"
#include "etc.h"
#include "myctype.h"
#include <libwc/wtf_len.h>
#include <unistd.h>
#include <utime.h>
#include <fcntl.h>
#include <sys/stat.h>

#define SHELLBUFFERNAME "*Shellout*"

static JMP_BUF AbortLoading;

/*
 * loadHTMLBuffer: read file and make new buffer
 */
struct Buffer*
loadHTMLBuffer(struct Url url, struct input_stream* stream, const char* t,
    struct Buffer* newBuf, bool internal)
{
    if (newBuf == NULL)
        newBuf = buf_new(NULL);

    if (newBuf->content->sourcefile == NULL
        && (url.scheme != SCM_LOCAL || newBuf->content->mailcap)) {
        Str tmp = tmpfname(TMPF_SRC, ".html");
        FILE* src = fopen(tmp->ptr, "w");
        if (src) {
            newBuf->content->sourcefile = tmp->ptr;
            is_readall_to_file(stream, src);
            fclose(src);
        }
        return newBuf;
    }

    newBuf->doc = loadHTMLstream(INIT_BUFFER_WIDTH, buf_baseUrl(newBuf), newBuf->content, stream, internal);
    return newBuf;
}

static char* _size_unit[] = { "b", "kb", "Mb", "Gb", "Tb",
    "Pb", "Eb", "Zb", "Bb", "Yb", NULL };

char* convert_size(int64_t size, int usefloat)
{
    float csize;
    int sizepos = 0;
    char** sizes = _size_unit;

    csize = (float)size;
    while (csize >= 999.495 && sizes[sizepos + 1]) {
        csize = csize / 1024.0;
        sizepos++;
    }
    return Sprintf(usefloat ? "%.3g%s" : "%.0f%s",
        floor(csize * 100.0 + 0.5) / 100.0, sizes[sizepos])
        ->ptr;
}

char* convert_size2(int64_t size1, int64_t size2, int usefloat)
{
    char** sizes = _size_unit;
    float csize, factor = 1;
    int sizepos = 0;

    csize = (float)((size1 > size2) ? size1 : size2);
    while (csize / factor >= 999.495 && sizes[sizepos + 1]) {
        factor *= 1024.0;
        sizepos++;
    }
    return Sprintf(usefloat ? "%.3g/%.3g%s" : "%.0f/%.0f%s",
        floor(size1 / factor * 100.0 + 0.5) / 100.0,
        floor(size2 / factor * 100.0 + 0.5) / 100.0,
        sizes[sizepos])
        ->ptr;
}

/*
 * loadHTMLString: read string and make new buffer
 */
struct Buffer*
loadHTMLString(Str page)
{
    MySignalHandler (*volatile prevtrap)(SIGNAL_ARG) = NULL;

    struct input_stream* stream = is_from_str(page);
    struct Buffer* newBuf = buf_new(NULL);
    if (SETJMP(AbortLoading) != 0) {
        TRAP_OFF;
        buf_discard(newBuf);
        is_close(stream);
        return NULL;
    }
    TRAP_ON;

    // newBuf->doc->charset = getRuntime()->InnerCharset;
    newBuf->doc = loadHTMLstream(INIT_BUFFER_WIDTH, buf_baseUrl(newBuf), newBuf->content, stream, TRUE);

    TRAP_OFF;
    is_close(stream);
    return newBuf;
}

struct Buffer*
loadImageBuffer(struct Url url, struct input_stream* stream,
    const char* t, struct Buffer* newBuf, bool internal)
{
    struct Image image;
    struct ImageCache* cache;
    Str tmp, tmpf;
    FILE* src = NULL;
    MySignalHandler (*volatile prevtrap)(SIGNAL_ARG) = NULL;
    const struct Url* pu = newBuf ? &newBuf->content->url : NULL;

    loadImage(IMG_FLAG_STOP);
    image.url = parsedURL2Str(&url)->ptr;
    image.ext = filename_extension(url.file, true);
    image.width = -1;
    image.height = -1;
    image.cache = NULL;
    cache = getImage(&image, (struct Url*)pu, IMG_FLAG_AUTO);
    struct stat st;
    if (!(pu && pu->is_nocache) && cache->loaded & IMG_FLAG_LOADED && !stat(cache->file, &st))
        goto image_buffer;

    TRAP_ON;
    if (!is_save2tmp(stream, cache->file)) {
        TRAP_OFF;
        return NULL;
    }
    TRAP_OFF;

    cache->loaded = IMG_FLAG_LOADED;
    cache->index = 0;

image_buffer:
    if (newBuf == NULL)
        newBuf = buf_new(NULL);
    cache->loaded |= IMG_FLAG_DONT_REMOVE;
    if (newBuf->content->sourcefile == NULL && url.scheme != SCM_LOCAL)
        newBuf->content->sourcefile = cache->file;

    tmp = Sprintf("<img src=\"%s\"><br><br>", html_quote(image.url));
    tmpf = tmpfname(TMPF_SRC, ".html");
    src = fopen(tmpf->ptr, "w");
    if (!src)
        return NULL;

    newBuf->content->mailcap_source = tmpf->ptr;
    struct input_stream* tmp_stream = is_from_str(tmp);
    is_readall_to_file(tmp_stream, src);

    is_close(tmp_stream);
    fclose(src);

    newBuf->doc->topLine = newBuf->doc->firstLine;
    newBuf->doc->lastLine = newBuf->doc->currentLine;
    newBuf->doc->currentLine = newBuf->doc->firstLine;
    newBuf->doc->image_flag = IMG_FLAG_AUTO;
    return newBuf;
}

static Str
conv_symbol(struct Line* l)
{
    Str tmp = NULL;
    char *p = l->lineBuf, *ep = p + l->len;
    Lineprop* pr = l->propBuf;
    int w;
    char** symbol = NULL;

    for (; p < ep; p++, pr++) {
        if (*pr & PC_SYMBOL) {
            char c = ((char)wtf_get_code((wc_uchar*)p) & 0x7f) - SYMBOL_BASE;
            int len = get_mclen(p);
            if (tmp == NULL) {
                tmp = Strnew_size(l->len);
                Strcopy_charp_n(tmp, l->lineBuf, p - l->lineBuf);

                w = (*pr & PC_KANJI) ? 2 : 1;
                symbol = get_symbol(getRuntime()->DisplayCharset, &w);
            }
            Strcat_charp(tmp, symbol[(unsigned char)c % N_SYMBOL]);

            p += len - 1;
            pr += len - 1;

        } else if (tmp != NULL)
            Strcat_char(tmp, *p);
    }
    if (tmp)
        return tmp;
    else
        return Strnew_charp_n(l->lineBuf, l->len);
}

/*
 * saveBuffer: write buffer to file
 */
static void
_saveBuffer(struct Buffer* buf, struct Line* l, FILE* f, int cont)
{
    Str tmp;
    int is_html = FALSE;

    // int set_charset = !getRuntime()->DisplayCharset;
    enum wc_ces charset = getRuntime()->DisplayCharset
        ? getRuntime()->DisplayCharset
        : WC_CES_US_ASCII;
    is_html = is_html_type(buf->content->content_type);

    // pager_next:
    for (; l != NULL; l = l->next) {
        if (is_html)
            tmp = conv_symbol(l);
        else
            tmp = Strnew_charp_n(l->lineBuf, l->len);
        tmp = wc_Str_conv(tmp, getRuntime()->InnerCharset, charset);
        Strfputs(tmp, f);
        if (Strlastchar(tmp) != '\n' && !(cont && l->next && l->next->bpos))
            putc('\n', f);
    }
    // if (buf->pagerSource && !(buf->bufferprop & BP_CLOSE)) {
    //     l = getNextPage(buf, PagerMax);
    //
    //     if (set_charset)
    //         charset = buf->doc.charset;
    //
    //     goto pager_next;
    // }
}

void saveBuffer(struct Buffer* buf, FILE* f, int cont)
{
    _saveBuffer(buf, buf->doc->firstLine, f, cont);
}

void saveBufferBody(struct Buffer* buf, FILE* f, int cont)
{
    struct Line* l = buf->doc->firstLine;

    while (l != NULL && l->real_linenumber == 0)
        l = l->next;
    _saveBuffer(buf, l, f, cont);
}

typedef struct Buffer* (*LoadBufferFunc)(struct Url, struct input_stream*, const char* type, struct Buffer*, bool internal);
static struct Buffer*
loadcmdout(char* cmd,
    LoadBufferFunc loadproc, struct Buffer* defaultbuf)
{
    if (cmd == NULL || *cmd == '\0')
        return NULL;

    FILE* f = popen(cmd, "r");
    if (f == NULL)
        return NULL;

    struct input_stream* stream = is_from_file(f, pclose);
    struct Url url;
    parseURL(cmd, &url, NULL);
    struct Buffer* buf = loadproc(url, stream, NULL,
        defaultbuf, defaultbuf->bufferprop & BP_FRAME);
    is_close(stream);
    return buf;
}

/*
 * getshell: execute shell command and get the result into a buffer
 */
struct Buffer*
getshell(char* cmd)
{
    struct Buffer* buf = loadcmdout(cmd, loadBuffer, NULL);
    if (buf == NULL)
        return NULL;
    buf->content->filename = cmd;
    buf->doc->title = Sprintf("%s %s", SHELLBUFFERNAME, conv_from_system(cmd))->ptr;
    return buf;
}

struct Buffer*
doExternal(struct Url url, struct input_stream* stream,
    const char* type, struct Buffer* defaultbuf, bool internal)
{
    Str command;
    struct mailcap* mcap;
    int mc_stat;
    struct Buffer* buf = NULL;
    const char* src = NULL;
    const char* ext = filename_extension(url.file, true);

    if (!(mcap = searchExtViewer(type)))
        return NULL;

    if (mcap->nametemplate) {
        Str _tmp = unquote_mailcap(mcap->nametemplate, NULL, "", NULL, NULL);
        if (_tmp->ptr[0] == '.')
            ext = _tmp->ptr;
    }

    Str tmpf = tmpfname(TMPF_DFL, (ext && *ext) ? ext : NULL);
    const char* header = checkHeader(defaultbuf->content, "Content-Type:");
    if (header)
        header = conv_to_system(header);
    command = unquote_mailcap(mcap->viewer, type, tmpf->ptr, header, &mc_stat);
    if (!(mc_stat & MCSTAT_REPNAME)) {
        Str tmp = Sprintf("(%s) < %s", command->ptr, shell_quote(tmpf->ptr));
        command = tmp;
    }

    if (!(mcap->flags & (MAILCAP_HTMLOUTPUT | MAILCAP_COPIOUSOUTPUT)) && !(mcap->flags & MAILCAP_NEEDSTERMINAL) && getRuntime()->BackgroundExtViewer) {
        flush_tty();
        if (!fork()) {
            setup_child(FALSE, 0, is_file_no(stream));
            if (!is_save2tmp(stream, tmpf->ptr))
                exit(1);
            is_close(stream);
            myExec(command->ptr);
        }
        return NULL;
    } else {
        if (!is_save2tmp(stream, tmpf->ptr)) {
            return NULL;
        }
    }
    if (mcap->flags & (MAILCAP_HTMLOUTPUT | MAILCAP_COPIOUSOUTPUT)) {
        if (defaultbuf == NULL)
            defaultbuf = buf_new(NULL);
        if (defaultbuf->content && defaultbuf->content->sourcefile)
            src = defaultbuf->content->sourcefile;
        else
            src = tmpf->ptr;
        defaultbuf->content->sourcefile = NULL;
        defaultbuf->content->mailcap = mcap;
    }
    if (mcap->flags & MAILCAP_HTMLOUTPUT) {
        buf = loadcmdout(command->ptr, loadHTMLBuffer, defaultbuf);
        if (buf) {
            buf->content->content_type = "text/html";
            buf->content->mailcap_source = buf->content->sourcefile;
            buf->content->sourcefile = src;
        }
    } else if (mcap->flags & MAILCAP_COPIOUSOUTPUT) {
        buf = loadcmdout(command->ptr, loadBuffer, defaultbuf);
        if (buf) {
            buf->content->content_type = "text/plain";
            buf->content->mailcap_source = buf->content->sourcefile;
            buf->content->sourcefile = src;
        }
    } else {
        if (mcap->flags & MAILCAP_NEEDSTERMINAL || !getRuntime()->BackgroundExtViewer) {
            exitRawMode();
            mySystem(command->ptr, 0);
            enterRawMode();
        } else {
            mySystem(command->ptr, 1);
        }
        buf = NULL;
    }
    if (buf) {
        if ((buf->doc->title == NULL || buf->doc->title[0] == '\0') && buf->content->filename)
            buf->doc->title = conv_from_system(lastFileName(buf->content->filename));
        buf->edit = mcap->edit;
        buf->content->mailcap = mcap;
    }
    return buf;
}

static int
_MoveFile(const char* path1, const char* path2)
{
    struct input_stream* f1 = is_from_fd(open(path1, O_RDONLY));
    if (!f1)
        return -1;

    FILE* f2;
    int is_pipe;
    int64_t linelen = 0, trbyte = 0;
    char* buf = NULL;
    int count;

    if (*path2 == '|' && getRuntime()->PermitSaveToPipe) {
        is_pipe = TRUE;
        f2 = popen(path2 + 1, "w");
    } else {
        is_pipe = FALSE;
        f2 = fopen(path2, "wb");
    }
    if (f2 == NULL) {
        is_close(f1);
        return -1;
    }
    buf = NewWithoutGC_N(char, SAVE_BUF_SIZE);
    while ((count = is_read(f1, buf, SAVE_BUF_SIZE)) > 0) {
        fwrite(buf, 1, count, f2);
        linelen += count;
        showProgress(&linelen, &trbyte, 0);
    }
    xfree(buf);
    is_close(f1);
    if (is_pipe)
        pclose(f2);
    else
        fclose(f2);
    return 0;
}

static int
is_dump_text_type(const char* type)
{
    struct mailcap* mcap;
    return (type && (mcap = searchExtViewer(type)) && (mcap->flags & (MAILCAP_HTMLOUTPUT | MAILCAP_COPIOUSOUTPUT)));
}

static int
is_text_type(const char* type)
{
    return (type == NULL || type[0] == '\0' || strncasecmp(type, "text/", 5) == 0 || (strncasecmp(type, "application/", 12) == 0 && strstr(type, "xhtml") != NULL) || strncasecmp(type, "message/", sizeof("message/") - 1) == 0);
}

static int
is_plain_text_type(const char* type)
{
    return ((type && strcasecmp(type, "text/plain") == 0) || (is_text_type(type) && !is_dump_text_type(type)));
}

int is_html_type(const char* type)
{
    return (type && (strcasecmp(type, "text/html") == 0 || strcasecmp(type, "application/xhtml+xml") == 0));
}

// static FILE*
// lessopen_stream(const char* path)
// {
//     const char* lessopen = getenv("LESSOPEN");
//     if (!lessopen || lessopen[0] == '\0')
//         return NULL;
//     if (lessopen[0] != '|') /* content.filename mode, not supported m(__)m */
//         return NULL;
//
//     /* pipe mode */
//     ++lessopen;
//
//     /* LESSOPEN must contain one conversion specifier for strings ('%s'). */
//     int n = 0;
//     for (const char* f = lessopen; *f; f++) {
//         if (*f == '%') {
//             if (f[1] == '%') /* Literal % */
//                 f++;
//             else if (*++f == 's') {
//                 if (n)
//                     return NULL;
//                 n++;
//             } else
//                 return NULL;
//         }
//     }
//     if (!n)
//         return NULL;
//
//     Str tmpf = Sprintf(lessopen, shell_quote(path));
//     FILE* fp = popen(tmpf->ptr, "r");
//     if (fp == NULL) {
//         return NULL;
//     }
//     int c = getc(fp);
//     if (c == EOF) {
//         pclose(fp);
//         return NULL;
//     }
//     ungetc(c, fp);
//     return fp;
// }

static int
setModtime(const char* path, time_t modtime)
{
    struct utimbuf t;
    struct stat st;
    if (stat(path, &st) == 0)
        t.actime = st.st_atime;
    else
        t.actime = time(NULL);
    t.modtime = modtime;
    return utime(path, &t);
}

int _doFileCopy(const char* tmpf, const char* defstr, bool download)
{
    Str msg;
    Str filen;
    char *p, *q = NULL;
    pid_t pid;
    char* lock;
    struct stat st;
    int is_pipe = FALSE;

    if (fmInitialized()) {
        p = searchKeyData();
        if (p == NULL || *p == '\0') {
            /* FIXME: gettextize? */
            q = inputLineHist("(Download)Save file to: ",
                defstr, IN_COMMAND, getRuntime()->SaveHist);
            if (q == NULL || *q == '\0')
                return FALSE;
            p = conv_to_system(q);
        }
        if (*p == '|' && getRuntime()->PermitSaveToPipe)
            is_pipe = TRUE;
        else {
            if (q) {
                p = unescape_spaces(Strnew_charp(q))->ptr;
                p = conv_to_system(p);
            }
            p = expandPath(p);
            if (checkOverWrite(p) < 0)
                return -1;
        }
        if (checkCopyFile(tmpf, p) < 0) {
            /* FIXME: gettextize? */
            msg = Sprintf("Can't copy. %s and %s are identical.",
                conv_from_system(tmpf), conv_from_system(p));
            disp_err_message(msg->ptr, FALSE);
            return -1;
        }
        if (!download) {
            if (_MoveFile(tmpf, p) < 0) {
                /* FIXME: gettextize? */
                msg = Sprintf("Can't save to %s", conv_from_system(p));
                disp_err_message(msg->ptr, FALSE);
            }
            return -1;
        }
        lock = tmpfname(TMPF_DFL, ".lock")->ptr;
        symlink(p, lock);
        flush_tty();
        pid = fork();
        if (!pid) {
            setup_child(FALSE, 0, -1);
            if (!_MoveFile(tmpf, p) && getRuntime()->PreserveTimestamp && !is_pipe && !stat(tmpf, &st))
                setModtime(p, st.st_mtime);
            unlink(lock);
            exit(0);
        }
    } else {
        q = searchKeyData();
        if (q == NULL || *q == '\0') {
            /* FIXME: gettextize? */
            printf("(Download)Save file to: ");
            fflush(stdout);
            filen = Strfgets(stdin);
            if (filen->length == 0)
                return -1;
            q = filen->ptr;
        }
        for (p = q + strlen(q) - 1; IS_SPACE(*p); p--)
            ;
        *(p + 1) = '\0';
        if (*q == '\0')
            return -1;
        p = q;
        if (*p == '|' && getRuntime()->PermitSaveToPipe)
            is_pipe = TRUE;
        else {
            p = expandPath(p);
            if (checkOverWrite(p) < 0)
                return -1;
        }
        if (checkCopyFile(tmpf, p) < 0) {
            /* FIXME: gettextize? */
            printf("Can't copy. %s and %s are identical.", tmpf, p);
            return -1;
        }
        if (_MoveFile(tmpf, p) < 0) {
            /* FIXME: gettextize? */
            printf("Can't save to %s\n", p);
            return -1;
        }
        if (getRuntime()->PreserveTimestamp && !is_pipe && !stat(tmpf, &st))
            setModtime(p, st.st_mtime);
    }
    return 0;
}

int doFileMove(const char* tmpf, const char* defstr)
{
    int ret = doFileCopy(tmpf, defstr);
    unlink(tmpf);
    return ret;
}

int doFileSave(struct Url url, struct input_stream* stream,
    const char* defstr, enum CompressionType compression)
{
    if (fmInitialized()) {
        const char* p = searchKeyData();
        if (p == NULL || *p == '\0') {
            p = inputLineHist("(Download)Save file to: ",
                defstr, IN_FILENAME, getRuntime()->SaveHist);
            if (p == NULL || *p == '\0')
                return -1;
            p = conv_to_system(p);
        }
        if (checkOverWrite(p) < 0)
            return -1;
        if (checkSaveFile(stream, p) < 0) {
            Str msg = Sprintf("Can't save. Load file and %s are identical.",
                conv_from_system(p));
            disp_err_message(msg->ptr, FALSE);
            return -1;
        }

        const char* lock = tmpfname(TMPF_DFL, ".lock")->ptr;
        symlink(p, lock);
        flush_tty();
        pid_t pid = fork();
        if (!pid) {
            if ((compression != CMP_NOCOMPRESS) && getRuntime()->AutoUncompress) {
                stream = uncompress_stream(stream, compression, NULL);
            }

            setup_child(FALSE, 0, is_file_no(stream));
            bool succeeded = is_save2tmp(stream, p);
            // if (succeeded && getRuntime()->PreserveTimestamp && uf.modtime != -1)
            //     setModtime(p, uf.modtime);
            is_close(stream);
            unlink(lock);
            if (!succeeded)
                exit(1);
            exit(0);
        }
    } else {
        char* q = searchKeyData();
        if (q == NULL || *q == '\0') {
            printf("(Download)Save file to: ");
            fflush(stdout);
            Str filen = Strfgets(stdin);
            if (filen->length == 0)
                return -1;
            q = filen->ptr;
        }
        char* p;
        for (p = q + strlen(q) - 1; IS_SPACE(*p); p--)
            ;
        *(p + 1) = '\0';
        if (*q == '\0')
            return -1;
        p = expandPath(q);
        if (checkOverWrite(p) < 0)
            return -1;
        if (checkSaveFile(stream, p) < 0) {
            printf("Can't save. Load file and %s are identical.", p);
            return -1;
        }
        if (compression != CMP_NOCOMPRESS && getRuntime()->AutoUncompress) {
            stream = uncompress_stream(stream, compression, NULL);
        }
        if (!is_save2tmp(stream, p)) {
            printf("Can't save to %s\n", p);
            return -1;
        }
        // if (getRuntime()->PreserveTimestamp && uf.modtime != -1)
        //     setModtime(p, uf.modtime);
    }
    return 0;
}

int checkCopyFile(const char* path1, const char* path2)
{
    struct stat st1, st2;
    if (*path2 == '|' && getRuntime()->PermitSaveToPipe)
        return 0;
    if ((stat(path1, &st1) == 0) && (stat(path2, &st2) == 0))
        if (st1.st_ino == st2.st_ino)
            return -1;
    return 0;
}

int checkSaveFile(struct input_stream* stream, const char* path2)
{
    int des = is_file_no(stream);
    if (des < 0)
        return 0;
    if (*path2 == '|' && getRuntime()->PermitSaveToPipe)
        return 0;
    struct stat st1, st2;
    if ((fstat(des, &st1) == 0) && (stat(path2, &st2) == 0))
        if (st1.st_ino == st2.st_ino)
            return -1;
    return 0;
}

int checkOverWrite(const char* path)
{
    struct stat st;
    if (stat(path, &st) < 0)
        return 0;

    const char* ans = inputAnswer("File exists. Overwrite? (y/n)");
    if (ans && TOLOWER(*ans) == 'y')
        return 0;
    else
        return -1;
}
