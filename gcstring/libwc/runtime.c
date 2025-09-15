#include "runtime.h"
#include "textlist.h"
#include "myctype.h"
#include "quote.h"
#include "url.h"
#include "convertline.h"
#include <Str.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <pwd.h>
#include <string.h>
#include <unistd.h>

// #include "rc.h"
// #include "ui.h"
// #include "textlist.h"
// #include "image.h"

const char* HostName = (NULL);
const char* CurrentDir = 0;
int CurrentPid = -1;
char* tmp_dir = 0;
char* rc_dir = 0;
const char* cgi_bin = (NULL);
const char* document_root = 0;

wc_ces InnerCharset = WC_CES_WTF; /* Don't change */
wc_ces SystemCharset = SYSTEM_CHARSET;
bool DecodeURL = false;

TextList* g_fileToDelete = NULL;

const char* expandPath(const char* name)
{
    if (name == NULL)
        return NULL;

    struct passwd* passent;
    Str extpath = NULL;
    const char* p = name;
    if (*p == '~') {
        p++;
        if (IS_ALPHA(*p)) {
            char* q = strchr(p, '/');
            if (q) { /* ~user/dir... */
                passent = getpwnam(allocStr(p, q - p));
                p = q;
            } else { /* ~user */
                passent = getpwnam(p);
                p = "";
            }
            if (!passent)
                goto rest;
            extpath = Strnew_charp(passent->pw_dir);
        } else if (*p == '/' || *p == '\0') { /* ~/dir... or ~ */
            extpath = Strnew_charp(getenv("HOME"));
        } else
            goto rest;
        if (Strcmp_charp(extpath, "/") == 0 && *p == '/')
            p++;
        Strcat_charp(extpath, p);
        return extpath->ptr;
    }
rest:
    return name;
}

const char* expandName(const char* name)
{
    if (name == NULL)
        return NULL;

    struct passwd* passent;
    Str extpath = NULL;

    const char* p = name;
    if (*p == '/') {
        // if ((*(p + 1) == '~' && IS_ALPHA(*(p + 2))) && personal_document_root) {
        //     char* q;
        //     p += 2;
        //     q = strchr(p, '/');
        //     if (q) { /* /~user/dir... */
        //         passent = getpwnam(allocStr(p, q - p));
        //         p = q;
        //     } else { /* /~user */
        //         passent = getpwnam(p);
        //         p = "";
        //     }
        //     if (!passent)
        //         goto rest;
        //     extpath = Strnew_m_charp(passent->pw_dir, "/",
        //         personal_document_root, NULL);
        //     if (*personal_document_root == '\0' && *p == '/')
        //         p++;
        // }
        // else
        goto rest;
        if (Strcmp_charp(extpath, "/") == 0 && *p == '/')
            p++;
        Strcat_charp(extpath, p);
        return extpath->ptr;
    } else
        return expandPath(p);
rest:
    return name;
}

const char* file_to_url(const char* file, const char* currentDir)
{
    file = expandPath(file);
    if (!file) {
        return NULL;
    }

    if (file[0] != '/') {
        Str tmp = Strnew_charp(currentDir);
        if (Strlastchar(tmp) != '/')
            Strcat_char(tmp, '/');
        Strcat_charp(tmp, file);
        file = tmp->ptr;
    }

    {
        Str tmp = Strnew_charp("file://");
        Strcat_charp(tmp, file_quote(cleanupName(file)));
        return tmp->ptr;
    }
}

static const char*
w3m_dir(const char* name, char* dft)
{
    const char* value = getenv(name);
    return value ? value : dft;
}

const char* w3m_auxbin_dir()
{
    return w3m_dir("W3M_AUXBIN_DIR", AUXBIN_DIR);
}

const char* w3m_lib_dir()
{
    /* FIXME: use W3M_CGIBIN_DIR? */
    return w3m_dir("W3M_LIB_DIR", CGIBIN_DIR);
}

const char* w3m_etc_dir()
{
    return w3m_dir("W3M_ETC_DIR", ETC_DIR);
}

const char* w3m_conf_dir()
{
    return w3m_dir("W3M_CONF_DIR", CONF_DIR);
}

const char* w3m_help_dir()
{
    return w3m_dir("W3M_HELP_DIR", HELP_DIR);
}

const char* rcFile(const char* base)
{
    if (base && (base[0] == '/' || (base[0] == '.' && (base[1] == '/' || (base[1] == '.' && base[2] == '/'))) || (base[0] == '~' && base[1] == '/')))
        /* /file, ./file, ../file, ~/file */
        return expandPath(base);
    return expandPath(Strnew_m_charp(rc_dir, "/", base, NULL)->ptr);
}

const char* auxbinFile(const char* base)
{
    return expandPath(Strnew_m_charp(w3m_auxbin_dir(), "/", base, NULL)->ptr);
}

const char* etcFile(const char* base)
{
    return expandPath(Strnew_m_charp(w3m_etc_dir(), "/", base, NULL)->ptr);
}

const char* confFile(const char* base)
{
    return expandPath(Strnew_m_charp(w3m_conf_dir(), "/", base, NULL)->ptr);
}

void initDeleteFile()
{
    g_fileToDelete = newTextList();
}

void deinitDeleteFile()
{
    for (char* f = popText(g_fileToDelete); f; f = popText(g_fileToDelete)) {
        unlink(f);
        // if (enable_inline_image == INLINE_IMG_SIXEL && strcmp(f + strlen(f) - 4, ".gif") == 0) {
        //     Str firstframe = Strnew_charp(f);
        //     Strcat_charp(firstframe, "-1");
        //     unlink(firstframe->ptr);
        // }
    }
}

void pushDeleteFile(const char* path)
{
    pushText(g_fileToDelete, path);
}

Str tmpfname(enum TmpFileType type, const char* ext)
{
    static char* tmpf_base[MAX_TMPF_TYPE] = {
        "tmp",
        "src",
        "cache",
        "cookie",
        "hist",
    };
    static unsigned int tmpf_seq[MAX_TMPF_TYPE] = { 0 };

    const char* dir = tmp_dir;
    switch (type) {
    case TMPF_HIST:
        dir = rc_dir;
        break;
    case TMPF_DFL:
    case TMPF_COOKIE:
    case TMPF_SRC:
    case TMPF_CACHE:
    default:
        break;
    }

    Str tmpf = Sprintf("%s/w3m%s%d-%d%s",
        dir,
        tmpf_base[type],
        CurrentPid, tmpf_seq[type]++, (ext) ? ext : "");

    pushDeleteFile(tmpf->ptr);
    return tmpf;
}

static const char* url_unquote_conv(const char* url, wc_ces charset)
{
    wc_uint8 old_auto_detect = WcOption.auto_detect;
    Str tmp = Str_url_unquote(Strnew_charp(url), false, true);
    if (!charset || charset == WC_CES_US_ASCII)
        charset = SystemCharset;
    WcOption.auto_detect = WC_OPT_DETECT_ON;
    tmp = convertLine(tmp, RAW_MODE, &charset, charset, InnerCharset);
    WcOption.auto_detect = old_auto_detect;
    return tmp->ptr;
}

const char* url_decode2(const char* url, wc_ces url_charset)
{
    if (!DecodeURL)
        return url;
    return url_unquote_conv(url, url_charset);
}

int _doFileCopy(const char* tmpf, const char* defstr, int download)
{
    // Str msg;
    // // Str filen;
    // char *p, *q = NULL;
    // pid_t pid;
    // char* lock;
    // struct stat st;
    // long long size = 0;
    // bool is_pipe = false;
    //
    // // if (fmInitialized)
    // {
    //     p = searchKeyData();
    //     if (p == NULL || *p == '\0') {
    //         /* FIXME: gettextize? */
    //         q = inputLineHist(getUI(), "(Download)Save file to: ",
    //             defstr, IN_COMMAND, SaveHist);
    //         if (q == NULL || *q == '\0')
    //             return false;
    //         p = conv_to_system(q);
    //     }
    //     if (*p == '|' && PermitSaveToPipe)
    //         is_pipe = true;
    //     else {
    //         if (q) {
    //             p = unescape_spaces(Strnew_charp(q))->ptr;
    //             p = conv_to_system(p);
    //         }
    //         p = expandPath(p);
    //         if (!notExistsOrOverWrite(p))
    //             return -1;
    //     }
    //     if (!canCopyFile(tmpf, p)) {
    //         msg = Sprintf("Can't copy. %s and %s are identical.",
    //             conv_from_system(tmpf), conv_from_system(p));
    //         message(getUI(), MSG_ERR, msg->ptr);
    //         return -1;
    //     }
    //     if (!download) {
    //         if (_MoveFile(tmpf, p) < 0) {
    //             /* FIXME: gettextize? */
    //             msg = Sprintf("Can't save to %s", conv_from_system(p));
    //             message(getUI(), MSG_ERR, msg->ptr);
    //         }
    //         return -1;
    //     }
    //     lock = tmpfname(TMPF_DFL, ".lock")->ptr;
    //
    //     symlink(p, lock);
    //
    //     flush_tty();
    //     pid = fork();
    //     if (!pid) {
    //         setup_child(false, 0, -1);
    //         if (!_MoveFile(tmpf, p) && PreserveTimestamp && !is_pipe && !stat(tmpf, &st))
    //             setModtime(p, st.st_mtime);
    //         unlink(lock);
    //         exit(0);
    //     }
    //     if (!stat(tmpf, &st))
    //         size = st.st_size;
    //     addDownloadList(pid, conv_from_system(tmpf), p, lock, size);
    // }
    //
    // // else {
    // //     q = searchKeyData();
    // //     if (q == NULL || *q == '\0') {
    // //         /* FIXME: gettextize? */
    // //         printf("(Download)Save file to: ");
    // //         fflush(stdout);
    // //         filen = Strfgets(stdin);
    // //         if (filen->length == 0)
    // //             return -1;
    // //         q = filen->ptr;
    // //     }
    // //     for (p = q + strlen(q) - 1; IS_SPACE(*p); p--)
    // //         ;
    // //     *(p + 1) = '\0';
    // //     if (*q == '\0')
    // //         return -1;
    // //     p = q;
    // //     if (*p == '|' && PermitSaveToPipe)
    // //         is_pipe = true;
    // //     else {
    // //         p = expandPath(p);
    // //         if (!notExistsOrOverWrite(p))
    // //             return -1;
    // //     }
    // //     if (checkCopyFile(tmpf, p) < 0) {
    // //         /* FIXME: gettextize? */
    // //         printf("Can't copy. %s and %s are identical.", tmpf, p);
    // //         return -1;
    // //     }
    // //     if (_MoveFile(tmpf, p) < 0) {
    // //         /* FIXME: gettextize? */
    // //         printf("Can't save to %s\n", p);
    // //         return -1;
    // //     }
    // //     if (PreserveTimestamp && !is_pipe && !stat(tmpf, &st))
    // //         setModtime(p, st.st_mtime);
    // // }
    return 0;
}
int doFileCopy(const char* tmpf, const char* defstr)
{
    return _doFileCopy(tmpf, defstr, false);
}
int doFileMove(const char* tmpf, const char* defstr)
{
    int ret = doFileCopy(tmpf, defstr);
    unlink(tmpf);
    return ret;
}
