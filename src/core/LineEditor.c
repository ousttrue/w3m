#include "LineEditor.h"
#include "str_util.h"
#include "display.h"
#include "etc.h"
#include "ctrlcode.h"
#include "local.h"
#include "term_renderer.h"
#include "tty.h"
#include "fm.h"
#include "history.h"
#include "screen.h"
#include <dirent.h>
#include <wtf.h>

static int
terminated(unsigned char c)
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

void le_initialize(struct LineEditor* e, struct UI ui, struct Hist* hist, enum InputLineFlags flag,
    const char* def_str)
{
    e->ui = ui;
    e->offset = 0;

    e->is_passwd = false;
    e->move_word = true;
    e->CurrentHist = hist;
    if (hist != NULL) {
        e->use_hist = true;
        e->strCurrentBuf = NULL;
    } else {
        e->use_hist = false;
    }

    if (flag & IN_URL) {
        e->cm_mode = CPL_ALWAYS | CPL_URL;
    } else if (flag & IN_FILENAME) {
        e->cm_mode = CPL_ALWAYS;
    } else if (flag & IN_PASSWORD) {
        e->cm_mode = CPL_NEVER;
        e->is_passwd = true;
        e->move_word = FALSE;
    } else if (flag & IN_COMMAND)
        e->cm_mode = CPL_ON;
    else
        e->cm_mode = CPL_OFF;

    if (def_str) {
        e->strBuf = Strnew_charp(def_str);
        e->CLen = e->CPos = setStrType(e, e->strBuf, e->strProp);
    } else {
        e->strBuf = Strnew();
        e->CLen = e->CPos = 0;
    }

    e->i_cont = TRUE;
    e->i_broken = FALSE;
    e->i_quote = FALSE;
    e->cm_next = FALSE;
    e->cm_disp_next = -1;
    e->need_redraw = FALSE;
}

void next_compl(struct LineEditor* e, int next)
{
    if (e->cm_mode == CPL_NEVER || e->cm_mode & CPL_OFF)
        return;

    enum CompletionStatus status;
    int b, a;
    Str s;
    e->cm_clear = false;
    if (!e->cm_next) {
        if (e->cm_mode & CPL_ALWAYS) {
            b = 0;
        } else {
            for (b = e->CPos - 1; b >= 0; b--) {
                if ((e->strBuf->ptr[b] == ' ' || e->strBuf->ptr[b] == CTRL_I) && !((b > 0) && e->strBuf->ptr[b - 1] == '\\'))
                    break;
            }
            b++;
        }
        a = e->CPos;
        e->CBeforeBuf = Strsubstr(e->strBuf, 0, b);
        Str buf = Strsubstr(e->strBuf, b, a - b);
        e->CAfterBuf = Strsubstr(e->strBuf, a, e->strBuf->length - a);
        s = doComplete(e, buf, &status, next);
    } else {
        s = doComplete(e, e->strBuf, &status, next);
    }
    if (next == 0)
        return;

    if (status != CPL_OK && status != CPL_MENU)
        termBell(ttyWriter());
    if (status == CPL_FAIL)
        return;

    e->strBuf = Strnew_m_charp(e->CBeforeBuf->ptr, s->ptr, e->CAfterBuf->ptr, NULL);
    e->CLen = setStrType(e, e->strBuf, e->strProp);
    e->CPos = e->CBeforeBuf->length + s->length;
    if (e->CPos > e->CLen)
        e->CPos = e->CLen;
}

void _compl(struct LineEditor* e)
{
    next_compl(e, 1);
}

void _mvB(struct LineEditor* e)
{
    e->CPos = 0;
}

void _mvL(struct LineEditor* e)
{
    if (e->CPos > 0)
        e->CPos--;
    while (e->CPos > 0 && e->strProp[e->CPos] & PC_WCHAR2)
        e->CPos--;
}

void _inbrk(struct LineEditor* e)
{
    e->i_cont = FALSE;
    e->i_broken = TRUE;
}

void delC(struct LineEditor* e)
{
    int i = e->CPos;
    int delta = 1;

    if (e->CLen == e->CPos)
        return;
    while (i + delta < e->CLen && e->strProp[i + delta] & PC_WCHAR2)
        delta++;
    for (i = e->CPos; i < e->CLen; i++) {
        e->strProp[i] = e->strProp[i + delta];
    }
    Strdelete(e->strBuf, e->CPos, delta);
    e->CLen -= delta;
}

void _mvE(struct LineEditor* e)
{
    e->CPos = e->CLen;
}

void _mvR(struct LineEditor* e)
{
    if (e->CPos < e->CLen)
        e->CPos++;
    while (e->CPos < e->CLen && e->strProp[e->CPos] & PC_WCHAR2)
        e->CPos++;
}

void _bs(struct LineEditor* e)
{
    if (e->CPos > 0) {
        _mvL(e);
        delC(e);
    }
}

void _enter(struct LineEditor* e)
{
    e->i_cont = false;
}

void killn(struct LineEditor* e)
{
    e->CLen = e->CPos;
    Strtruncate(e->strBuf, e->CLen);
}

void _next(struct LineEditor* e)
{
    struct Hist* hist = e->CurrentHist;
    char* p;

    if (!e->use_hist)
        return;
    if (e->strCurrentBuf == NULL)
        return;
    p = nextHist(hist);
    if (p) {
        if (DecodeURL && (e->cm_mode & CPL_URL))
            p = url_decode2(p, NULL);
        e->strBuf = Strnew_charp(p);
    } else {
        e->strBuf = e->strCurrentBuf;
        e->strCurrentBuf = NULL;
    }
    e->CLen = e->CPos = setStrType(e, e->strBuf, e->strProp);
    e->offset = 0;
}

void _editor(struct LineEditor* e)
{
    FormItemList fi;
    char* p;

    if (e->is_passwd)
        return;

    fi.readonly = FALSE;
    fi.value = Strdup(e->strBuf);
    Strcat_char(fi.value, '\n');

    input_textarea(&fi);

    e->strBuf = Strnew();
    for (p = fi.value->ptr; *p; p++) {
        if (*p == '\r' || *p == '\n')
            continue;
        Strcat_char(e->strBuf, *p);
    }
    e->CLen = e->CPos = setStrType(e, e->strBuf, e->strProp);
}

void _prev(struct LineEditor* e)
{
    if (!e->use_hist)
        return;

    struct Hist* hist = e->CurrentHist;
    char* p;
    if (e->strCurrentBuf) {
        p = prevHist(hist);
        if (p == NULL)
            return;
    } else {
        p = lastHist(hist);
        if (p == NULL)
            return;
        e->strCurrentBuf = e->strBuf;
    }
    if (DecodeURL && (e->cm_mode & CPL_URL))
        p = url_decode2(p, NULL);
    e->strBuf = Strnew_charp(p);
    e->CLen = e->CPos = setStrType(e, e->strBuf, e->strProp);
    e->offset = 0;
}

void _quo(struct LineEditor* e)
{
    e->i_quote = TRUE;
}

void _bsw(struct LineEditor* e)
{
    int t = 0;
    while (e->CPos > 0 && !t) {
        _mvL(e);
        t = (e->move_word && terminated(e->strBuf->ptr[e->CPos - 1]));
        delC(e);
    }
}

void _mvLw(struct LineEditor* e)
{
    int first = 1;
    while (e->CPos > 0 && (first || !terminated(e->strBuf->ptr[e->CPos - 1]))) {
        e->CPos--;
        first = 0;
        if (e->CPos > 0 && e->strProp[e->CPos] & PC_WCHAR2)
            e->CPos--;
        if (!e->move_word)
            break;
    }
}

void killb(struct LineEditor* e)
{
    while (e->CPos > 0)
        _bs(e);
}

void _tcompl(struct LineEditor* e)
{
    if (e->cm_mode & CPL_OFF)
        e->cm_mode = CPL_ON;
    else if (e->cm_mode & CPL_ON)
        e->cm_mode = CPL_OFF;
}

void _mvRw(struct LineEditor* e)
{
    int first = 1;
    while (e->CPos < e->CLen && (first || !terminated(e->strBuf->ptr[e->CPos - 1]))) {
        e->CPos++;
        first = 0;
        if (e->CPos < e->CLen && e->strProp[e->CPos] & PC_WCHAR2)
            e->CPos++;
        if (!e->move_word)
            break;
    }
}

void _dcompl(struct LineEditor* e)
{
    next_dcompl(e, 1);
}

void insC(struct LineEditor* e)
{
    int i;

    Strinsert_char(e->strBuf, e->CPos, ' ');
    e->CLen = e->strBuf->length;
    for (i = e->CLen; i > e->CPos; i--) {
        e->strProp[i] = e->strProp[i - 1];
    }
}

void insertself(struct LineEditor* e, char c)
{
    if (e->CLen >= STR_LEN)
        return;
    insC(e);
    e->strBuf->ptr[e->CPos] = c;
    e->strProp[e->CPos] = (e->is_passwd) ? PC_ASCII : PC_CTRL;
    e->CPos++;
}

void _rdcompl(struct LineEditor* e)
{
    next_dcompl(e, -1);
}

void _rcompl(struct LineEditor* e)
{
    next_compl(e, -1);
}

void next_dcompl(struct LineEditor* e, int next)
{
    struct VirtualTerm* vt = getScreen();
    static int col, row;
    static unsigned int len;
    static Str d;

    int i, j, n, y;
    Str f;
    char* p;
    struct stat st;
    int comment, nline;

    if (e->cm_mode == CPL_NEVER || e->cm_mode & CPL_OFF)
        return;
    e->cm_disp_clear = FALSE;
    if (e->ui.vt->ROWS - 1 >= 3) {
        comment = TRUE;
        nline = e->ui.vt->ROWS - 1 - 2;
    } else if (e->ui.vt->ROWS - 1) {
        comment = FALSE;
        nline = e->ui.vt->ROWS - 1;
    } else {
        return;
    }

    if (e->cm_disp_next >= 0) {
        if (next == 1) {
            e->cm_disp_next += col * nline;
            if (e->cm_disp_next >= e->NCFileBuf)
                e->cm_disp_next = 0;
        } else if (next == -1) {
            e->cm_disp_next -= col * nline;
            if (e->cm_disp_next < 0)
                e->cm_disp_next = 0;
        }
        row = (e->NCFileBuf - e->cm_disp_next + col - 1) / col;
        goto disp_next;
    }

    e->cm_next = FALSE;
    next_compl(e, 0);
    if (e->NCFileBuf == 0)
        return;
    e->cm_disp_next = 0;

    d = Str_conv_to_system(Strdup(e->CDirBuf));
    if (d->length > 0 && Strlastchar(d) != '/')
        Strcat_char(d, '/');
    if (e->cm_mode & CPL_URL && d->ptr[0] == 'f') {
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
    for (i = 0; i < e->NCFileBuf; i++) {
        n = strlen(e->CFileBuf[i]) + 3;
        if (len < n)
            len = n;
    }
    if (len > 0 && e->ui.vt->COLS > len)
        col = e->ui.vt->COLS / len;
    else
        col = 1;
    row = (e->NCFileBuf + col - 1) / col;

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
        move(vt, y - 1, 0);
        clrtoeolx(vt);
    }
    if (comment) {
        move(vt, y, 0);
        clrtoeolx(vt);
        bold(vt);
        /* FIXME: gettextize? */
        addstr(vt, "----- Completion list -----");
        boldend(vt);
        y++;
    }
    for (i = 0; i < row; i++) {
        for (j = 0; j < col; j++) {
            n = e->cm_disp_next + j * row + i;
            if (n >= e->NCFileBuf)
                break;
            move(vt, y, j * len);
            clrtoeolx(vt);
            f = Strdup(d);
            Strcat_charp(f, e->CFileBuf[n]);
            addstr(vt, conv_from_system(e->CFileBuf[n]));
            if (stat(expandPath(f->ptr), &st) != -1 && S_ISDIR(st.st_mode))
                addstr(vt, "/");
        }
        y++;
    }
    if (comment && y == e->ui.vt->ROWS - 1 - 1) {
        move(vt, y, 0);
        clrtoeolx(vt);
        bold(vt);
        if (emacs_like_lineedit)
            /* FIXME: gettextize? */
            addstr(vt, "----- Press TAB to continue -----");
        else
            /* FIXME: gettextize? */
            addstr(vt, "----- Press CTRL-D to continue -----");
        boldend(vt);
    }
}

Str doComplete(struct LineEditor* e, Str ifn, enum CompletionStatus* status, int next)
{
    int fl, i;
    char *fn, *p;
    DIR* d;
    Directory* dir;
    struct stat st;

    if (!e->cm_next) {
        e->NCFileBuf = 0;
        ifn = Str_conv_to_system(ifn);
        if (e->cm_mode & CPL_ON)
            ifn = unescape_spaces(ifn);
        e->CompleteBuf = Strdup(ifn);
        while (Strlastchar(e->CompleteBuf) != '/' && e->CompleteBuf->length > 0)
            Strshrink(e->CompleteBuf, 1);
        e->CDirBuf = Strdup(e->CompleteBuf);
        if (e->cm_mode & CPL_URL) {
            if (strncmp(e->CompleteBuf->ptr, "file://localhost/", 17) == 0)
                Strdelete(e->CompleteBuf, 0, 16);
            else if (strncmp(e->CompleteBuf->ptr, "file:///", 8) == 0)
                Strdelete(e->CompleteBuf, 0, 7);
            else if (strncmp(e->CompleteBuf->ptr, "file:/", 6) == 0 && e->CompleteBuf->ptr[6] != '/')
                Strdelete(e->CompleteBuf, 0, 5);
            else {
                e->CompleteBuf = Strdup(ifn);
                *status = CPL_FAIL;
                return Str_conv_to_system(e->CompleteBuf);
            }
        }
        if (e->CompleteBuf->length == 0) {
            Strcat_char(e->CompleteBuf, '.');
        }
        if (Strlastchar(e->CompleteBuf) == '/' && e->CompleteBuf->length > 1) {
            Strshrink(e->CompleteBuf, 1);
        }
        if ((d = opendir(expandPath(e->CompleteBuf->ptr))) == NULL) {
            e->CompleteBuf = Strdup(ifn);
            *status = CPL_FAIL;
            if (e->cm_mode & CPL_ON)
                e->CompleteBuf = escape_spaces(e->CompleteBuf);
            return e->CompleteBuf;
        }
        fn = lastFileName(ifn->ptr);
        fl = strlen(fn);
        e->CFileName = Strnew();
        for (;;) {
            dir = readdir(d);
            if (dir == NULL)
                break;
            if (fl == 0
                && (!strcmp(dir->d_name, ".") || !strcmp(dir->d_name, "..")))
                continue;
            if (!strncmp(dir->d_name, fn, fl)) { /* match */
                e->NCFileBuf++;
                e->CFileBuf = New_Reuse(char*, e->CFileBuf, e->NCFileBuf);
                e->CFileBuf[e->NCFileBuf - 1] = NewAtom_N(char, strlen(dir->d_name) + 1);
                strcpy(e->CFileBuf[e->NCFileBuf - 1], dir->d_name);
                if (e->NCFileBuf == 1) {
                    e->CFileName = Strnew_charp(dir->d_name);
                } else {
                    for (i = 0; e->CFileName->ptr[i] == dir->d_name[i]; i++)
                        ;
                    Strtruncate(e->CFileName, i);
                }
            }
        }
        closedir(d);
        if (e->NCFileBuf == 0) {
            e->CompleteBuf = Strdup(ifn);
            *status = CPL_FAIL;
            if (e->cm_mode & CPL_ON)
                e->CompleteBuf = escape_spaces(e->CompleteBuf);
            return e->CompleteBuf;
        }
        qsort(e->CFileBuf, e->NCFileBuf, sizeof(e->CFileBuf[0]), strCmp);
        e->NCFileOffset = 0;
        if (e->NCFileBuf >= 2) {
            e->cm_next = TRUE;
            *status = CPL_AMBIG;
        } else {
            *status = CPL_OK;
        }
    } else {
        e->CFileName = Strnew_charp(e->CFileBuf[e->NCFileOffset]);
        e->NCFileOffset = (e->NCFileOffset + next + e->NCFileBuf) % e->NCFileBuf;
        *status = CPL_MENU;
    }
    e->CompleteBuf = Strdup(e->CDirBuf);
    if (e->CompleteBuf->length && Strlastchar(e->CompleteBuf) != '/')
        Strcat_char(e->CompleteBuf, '/');
    Strcat(e->CompleteBuf, e->CFileName);
    if (*status != CPL_AMBIG) {
        p = e->CompleteBuf->ptr;
        if (e->cm_mode & CPL_URL) {
            if (strncmp(p, "file://localhost/", 17) == 0)
                p = &p[16];
            else if (strncmp(p, "file:///", 8) == 0)
                p = &p[7];
            else if (strncmp(p, "file:/", 6) == 0 && p[6] != '/')
                p = &p[5];
        }
        if (stat(expandPath(p), &st) != -1 && S_ISDIR(st.st_mode))
            Strcat_char(e->CompleteBuf, '/');
    }
    if (e->cm_mode & CPL_ON)
        e->CompleteBuf = escape_spaces(e->CompleteBuf);
    return Str_conv_from_system(e->CompleteBuf);
}

int setStrType(struct LineEditor* e, Str str, Lineprop* prop)
{
    Lineprop ctype;
    char *p = str->ptr, *ep = p + str->length;
    int i, len = 1;

    for (i = 0; p < ep;) {
        len = get_mclen(p);
        if (i + len > STR_LEN)
            break;
        ctype = get_mctype(p);
        if (e->is_passwd) {
            if (ctype & PC_CTRL)
                ctype = PC_ASCII;
            if (ctype & PC_UNKNOWN)
                ctype = PC_WCHAR1;
        }
        prop[i++] = ctype;
        p += len;
        if (--len) {
            ctype = (ctype & ~PC_WCHAR1) | PC_WCHAR2;
            while (len--)
                prop[i++] = ctype;
        }
    }
    return i;
}

void ins_char(struct LineEditor* e, Str str)
{
    if (e->CLen + str->length >= STR_LEN)
        return;

    char* p = str->ptr;
    char* ep = p + str->length;
    Lineprop ctype;
    int len;
    while (p < ep) {
        len = get_mclen(p);
        ctype = get_mctype(p);
        if (e->is_passwd) {
            if (ctype & PC_CTRL)
                ctype = PC_ASCII;
            if (ctype & PC_UNKNOWN)
                ctype = PC_WCHAR1;
        }
        insC(e);
        e->strBuf->ptr[e->CPos] = *(p++);
        e->strProp[e->CPos] = ctype;
        e->CPos++;
        if (--len) {
            ctype = (ctype & ~PC_WCHAR1) | PC_WCHAR2;
            while (len--) {
                insC(e);
                e->strBuf->ptr[e->CPos] = *(p++);
                e->strProp[e->CPos] = ctype;
                e->CPos++;
            }
        }
    }
}

void addPasswd(struct LineEditor* e, char* p, Lineprop* pr, int len, int offset, int limit)
{
    int rcol = 0, ncol;

    ncol = calcPosition(p, pr, len, len, 0, CP_AUTO);
    if (ncol > offset + limit)
        ncol = offset + limit;
    if (offset) {
        addChar('{', 0);
        rcol = offset + 1;
    }
    for (; rcol < ncol; rcol++)
        addChar('*', 0);
}

void addStr(struct LineEditor* e, char* p, Lineprop* pr, int len, int offset, int limit)
{
    int i = 0, rcol = 0, ncol, delta = 1;

    if (offset) {
        for (i = 0; i < len; i++) {
            if (calcPosition(p, pr, len, i, 0, CP_AUTO) > offset)
                break;
        }
        if (i >= len)
            return;
        while (pr[i] & PC_WCHAR2)
            i++;
        addChar('{', 0);
        rcol = offset + 1;
        ncol = calcPosition(p, pr, len, i, 0, CP_AUTO);
        for (; rcol < ncol; rcol++)
            addChar(' ', 0);
    }
    for (; i < len; i += delta) {
        delta = wtf_len((wc_uchar*)&p[i]);
        ncol = calcPosition(p, pr, len, i + delta, 0, CP_AUTO);
        if (ncol - offset > limit)
            break;
        if (p[i] == '\t') {
            for (; rcol < ncol; rcol++)
                addChar(' ', 0);
            continue;
        } else {
            addMChar(&p[i], pr[i], delta);
        }
        rcol = ncol;
    }
}
