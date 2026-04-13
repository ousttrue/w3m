#include "line_input.h"
#include "qsort_util.h"
#include "alloc.h"
#include "term_tty.h"
#include "global.h"
#include "ctrlcode.h"
#include "form.h"
#include "terms.h"
#include "indep.h"
#include "tab.h"
#include "buffer.h"
#include "wc_util.h"
#include "url.h"
#include "proto.h"
#include "etc.h"
#include "display.h"
#include "local.h"
#include "myctype.h"
#include "history.h"

#include <libwc/charset.h>
#include <libwc/wtf.h>

#define STR_LEN 1024
#define CLEN (COLS - 2)

enum CompletionStatus {
    CPL_OK = 0,
    CPL_AMBIG = 1,
    CPL_FAIL = 2,
    CPL_MENU = 3,
};

enum CompletionFlags {
    CPL_NEVER = 0x0,
    CPL_OFF = 0x1,
    CPL_ON = 0x2,
    CPL_ALWAYS = 0x4,
    CPL_URL = 0x8,
};

static Str strBuf;
static Lineprop strProp[STR_LEN];

static Str CompleteBuf;
static Str CFileName;
static Str CBeforeBuf;
static Str CAfterBuf;
static Str CDirBuf;
static char** CFileBuf = NULL;
static int NCFileBuf;
static int NCFileOffset;

// static void insertself(char c),

typedef int (*InputFunc)(struct CmdArgs *args);
static int iself(struct CmdArgs *args);
static int _mvR(struct CmdArgs *args);
static int _mvL(struct CmdArgs *args);
static int _mvRw(struct CmdArgs *args);
static int _mvLw(struct CmdArgs *args);
static int delC(struct CmdArgs *args);
static int insC(struct CmdArgs *args);
static int _mvB(struct CmdArgs *args);
static int _mvE(struct CmdArgs *args);
static int _enter(struct CmdArgs *args);
static int _quo(struct CmdArgs *args);
static int _bs(struct CmdArgs *args);
static int _bsw(struct CmdArgs *args);
static int killn(struct CmdArgs *args);
static int killb(struct CmdArgs *args);
static int _inbrk(struct CmdArgs *args);
static int _esc(struct CmdArgs *args);
static int _editor(struct CmdArgs *args);
static int _prev(struct CmdArgs *args);
static int _next(struct CmdArgs *args);
static int _compl(struct CmdArgs *args);
static int _tcompl(struct CmdArgs *args);
static int _dcompl(struct CmdArgs *args);
static int _rdcompl(struct CmdArgs *args);
static int _rcompl(struct CmdArgs *args);
;
static int terminated(unsigned char c);

static void next_compl(int next);
static void next_dcompl(int next);
static Str doComplete(Str ifn, enum CompletionStatus* status, int next);

// clang-format off
InputFunc InputKeymap[32] = {
    /*  C-@     C-a     C-b     C-c     C-d     C-e     C-f     C-g     */
    _compl, _mvB, _mvL, _inbrk, delC, _mvE, _mvR, _inbrk,
    /*  C-h     C-i     C-j     C-k     C-l     C-m     C-n     C-o     */
    _bs, iself, _enter, killn, iself, _enter, _next, _editor,
    /*  C-p     C-q     C-r     C-s     C-t     C-u     C-v     C-w     */
    _prev, _quo, _bsw, iself, _mvLw, killb, _quo, _bsw,
    /*  C-x     C-y     C-z     C-[     C-\     C-]     C-^     C-_     */
    _tcompl, _mvRw, iself, _esc, iself, iself, iself, iself,
};
// clang-format on

static int setStrType(Str str, Lineprop* prop);
static void addPasswd(char* p, Lineprop* pr, int len, int pos, int limit);
static void addStr(char* p, Lineprop* pr, int len, int pos, int limit);
static void ins_char(struct CmdArgs *args, Str str);

static int CPos, CLen, offset;
static int i_cont, i_broken, i_quote;
static int cm_next, cm_clear, cm_disp_next, cm_disp_clear;
static enum CompletionFlags cm_mode = 0;
static int need_redraw, is_passwd;
static int move_word;

static struct Hist* CurrentHist;
static Str strCurrentBuf;
static int use_hist;

char* inputLineHistSearch(struct CmdArgs *args, const char* prompt, const char* def_str,
    enum InputLineFlags flag, struct Hist* hist, IncrFunc incrfunc)
{
    int opos, x, y, lpos, rpos, epos;
    unsigned char c;
    char* p;
    Str tmp;

    is_passwd = false;
    move_word = true;

    CurrentHist = hist;
    if (hist != NULL) {
        use_hist = true;
        strCurrentBuf = NULL;
    } else {
        use_hist = false;
    }
    if (flag & IN_URL) {
        cm_mode = CPL_ALWAYS | CPL_URL;
    } else if (flag & IN_FILENAME) {
        cm_mode = CPL_ALWAYS;
    } else if (flag & IN_PASSWORD) {
        cm_mode = CPL_NEVER;
        is_passwd = true;
        move_word = false;
    } else if (flag & IN_COMMAND)
        cm_mode = CPL_ON;
    else
        cm_mode = CPL_OFF;
    opos = get_strwidth(WcOption, prompt);
    epos = CLEN - opos;
    if (epos < 0)
        epos = 0;
    lpos = epos / 3;
    rpos = epos * 2 / 3;
    offset = 0;

    if (def_str) {
        strBuf = Strnew_charp(def_str);
        CLen = CPos = setStrType(strBuf, strProp);
    } else {
        strBuf = Strnew();
        CLen = CPos = 0;
    }

    i_cont = true;
    i_broken = false;
    i_quote = false;
    cm_next = false;
    cm_disp_next = -1;
    need_redraw = false;

    wc_char_conv_init(wc_guess_8bit_charset(DisplayCharset), InnerCharset);
    do {
        x = calcPosition(strBuf->ptr, strProp, CLen, CPos, 0, CP_FORCE);
        if (x - rpos > offset) {
            y = calcPosition(strBuf->ptr, strProp, CLen, CLen, 0, CP_AUTO);
            if (y - epos > x - rpos)
                offset = x - rpos;
            else if (y - epos > 0)
                offset = y - epos;
        } else if (x - lpos < offset) {
            if (x - lpos > 0)
                offset = x - lpos;
            else
                offset = 0;
        }
        move((LINES - 1), 0);
        addstr(prompt);
        if (is_passwd)
            addPasswd(strBuf->ptr, strProp, CLen, offset, COLS - opos);
        else
            addStr(strBuf->ptr, strProp, CLen, offset, COLS - opos);
        clrtoeolx();
        move((LINES - 1), opos + x - offset);
        refresh();

    next_char:
        c = getch(args);
        cm_clear = true;
        cm_disp_clear = true;
        if (!i_quote && (((cm_mode & CPL_ALWAYS) && (c == CTRL_I || (space_autocomplete && c == ' '))) || ((cm_mode & CPL_ON) && (c == CTRL_I)))) {
            if (emacs_like_lineedit && cm_next) {
                _dcompl(args);
                need_redraw = true;
            } else {
                _compl(args);
                cm_disp_next = -1;
            }
        } else if (!i_quote && CLen == CPos && (cm_mode & CPL_ALWAYS || cm_mode & CPL_ON) && c == CTRL_D) {
            if (!emacs_like_lineedit) {
                _dcompl(args);
                need_redraw = true;
            }
        } else if (!i_quote && c == DEL_CODE) {
            _bs(args);
            cm_next = false;
            cm_disp_next = -1;
        } else if (!i_quote && c < 0x20) { /* Control code */
            if (incrfunc == NULL
                || (c = incrfunc((int)c, strBuf, strProp)) < 0x20)
                ((int (*)(int))*InputKeymap[(int)c])(c);
            if (incrfunc && c != (unsigned char)-1 && c != CTRL_J)
                incrfunc(-1, strBuf, strProp);
            if (cm_clear)
                cm_next = false;
            if (cm_disp_clear)
                cm_disp_next = -1;
        } else {
            tmp = Strnew_wc_output(wc_char_conv(WcOption, c));
            if (tmp == NULL) {
                i_quote = true;
                goto next_char;
            }
            i_quote = false;
            cm_next = false;
            cm_disp_next = -1;
            if (CLen + tmp->length > STR_LEN || !tmp->length)
                goto next_char;
            ins_char(args, tmp);
            if (incrfunc)
                incrfunc(-1, strBuf, strProp);
        }
        if (CLen && (flag & IN_CHAR))
            break;
    } while (i_cont);

    if (CurrentTab) {
        if (need_redraw)
            displayBuffer(Currentbuf, B_FORCE_REDRAW);
    }

    if (i_broken)
        return NULL;

    move((LINES - 1), 0);
    refresh();
    p = strBuf->ptr;
    if (flag & (IN_FILENAME | IN_COMMAND)) {
        SKIP_BLANKS(p);
    }
    if (use_hist && !(flag & IN_URL) && *p != '\0') {
        const char* q = lastHist(hist);
        if (!q || strcmp(q, p))
            pushHist(hist, p);
    }
    if (flag & IN_FILENAME)
        return expandPath(p);
    else
        return allocStr(p, -1);
}

static void
addPasswd(char* p, Lineprop* pr, int len, int offset, int limit)
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

static void
addStr(char* p, Lineprop* pr, int len, int offset, int limit)
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

static void
ins_char(struct CmdArgs *args, Str str)
{
    char *p = str->ptr, *ep = p + str->length;
    Lineprop ctype;
    int len;

    if (CLen + str->length >= STR_LEN)
        return;
    while (p < ep) {
        len = get_mclen(p);
        ctype = get_mctype(p);
        if (is_passwd) {
            if (ctype & PC_CTRL)
                ctype = PC_ASCII;
            if (ctype & PC_UNKNOWN)
                ctype = PC_WCHAR1;
        }
        insC(args);
        strBuf->ptr[CPos] = *(p++);
        strProp[CPos] = ctype;
        CPos++;
        if (--len) {
            ctype = (ctype & ~PC_WCHAR1) | PC_WCHAR2;
            while (len--) {
                insC(args);
                strBuf->ptr[CPos] = *(p++);
                strProp[CPos] = ctype;
                CPos++;
            }
        }
    }
}

static int _esc(struct CmdArgs *args)
{
    args->ch = getch(args);
    switch (args->ch) {
    case '[':
    case 'O':
        switch (args->ch = getch(args)) {
        case 'A':
            _prev(args);
            break;
        case 'B':
            _next(args);
            break;
        case 'C':
            _mvR(args);
            break;
        case 'D':
            _mvL(args);
            break;
        }
        break;
    case CTRL_I:
    case ' ':
        if (emacs_like_lineedit) {
            _rdcompl(args);
            cm_clear = false;
            need_redraw = true;
        } else
            _rcompl(args);
        break;
    case CTRL_D:
        if (!emacs_like_lineedit)
            _rdcompl(args);
        need_redraw = true;
        break;
    case 'f':
        if (emacs_like_lineedit)
            _mvRw(args);
        break;
    case 'b':
        if (emacs_like_lineedit)
            _mvLw(args);
        break;
    case CTRL_H:
        if (emacs_like_lineedit)
            _bsw(args);
        break;
    default:
        if (wc_char_conv(WcOption, ESC_CODE).data == NULL && wc_char_conv(WcOption, args->ch).data == NULL)
            i_quote = true;
    }

    return 0;
}

static int insC(struct CmdArgs *args)
{
    Strinsert_char(strBuf, CPos, ' ');
    CLen = strBuf->length;
    for (int i = CLen; i > CPos; i--) {
        strProp[i] = strProp[i - 1];
    }
    return 0;
}

static int delC(struct CmdArgs *args)
{
    if (CLen == CPos)
        return 0;

    int i = CPos;
    int delta = 1;
    while (i + delta < CLen && strProp[i + delta] & PC_WCHAR2)
        delta++;
    for (i = CPos; i < CLen; i++) {
        strProp[i] = strProp[i + delta];
    }
    Strdelete(strBuf, CPos, delta);
    CLen -= delta;
    return 0;
}

static int _mvL(struct CmdArgs *args)
{
    if (CPos > 0)
        CPos--;
    while (CPos > 0 && strProp[CPos] & PC_WCHAR2)
        CPos--;
    return 0;
}

static int _mvLw(struct CmdArgs *args)
{
    int first = 1;
    while (CPos > 0 && (first || !terminated(strBuf->ptr[CPos - 1]))) {
        CPos--;
        first = 0;
        if (CPos > 0 && strProp[CPos] & PC_WCHAR2)
            CPos--;
        if (!move_word)
            break;
    }
    return 0;
}

static int _mvRw(struct CmdArgs *args)
{
    int first = 1;
    while (CPos < CLen && (first || !terminated(strBuf->ptr[CPos - 1]))) {
        CPos++;
        first = 0;
        if (CPos < CLen && strProp[CPos] & PC_WCHAR2)
            CPos++;
        if (!move_word)
            break;
    }
    return 0;
}

static int _mvR(struct CmdArgs *args)
{
    if (CPos < CLen)
        CPos++;
    while (CPos < CLen && strProp[CPos] & PC_WCHAR2)
        CPos++;
    return 0;
}

static int _bs(struct CmdArgs *args)
{
    if (CPos > 0) {
        _mvL(args);
        delC(args);
    }
    return 0;
}

static int _bsw(struct CmdArgs *args)
{
    int t = 0;
    while (CPos > 0 && !t) {
        _mvL(args);
        t = (move_word && terminated(strBuf->ptr[CPos - 1]));
        delC(args);
    }
    return 0;
}

static int _enter(struct CmdArgs *args)
{
    i_cont = false;
    return 0;
}

static int iself(struct CmdArgs *args)
{
    if (CLen >= STR_LEN)
        return 0;
    insC(args);
    strBuf->ptr[CPos] = args->ch;
    strProp[CPos] = (is_passwd) ? PC_ASCII : PC_CTRL;
    CPos++;
    return 0;
}

static int _quo(struct CmdArgs *args)
{
    i_quote = true;
    return 0;
}

static int _mvB(struct CmdArgs *args)
{
    CPos = 0;
    return 0;
}

static int _mvE(struct CmdArgs *args)
{
    CPos = CLen;
    return 0;
}

static int killn(struct CmdArgs *args)
{
    CLen = CPos;
    Strtruncate(strBuf, CLen);
    return 0;
}

static int killb(struct CmdArgs *args)
{
    while (CPos > 0)
        _bs(args);
    return 0;
}

static int _inbrk(struct CmdArgs *args)
{
    i_cont = false;
    i_broken = true;
    return 0;
}

static int _compl(struct CmdArgs *args)
{
    next_compl(1);
    return 0;
}

static int _rcompl(struct CmdArgs *args)
{
    next_compl(-1);
    return 0;
}

static int _tcompl(struct CmdArgs *args)
{
    if (cm_mode & CPL_OFF)
        cm_mode = CPL_ON;
    else if (cm_mode & CPL_ON)
        cm_mode = CPL_OFF;
    return 0;
}

static void
next_compl(int next)
{
    enum CompletionStatus status;
    int b, a;
    Str buf;
    Str s;

    if (cm_mode == CPL_NEVER || cm_mode & CPL_OFF)
        return;
    cm_clear = false;
    if (!cm_next) {
        if (cm_mode & CPL_ALWAYS) {
            b = 0;
        } else {
            for (b = CPos - 1; b >= 0; b--) {
                if ((strBuf->ptr[b] == ' ' || strBuf->ptr[b] == CTRL_I) && !((b > 0) && strBuf->ptr[b - 1] == '\\'))
                    break;
            }
            b++;
        }
        a = CPos;
        CBeforeBuf = Strsubstr(strBuf, 0, b);
        buf = Strsubstr(strBuf, b, a - b);
        CAfterBuf = Strsubstr(strBuf, a, strBuf->length - a);
        s = doComplete(buf, &status, next);
    } else {
        s = doComplete(strBuf, &status, next);
    }
    if (next == 0)
        return;

    if (status != CPL_OK && status != CPL_MENU)
        bell();
    if (status == CPL_FAIL)
        return;

    strBuf = Strnew_m_charp(CBeforeBuf->ptr, s->ptr, CAfterBuf->ptr, NULL);
    CLen = setStrType(strBuf, strProp);
    CPos = CBeforeBuf->length + s->length;
    if (CPos > CLen)
        CPos = CLen;
}

static int _dcompl(struct CmdArgs *args)
{
    next_dcompl(1);
    return 0;
}

static int _rdcompl(struct CmdArgs *args)
{
    next_dcompl(-1);
    return 0;
}

static void
next_dcompl(int next)
{
    static int col, row;
    static unsigned int len;
    static Str d;
    int i, j, n, y;
    Str f;
    char* p;
    struct stat st;
    int comment, nline;

    if (cm_mode == CPL_NEVER || cm_mode & CPL_OFF)
        return;
    cm_disp_clear = false;
    if (CurrentTab)
        displayBuffer(Currentbuf, B_FORCE_REDRAW);
    if ((LINES - 1) >= 3) {
        comment = true;
        nline = (LINES - 1) - 2;
    } else if ((LINES - 1)) {
        comment = false;
        nline = (LINES - 1);
    } else {
        return;
    }

    if (cm_disp_next >= 0) {
        if (next == 1) {
            cm_disp_next += col * nline;
            if (cm_disp_next >= NCFileBuf)
                cm_disp_next = 0;
        } else if (next == -1) {
            cm_disp_next -= col * nline;
            if (cm_disp_next < 0)
                cm_disp_next = 0;
        }
        row = (NCFileBuf - cm_disp_next + col - 1) / col;
        goto disp_next;
    }

    cm_next = false;
    next_compl(0);
    if (NCFileBuf == 0)
        return;
    cm_disp_next = 0;

    d = Str_conv_to_system(CDirBuf->ptr, CDirBuf->length);
    if (d->length > 0 && Strlastchar(d) != '/')
        Strcat_char(d, '/');
    if (cm_mode & CPL_URL && d->ptr[0] == 'f') {
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
    for (i = 0; i < NCFileBuf; i++) {
        n = strlen(CFileBuf[i]) + 3;
        if (len < n)
            len = n;
    }
    if (len > 0 && COLS > len)
        col = COLS / len;
    else
        col = 1;
    row = (NCFileBuf + col - 1) / col;

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
            n = cm_disp_next + j * row + i;
            if (n >= NCFileBuf)
                break;
            move(y, j * len);
            clrtoeolx();
            f = Strdup(d);
            Strcat_charp(f, CFileBuf[n]);
            addstr(conv_from_system(CFileBuf[n]));
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

Str unescape_spaces(Str s)
{
    Str tmp = NULL;
    char* p;

    if (s == NULL)
        return s;
    for (p = s->ptr; *p; p++) {
        if (*p == '\\' && (*(p + 1) == ' ' || *(p + 1) == CTRL_I)) {
            if (tmp == NULL)
                tmp = Strnew_charp_n(s->ptr, (int)(p - s->ptr));
        } else {
            if (tmp)
                Strcat_char(tmp, *p);
        }
    }
    if (tmp)
        return tmp;
    return s;
}

static Str
doComplete(Str ifn, enum CompletionStatus* status, int next)
{
    int fl, i;
    char *fn, *p;
    DIR* d;
    Directory* dir;
    struct stat st;

    if (!cm_next) {
        NCFileBuf = 0;
        ifn = Str_conv_to_system(ifn->ptr, ifn->length);
        if (cm_mode & CPL_ON)
            ifn = unescape_spaces(ifn);
        CompleteBuf = Strdup(ifn);
        while (Strlastchar(CompleteBuf) != '/' && CompleteBuf->length > 0)
            Strshrink(CompleteBuf, 1);
        CDirBuf = Strdup(CompleteBuf);
        if (cm_mode & CPL_URL) {
            if (strncmp(CompleteBuf->ptr, "file://localhost/", 17) == 0)
                Strdelete(CompleteBuf, 0, 16);
            else if (strncmp(CompleteBuf->ptr, "file:///", 8) == 0)
                Strdelete(CompleteBuf, 0, 7);
            else if (strncmp(CompleteBuf->ptr, "file:/", 6) == 0 && CompleteBuf->ptr[6] != '/')
                Strdelete(CompleteBuf, 0, 5);
            else {
                CompleteBuf = Strdup(ifn);
                *status = CPL_FAIL;
                return Str_conv_to_system(CompleteBuf->ptr, CompleteBuf->length);
            }
        }
        if (CompleteBuf->length == 0) {
            Strcat_char(CompleteBuf, '.');
        }
        if (Strlastchar(CompleteBuf) == '/' && CompleteBuf->length > 1) {
            Strshrink(CompleteBuf, 1);
        }
        if ((d = opendir(expandPath(CompleteBuf->ptr))) == NULL) {
            CompleteBuf = Strdup(ifn);
            *status = CPL_FAIL;
            if (cm_mode & CPL_ON)
                CompleteBuf = escape_spaces(CompleteBuf);
            return CompleteBuf;
        }
        fn = lastFileName(ifn->ptr);
        fl = strlen(fn);
        CFileName = Strnew();
        for (;;) {
            dir = readdir(d);
            if (dir == NULL)
                break;
            if (fl == 0
                && (!strcmp(dir->d_name, ".") || !strcmp(dir->d_name, "..")))
                continue;
            if (!strncmp(dir->d_name, fn, fl)) { /* match */
                NCFileBuf++;
                CFileBuf = New_Reuse(char*, CFileBuf, NCFileBuf);
                CFileBuf[NCFileBuf - 1] = NewAtom_N(char, strlen(dir->d_name) + 1);
                strcpy(CFileBuf[NCFileBuf - 1], dir->d_name);
                if (NCFileBuf == 1) {
                    CFileName = Strnew_charp(dir->d_name);
                } else {
                    for (i = 0; CFileName->ptr[i] == dir->d_name[i]; i++)
                        ;
                    Strtruncate(CFileName, i);
                }
            }
        }
        closedir(d);
        if (NCFileBuf == 0) {
            CompleteBuf = Strdup(ifn);
            *status = CPL_FAIL;
            if (cm_mode & CPL_ON)
                CompleteBuf = escape_spaces(CompleteBuf);
            return CompleteBuf;
        }
        qsort(CFileBuf, NCFileBuf, sizeof(CFileBuf[0]), strCmp);
        NCFileOffset = 0;
        if (NCFileBuf >= 2) {
            cm_next = true;
            *status = CPL_AMBIG;
        } else {
            *status = CPL_OK;
        }
    } else {
        CFileName = Strnew_charp(CFileBuf[NCFileOffset]);
        NCFileOffset = (NCFileOffset + next + NCFileBuf) % NCFileBuf;
        *status = CPL_MENU;
    }
    CompleteBuf = Strdup(CDirBuf);
    if (CompleteBuf->length && Strlastchar(CompleteBuf) != '/')
        Strcat_char(CompleteBuf, '/');
    Strcat(CompleteBuf, CFileName);
    if (*status != CPL_AMBIG) {
        p = CompleteBuf->ptr;
        if (cm_mode & CPL_URL) {
            if (strncmp(p, "file://localhost/", 17) == 0)
                p = &p[16];
            else if (strncmp(p, "file:///", 8) == 0)
                p = &p[7];
            else if (strncmp(p, "file:/", 6) == 0 && p[6] != '/')
                p = &p[5];
        }
        if (stat(expandPath(p), &st) != -1 && S_ISDIR(st.st_mode))
            Strcat_char(CompleteBuf, '/');
    }
    if (cm_mode & CPL_ON)
        CompleteBuf = escape_spaces(CompleteBuf);
    return Str_conv_from_system(CompleteBuf->ptr, CompleteBuf->length);
}

static int _prev(struct CmdArgs *args)
{
    struct Hist* hist = CurrentHist;
    if (!use_hist)
        return 0;
    const char* p;
    if (strCurrentBuf) {
        p = prevHist(hist);
        if (p == NULL)
            return 0;
    } else {
        p = lastHist(hist);
        if (p == NULL)
            return 0;
        strCurrentBuf = strBuf;
    }
    if (DecodeURL && (cm_mode & CPL_URL))
        p = url_decode2(p, NULL);
    strBuf = Strnew_charp(p);
    CLen = CPos = setStrType(strBuf, strProp);
    offset = 0;
    return 0;
}

static int _next(struct CmdArgs *args)
{
    struct Hist* hist = CurrentHist;

    if (!use_hist)
        return 0;

    if (strCurrentBuf == NULL)
        return 0;

    const char* p = nextHist(hist);
    if (p) {
        if (DecodeURL && (cm_mode & CPL_URL))
            p = url_decode2(p, NULL);
        strBuf = Strnew_charp(p);
    } else {
        strBuf = strCurrentBuf;
        strCurrentBuf = NULL;
    }
    CLen = CPos = setStrType(strBuf, strProp);
    offset = 0;
    return 0;
}

static int
setStrType(Str str, Lineprop* prop)
{
    Lineprop ctype;
    char *p = str->ptr, *ep = p + str->length;
    int i, len = 1;

    for (i = 0; p < ep;) {
        len = get_mclen(p);
        if (i + len > STR_LEN)
            break;
        ctype = get_mctype(p);
        if (is_passwd) {
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

static int _editor(struct CmdArgs *args)
{
    if (is_passwd)
        return 0;

    struct FormItem fi;
    fi.readonly = false;
    fi.value = Strdup(strBuf);
    Strcat_char(fi.value, '\n');

    input_textarea(args, &fi);

    strBuf = Strnew();
    for (char* p = fi.value->ptr; *p; p++) {
        if (*p == '\r' || *p == '\n')
            continue;
        Strcat_char(strBuf, *p);
    }
    CLen = CPos = setStrType(strBuf, strProp);
    if (CurrentTab)
        displayBuffer(Currentbuf, B_FORCE_REDRAW);
    return 0;
}

char* inputAnswer(struct CmdArgs *args, const char* prompt)
{
    if (QuietMessage)
        return "n";

    if (fmInitialized) {
        term_raw();
        return inputChar(args, prompt);
    } else {
        printf("%s", prompt);
        fflush(stdout);
        return Strfgets(stdin)->ptr;
    }
}
