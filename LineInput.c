#include "LineInput.h"
#include "global.h"
#include "ctrlcode.h"
#include "form.h"
#include "local_cgi.h"
#include "tab.h"
#include "display.h"
#include "history.h"
#include "url.h"
#include "proto.h"
#include "indep.h"
#include "etc.h"
#include "alloc.h"
#include "qsort_util.h"
#include "term_tty.h"
#include "terms.h"
#include <dirent.h>
#include <w3m.h>

#include "wc_util.h"
#include <libwc/conv.h>

struct LineInput LineInputInit(const char* def_str,
    enum InputLineFlags flag,
    enum HistoryType hist)
{
    struct LineInput li = {
        .is_passwd = false,
        .move_word = true,
        .strBuf = Strnew(),
        .CLen = 0,
        .CPos = 0,
    };
    if (def_str) {
        li.strBuf = Strnew_charp(def_str);
        setStrType(&li);
    }
    li.CurrentHist = hist;
    if (hist != HistoryNone) {
        li.use_hist = true;
        li.strCurrentBuf = NULL;
    } else {
        li.use_hist = false;
    }
    if (flag & IN_URL) {
        li.cm_mode = CPL_ALWAYS | CPL_URL;
    } else if (flag & IN_FILENAME) {
        li.cm_mode = CPL_ALWAYS;
    } else if (flag & IN_PASSWORD) {
        li.cm_mode = CPL_NEVER;
        li.is_passwd = true;
        li.move_word = false;
    } else if (flag & IN_COMMAND)
        li.cm_mode = CPL_ON;
    else
        li.cm_mode = CPL_OFF;
    li.offset = 0;
    li.i_cont = true;
    li.i_broken = false;
    li.i_quote = false;
    li.cm_next = false;
    li.cm_disp_next = -1;
    li.need_redraw = false;

    return li;
}

void ins_char(struct LineInput* li, struct CmdArgs* args, Str str)
{
    char *p = str->ptr, *ep = p + str->length;
    Lineprop ctype;
    int len;

    if (li->CLen + str->length >= STR_LEN)
        return;
    while (p < ep) {
        len = get_mclen(p);
        ctype = get_mctype(p);
        if (li->is_passwd) {
            if (ctype & PC_CTRL)
                ctype = PC_ASCII;
            if (ctype & PC_UNKNOWN)
                ctype = PC_WCHAR1;
        }
        insC(args, li);
        li->strBuf->ptr[li->CPos] = *(p++);
        li->strProp[li->CPos] = ctype;
        li->CPos++;
        if (--len) {
            ctype = (ctype & ~PC_WCHAR1) | PC_WCHAR2;
            while (len--) {
                insC(args, li);
                li->strBuf->ptr[li->CPos] = *(p++);
                li->strProp[li->CPos] = ctype;
                li->CPos++;
            }
        }
    }
}

void setStrType(struct LineInput* li)
{
    Lineprop ctype;
    char* p = li->strBuf->ptr;
    char* ep = p + li->strBuf->length;
    int i = 0;
    for (; p < ep;) {
        int len = get_mclen(p);
        if (i + len > STR_LEN)
            break;
        ctype = get_mctype(p);
        if (li->is_passwd) {
            if (ctype & PC_CTRL)
                ctype = PC_ASCII;
            if (ctype & PC_UNKNOWN)
                ctype = PC_WCHAR1;
        }
        li->strProp[i++] = ctype;
        p += len;
        if (--len) {
            ctype = (ctype & ~PC_WCHAR1) | PC_WCHAR2;
            while (len--)
                li->strProp[i++] = ctype;
        }
    }
    li->CLen = i;
    li->CPos = i;
}

static Str
escape_spaces(Str s)
{
    Str tmp = NULL;
    char* p;

    if (s == NULL)
        return s;
    for (p = s->ptr; *p; p++) {
        if (*p == ' ' || *p == CTRL_I) {
            if (tmp == NULL)
                tmp = Strnew_charp_n(s->ptr, (int)(p - s->ptr));
            Strcat_char(tmp, '\\');
        }
        if (tmp)
            Strcat_char(tmp, *p);
    }
    if (tmp)
        return tmp;
    return s;
}
static Str
doComplete(struct LineInput* li, Str ifn, enum CompletionStatus* status, int next)
{
    int fl, i;
    char *fn, *p;
    DIR* d;
    Directory* dir;
    struct stat st;

    if (!li->cm_next) {
        li->NCFileBuf = 0;
        ifn = Str_conv_to_system(ifn->ptr, ifn->length);
        if (li->cm_mode & CPL_ON)
            ifn = unescape_spaces(ifn);
        li->CompleteBuf = Strdup(ifn);
        while (Strlastchar(li->CompleteBuf) != '/' && li->CompleteBuf->length > 0)
            Strshrink(li->CompleteBuf, 1);
        li->CDirBuf = Strdup(li->CompleteBuf);
        if (li->cm_mode & CPL_URL) {
            if (strncmp(li->CompleteBuf->ptr, "file://localhost/", 17) == 0)
                Strdelete(li->CompleteBuf, 0, 16);
            else if (strncmp(li->CompleteBuf->ptr, "file:///", 8) == 0)
                Strdelete(li->CompleteBuf, 0, 7);
            else if (strncmp(li->CompleteBuf->ptr, "file:/", 6) == 0 && li->CompleteBuf->ptr[6] != '/')
                Strdelete(li->CompleteBuf, 0, 5);
            else {
                li->CompleteBuf = Strdup(ifn);
                *status = CPL_FAIL;
                return Str_conv_to_system(li->CompleteBuf->ptr, li->CompleteBuf->length);
            }
        }
        if (li->CompleteBuf->length == 0) {
            Strcat_char(li->CompleteBuf, '.');
        }
        if (Strlastchar(li->CompleteBuf) == '/' && li->CompleteBuf->length > 1) {
            Strshrink(li->CompleteBuf, 1);
        }
        if ((d = opendir(expandPath(li->CompleteBuf->ptr))) == NULL) {
            li->CompleteBuf = Strdup(ifn);
            *status = CPL_FAIL;
            if (li->cm_mode & CPL_ON)
                li->CompleteBuf = escape_spaces(li->CompleteBuf);
            return li->CompleteBuf;
        }
        fn = lastFileName(ifn->ptr);
        fl = strlen(fn);
        li->CFileName = Strnew();
        for (;;) {
            dir = readdir(d);
            if (dir == NULL)
                break;
            if (fl == 0
                && (!strcmp(dir->d_name, ".") || !strcmp(dir->d_name, "..")))
                continue;
            if (!strncmp(dir->d_name, fn, fl)) { /* match */
                li->NCFileBuf++;
                li->CFileBuf = New_Reuse(char*, li->CFileBuf, li->NCFileBuf);
                li->CFileBuf[li->NCFileBuf - 1] = NewAtom_N(char, strlen(dir->d_name) + 1);
                strcpy(li->CFileBuf[li->NCFileBuf - 1], dir->d_name);
                if (li->NCFileBuf == 1) {
                    li->CFileName = Strnew_charp(dir->d_name);
                } else {
                    for (i = 0; li->CFileName->ptr[i] == dir->d_name[i]; i++)
                        ;
                    Strtruncate(li->CFileName, i);
                }
            }
        }
        closedir(d);
        if (li->NCFileBuf == 0) {
            li->CompleteBuf = Strdup(ifn);
            *status = CPL_FAIL;
            if (li->cm_mode & CPL_ON)
                li->CompleteBuf = escape_spaces(li->CompleteBuf);
            return li->CompleteBuf;
        }
        qsort(li->CFileBuf, li->NCFileBuf, sizeof(li->CFileBuf[0]), strCmp);
        li->NCFileOffset = 0;
        if (li->NCFileBuf >= 2) {
            li->cm_next = true;
            *status = CPL_AMBIG;
        } else {
            *status = CPL_OK;
        }
    } else {
        li->CFileName = Strnew_charp(li->CFileBuf[li->NCFileOffset]);
        li->NCFileOffset = (li->NCFileOffset + next + li->NCFileBuf) % li->NCFileBuf;
        *status = CPL_MENU;
    }
    li->CompleteBuf = Strdup(li->CDirBuf);
    if (li->CompleteBuf->length && Strlastchar(li->CompleteBuf) != '/')
        Strcat_char(li->CompleteBuf, '/');
    Strcat(li->CompleteBuf, li->CFileName);
    if (*status != CPL_AMBIG) {
        p = li->CompleteBuf->ptr;
        if (li->cm_mode & CPL_URL) {
            if (strncmp(p, "file://localhost/", 17) == 0)
                p = &p[16];
            else if (strncmp(p, "file:///", 8) == 0)
                p = &p[7];
            else if (strncmp(p, "file:/", 6) == 0 && p[6] != '/')
                p = &p[5];
        }
        if (stat(expandPath(p), &st) != -1 && S_ISDIR(st.st_mode))
            Strcat_char(li->CompleteBuf, '/');
    }
    if (li->cm_mode & CPL_ON)
        li->CompleteBuf = escape_spaces(li->CompleteBuf);
    return Str_conv_from_system(li->CompleteBuf->ptr, li->CompleteBuf->length);
}

void next_compl(struct LineInput* li, int next)
{
    enum CompletionStatus status;
    int b, a;
    Str buf;
    Str s;

    if (li->cm_mode == CPL_NEVER || li->cm_mode & CPL_OFF)
        return;
    li->cm_clear = false;
    if (!li->cm_next) {
        if (li->cm_mode & CPL_ALWAYS) {
            b = 0;
        } else {
            for (b = li->CPos - 1; b >= 0; b--) {
                if ((li->strBuf->ptr[b] == ' ' || li->strBuf->ptr[b] == CTRL_I) && !((b > 0) && li->strBuf->ptr[b - 1] == '\\'))
                    break;
            }
            b++;
        }
        a = li->CPos;
        li->CBeforeBuf = Strsubstr(li->strBuf, 0, b);
        buf = Strsubstr(li->strBuf, b, a - b);
        li->CAfterBuf = Strsubstr(li->strBuf, a, li->strBuf->length - a);
        s = doComplete(li, buf, &status, next);
    } else {
        s = doComplete(li, li->strBuf, &status, next);
    }
    if (next == 0)
        return;

    if (status != CPL_OK && status != CPL_MENU)
        bell();
    if (status == CPL_FAIL)
        return;

    li->strBuf = Strnew_m_charp(li->CBeforeBuf->ptr, s->ptr, li->CAfterBuf->ptr, NULL);
    setStrType(li);
    li->CPos = li->CBeforeBuf->length + s->length;
    if (li->CPos > li->CLen)
        li->CPos = li->CLen;
}

void next_dcompl(struct LineInput* li, struct CmdArgs* args, int next)
{
    static int col, row;
    static unsigned int len;
    static Str d;
    int i, j, n, y;
    Str f;
    char* p;
    struct stat st;
    int comment, nline;

    if (li->cm_mode == CPL_NEVER || li->cm_mode & CPL_OFF)
        return;
    li->cm_disp_clear = false;
    if (CurrentTab)
        displayBuffer(args, B_FORCE_REDRAW);
    if ((LINES - 1) >= 3) {
        comment = true;
        nline = (LINES - 1) - 2;
    } else if ((LINES - 1)) {
        comment = false;
        nline = (LINES - 1);
    } else {
        return;
    }

    if (li->cm_disp_next >= 0) {
        if (next == 1) {
            li->cm_disp_next += col * nline;
            if (li->cm_disp_next >= li->NCFileBuf)
                li->cm_disp_next = 0;
        } else if (next == -1) {
            li->cm_disp_next -= col * nline;
            if (li->cm_disp_next < 0)
                li->cm_disp_next = 0;
        }
        row = (li->NCFileBuf - li->cm_disp_next + col - 1) / col;
        goto disp_next;
    }

    li->cm_next = false;
    next_compl(li, 0);
    if (li->NCFileBuf == 0)
        return;
    li->cm_disp_next = 0;

    d = Str_conv_to_system(li->CDirBuf->ptr, li->CDirBuf->length);
    if (d->length > 0 && Strlastchar(d) != '/')
        Strcat_char(d, '/');
    if (li->cm_mode & CPL_URL && d->ptr[0] == 'f') {
        p = d->ptr;
        if (strncmp(p, "file://localhost/", 17) == 0)
            p = &p[16];
        else if (strncmp(p, "file:///", 8) == 0)
            p = &p[7];
        else if (strncmp(p, "file:/", 6) == 0 && p[6] != '/')
            p = &p[5];
        d = Strnew_charp(p);
    }

    len = 0;
    for (i = 0; i < li->NCFileBuf; i++) {
        n = strlen(li->CFileBuf[i]) + 3;
        if (len < n)
            len = n;
    }
    if (len > 0 && COLS > len)
        col = COLS / len;
    else
        col = 1;
    row = (li->NCFileBuf + col - 1) / col;

disp_next:
    if (comment) {
        if (row > nline) {
            row = nline;
            y = 0;
        } else
            y = nline - row + 1;
    } else {
        if (row >= nline) {
            row = nline;
            y = 0;
        } else
            y = nline - row - 1;
    }
    if (y) {
        move(y - 1, 0);
        clrtoeolx();
    }
    if (comment) {
        move(y, 0);
        clrtoeolx();
        bold();
        /* FIXME: gettextize? */
        addstr("----- Completion list -----");
        boldend();
        y++;
    }
    for (i = 0; i < row; i++) {
        for (j = 0; j < col; j++) {
            n = li->cm_disp_next + j * row + i;
            if (n >= li->NCFileBuf)
                break;
            move(y, j * len);
            clrtoeolx();
            f = Strdup(d);
            Strcat_charp(f, li->CFileBuf[n]);
            addstr(conv_from_system(li->CFileBuf[n]));
            if (stat(expandPath(f->ptr), &st) != -1 && S_ISDIR(st.st_mode))
                addstr("/");
        }
        y++;
    }
    if (comment && y == (LINES - 1) - 1) {
        move(y, 0);
        clrtoeolx();
        bold();
        if (emacs_like_lineedit)
            /* FIXME: gettextize? */
            addstr("----- Press TAB to continue -----");
        else
            /* FIXME: gettextize? */
            addstr("----- Press CTRL-D to continue -----");
        boldend();
    }
}

int iself(struct CmdArgs* args, struct LineInput* li)
{
    if (li->CLen >= STR_LEN)
        return 0;
    insC(args, li);
    li->strBuf->ptr[li->CPos] = args->ch;
    li->strProp[li->CPos] = (li->is_passwd) ? PC_ASCII : PC_CTRL;
    li->CPos++;
    return 0;
}

int _mvR(struct CmdArgs* args, struct LineInput* li)
{
    if (li->CPos < li->CLen)
        li->CPos++;
    while (li->CPos < li->CLen && li->strProp[li->CPos] & PC_WCHAR2)
        li->CPos++;
    return 0;
}

int _mvL(struct CmdArgs* args, struct LineInput* li)
{
    if (li->CPos > 0)
        li->CPos--;
    while (li->CPos > 0 && li->strProp[li->CPos] & PC_WCHAR2)
        li->CPos--;
    return 0;
}

int terminated(unsigned char c)
{
    int termchar[] = { '/', '&', '?', ' ', -1 };
    int* tp;

    for (tp = termchar; *tp > 0; tp++) {
        if (c == *tp) {
            return 1;
        }
    }

    return 0;
}

int _mvRw(struct CmdArgs* args, struct LineInput* li)
{
    int first = 1;
    while (li->CPos < li->CLen && (first || !terminated(li->strBuf->ptr[li->CPos - 1]))) {
        li->CPos++;
        first = 0;
        if (li->CPos < li->CLen && li->strProp[li->CPos] & PC_WCHAR2)
            li->CPos++;
        if (!li->move_word)
            break;
    }
    return 0;
}

int _mvLw(struct CmdArgs* args, struct LineInput* li)
{
    int first = 1;
    while (li->CPos > 0 && (first || !terminated(li->strBuf->ptr[li->CPos - 1]))) {
        li->CPos--;
        first = 0;
        if (li->CPos > 0 && li->strProp[li->CPos] & PC_WCHAR2)
            li->CPos--;
        if (!li->move_word)
            break;
    }
    return 0;
}

int delC(struct CmdArgs* args, struct LineInput* li)
{
    if (li->CLen == li->CPos)
        return 0;

    int i = li->CPos;
    int delta = 1;
    while (i + delta < li->CLen && li->strProp[i + delta] & PC_WCHAR2)
        delta++;
    for (i = li->CPos; i < li->CLen; i++) {
        li->strProp[i] = li->strProp[i + delta];
    }
    Strdelete(li->strBuf, li->CPos, delta);
    li->CLen -= delta;
    return 0;
}

int insC(struct CmdArgs* args, struct LineInput* li)
{
    Strinsert_char(li->strBuf, li->CPos, ' ');
    li->CLen = li->strBuf->length;
    for (int i = li->CLen; i > li->CPos; i--) {
        li->strProp[i] = li->strProp[i - 1];
    }
    return 0;
}

int _mvB(struct CmdArgs* args, struct LineInput* li)
{
    li->CPos = 0;
    return 0;
}

int _mvE(struct CmdArgs* args, struct LineInput* li)
{
    li->CPos = li->CLen;
    return 0;
}

int _enter(struct CmdArgs* args, struct LineInput* li)
{
    li->i_cont = false;
    return 0;
}

int _quo(struct CmdArgs* args, struct LineInput* li)
{
    li->i_quote = true;
    return 0;
}

int _bs(struct CmdArgs* args, struct LineInput* li)
{
    if (li->CPos > 0) {
        _mvL(args, li);
        delC(args, li);
    }
    return 0;
}

int _bsw(struct CmdArgs* args, struct LineInput* li)
{
    int t = 0;
    while (li->CPos > 0 && !t) {
        _mvL(args, li);
        t = (li->move_word && terminated(li->strBuf->ptr[li->CPos - 1]));
        delC(args, li);
    }
    return 0;
}

int killn(struct CmdArgs* args, struct LineInput* li)
{
    li->CLen = li->CPos;
    Strtruncate(li->strBuf, li->CLen);
    return 0;
}

int killb(struct CmdArgs* args, struct LineInput* li)
{
    while (li->CPos > 0)
        _bs(args, li);
    return 0;
}

int _inbrk(struct CmdArgs* args, struct LineInput* li)
{
    li->i_cont = false;
    li->i_broken = true;
    return 0;
}

int _esc(struct CmdArgs* args, struct LineInput* li)
{
    args->ch = getch(args);
    switch (args->ch) {
    case '[':
    case 'O':
        switch (args->ch = getch(args)) {
        case 'A':
            _prev(args, li);
            break;
        case 'B':
            _next(args, li);
            break;
        case 'C':
            _mvR(args, li);
            break;
        case 'D':
            _mvL(args, li);
            break;
        }
        break;
    case CTRL_I:
    case ' ':
        if (emacs_like_lineedit) {
            _rdcompl(args, li);
            li->cm_clear = false;
            li->need_redraw = true;
        } else
            _rcompl(args, li);
        break;
    case CTRL_D:
        if (!emacs_like_lineedit)
            _rdcompl(args, li);
        li->need_redraw = true;
        break;
    case 'f':
        if (emacs_like_lineedit)
            _mvRw(args, li);
        break;
    case 'b':
        if (emacs_like_lineedit)
            _mvLw(args, li);
        break;
    case CTRL_H:
        if (emacs_like_lineedit)
            _bsw(args, li);
        break;
    default:
        if (wc_char_conv(WcOption, ESC_CODE).data == NULL && wc_char_conv(WcOption, args->ch).data == NULL)
            li->i_quote = true;
    }

    return 0;
}

int _editor(struct CmdArgs* args, struct LineInput* li)
{
    if (li->is_passwd)
        return 0;

    struct FormItem fi;
    fi.readonly = false;
    fi.value = Strdup(li->strBuf);
    Strcat_char(fi.value, '\n');

    input_textarea(args, &fi);

    li->strBuf = Strnew();
    for (char* p = fi.value->ptr; *p; p++) {
        if (*p == '\r' || *p == '\n')
            continue;
        Strcat_char(li->strBuf, *p);
    }
    setStrType(li);
    if (CurrentTab)
        displayBuffer(args, B_FORCE_REDRAW);
    return 0;
}

int _prev(struct CmdArgs* args, struct LineInput* li)
{
    enum HistoryType hist = li->CurrentHist;
    if (!li->use_hist)
        return 0;
    const char* p;
    if (li->strCurrentBuf) {
        p = prevHist(hist);
        if (p == NULL)
            return 0;
    } else {
        p = lastHist(hist);
        if (p == NULL)
            return 0;
        li->strCurrentBuf = li->strBuf;
    }
    if (DecodeURL && (li->cm_mode & CPL_URL))
        p = url_decode2(p, NULL);
    li->strBuf = Strnew_charp(p);
    setStrType(li);
    li->offset = 0;
    return 0;
}

int _next(struct CmdArgs* args, struct LineInput* li)
{
    enum HistoryType hist = li->CurrentHist;

    if (!li->use_hist)
        return 0;

    if (li->strCurrentBuf == NULL)
        return 0;

    const char* p = nextHist(hist);
    if (p) {
        if (DecodeURL && (li->cm_mode & CPL_URL))
            p = url_decode2(p, NULL);
        li->strBuf = Strnew_charp(p);
    } else {
        li->strBuf = li->strCurrentBuf;
        li->strCurrentBuf = NULL;
    }
    setStrType(li);
    li->offset = 0;
    return 0;
}

int _compl(struct CmdArgs* args, struct LineInput* li)
{
    next_compl(li, 1);
    return 0;
}

int _tcompl(struct CmdArgs* args, struct LineInput* li)
{
    if (li->cm_mode & CPL_OFF)
        li->cm_mode = CPL_ON;
    else if (li->cm_mode & CPL_ON)
        li->cm_mode = CPL_OFF;
    return 0;
}

int _dcompl(struct CmdArgs* args, struct LineInput* li)
{
    next_dcompl(li, args, 1);
    return 0;
}

int _rdcompl(struct CmdArgs* args, struct LineInput* li)
{
    next_dcompl(li, args, -1);
    return 0;
}

int _rcompl(struct CmdArgs* args, struct LineInput* li)
{
    next_compl(li, -1);
    return 0;
}

