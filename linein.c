#include "linein.h"
#include "str_gc.h"
#include "str_const.h"
#include "alloc.h"
#include "buffer.h"
#include "charset.h"
#include "config.h"
#include "ctrlcode.h"
#include "fm.h"
#include "proto.h"
#include "form.h"
#include "indep.h"
#include "rc.h"
#include "search.h"
#include "tab.h"
#include "terms.h"

#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

// Completion status.
#define CPL_OK 0
#define CPL_AMBIG 1
#define CPL_FAIL 2
#define CPL_MENU 3

#define CPL_NEVER 0x0
#define CPL_OFF 0x1
#define CPL_ON 0x2
#define CPL_ALWAYS 0x4
#define CPL_URL 0x8

#define STR_LEN 1024
#define CLEN (COLS - 2)

static pStr strBuf;
static Lineprop strProp[STR_LEN];
static pStr ynkBuf;
static int ynkCon;

static pStr CompleteBuf;
static pStr CFileName;
static pStr CBeforeBuf;
static pStr CAfterBuf;
static pStr CDirBuf;
static char** CFileBuf = NULL;
static int NCFileBuf;
static int NCFileOffset;

static void _bs(void);
static void _bsw(void);
static void _compl(void);
static void _cy(void);
static void _dcompl(void);
static void _editor(void);
static void _enter(void);
static void _esc(void);
static void _inbrk(void);
static void _isrch(void);
static void _iword(void);
static void _mvB(void);
static void _mvE(void);
static void _mvL(void);
static void _mvLw(void);
static void _mvR(void);
static void _mvRw(void);
static void _next(void);
static void _paste(void);
static void _prev(void);
static void _quo(void);
static void _rcompl(void);
static void _rdcompl(void);
static void _tcompl(void);
static void delC(void);
static void insC(void);
static void killb(void);
static void killn(void);
static void _noop(void);

static int terminated(unsigned char c);

static void next_compl(int next);
static void next_dcompl(int next);
static pStr doComplete(pStr ifn, int* status, int next);

// clang-format off
void (*InputKeymap[32]) (void) = {
/*  C-@     C-a     C-b     C-c     C-d     C-e     C-f     C-g     */
    _compl, _mvB,   _mvL,   _inbrk, delC,   _mvE,   _mvR,   _inbrk,
/*  C-h     C-i     C-j     C-k     C-l     C-m     C-n     C-o     */
    _bs,    _noop,  _enter, killn,  _noop,  _enter, _next,  _editor,
/*  C-p     C-q     C-r     C-s     C-t     C-u     C-v     C-w     */
    _prev,  _quo,   _bsw,   _noop,  _mvLw,  killb,  _quo,   _bsw,
/*  C-x     C-y     C-z     C-[     C-\     C-]     C-^     C-_     */
    _tcompl,_cy,    _noop,  _esc,   _isrch, _iword, _noop,  _noop,
};
// clang-format on

static pStr escape_spaces(pStr s);
static int setStrType(pStr str, Lineprop* prop);
static void addPasswd(char* p, Lineprop* pr, int len, int pos, int limit);
static void addStr(char* p, Lineprop* pr, int len, int pos, int limit);

static int CPos, CLen, offset;
static int i_cont, i_broken, i_quote;
static int cm_mode, cm_next, cm_clear, cm_disp_next, cm_disp_clear;
static int need_redraw, is_passwd;

static Hist* CurrentHist;
static pStr strCurrentBuf;
static void ins_char(pStr str);

struct Str inputLineHistSearch(const char* prompt, const char* def_str,
    enum InputLineFlag flag, Hist* hist,
    IncrFunc incrfunc)
{
    int opos, x, y, lpos, rpos, epos;
    unsigned char c;
    char* p;
    pStr tmp;

    is_passwd = FALSE;

    if ((CurrentHist = hist))
        strCurrentBuf = NULL;

    if (flag & IN_URL) {
        cm_mode = CPL_ALWAYS | CPL_URL;
    } else if (flag & IN_FILENAME) {
        cm_mode = CPL_ALWAYS;
    } else if (flag & IN_PASSWORD) {
        cm_mode = CPL_NEVER;
        is_passwd = TRUE;
    } else if (flag & IN_COMMAND)
        cm_mode = CPL_ON;
    else
        cm_mode = CPL_OFF;
    opos = get_strwidth(prompt);
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

    i_cont = TRUE;
    i_broken = FALSE;
    i_quote = FALSE;
    cm_next = FALSE;
    cm_disp_next = -1;
    need_redraw = FALSE;

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
        move(LASTLINE, 0);
        addstr(prompt);
        if (is_passwd)
            addPasswd(strBuf->ptr, strProp, CLen, offset, COLS - opos);
        else
            addStr(strBuf->ptr, strProp, CLen, offset, COLS - opos);
        clrtoeolx();
        move(LASTLINE, opos + x - offset);
        refresh();

    next_char:
        c = getch();
        cm_clear = TRUE;
        cm_disp_clear = TRUE;
        if (!i_quote && (((cm_mode & CPL_ALWAYS) && (c == CTRL_I || (space_autocomplete && c == ' '))) || ((cm_mode & CPL_ON) && (c == CTRL_I)))) {
            if (emacs_like_lineedit && cm_next) {
                _dcompl();
                need_redraw = TRUE;
            } else {
                _compl();
                cm_disp_next = -1;
            }
        } else if (!i_quote && CLen == CPos && (cm_mode & CPL_ALWAYS || cm_mode & CPL_ON) && c == CTRL_D) {
            if (!emacs_like_lineedit) {
                _dcompl();
                need_redraw = TRUE;
            }
        } else if (!i_quote && c == DEL_CODE) {
            _bs();
            cm_next = FALSE;
            cm_disp_next = -1;
        } else if (!i_quote && c < 0x20) { /* Control code */
            if (incrfunc == NULL
                || (c = incrfunc((int)c, strBuf, strProp)) < 0x20)
                (*InputKeymap[(int)c])();
            if (incrfunc && c != (unsigned char)-1 && c != CTRL_J)
                incrfunc(-1, strBuf, strProp);
            if (cm_clear)
                cm_next = FALSE;
            if (cm_disp_clear)
                cm_disp_next = -1;
        } else {
            tmp = wc_char_conv(c);
            if (tmp == NULL) {
                i_quote = TRUE;
                goto next_char;
            }
            i_quote = FALSE;
            cm_next = FALSE;
            cm_disp_next = -1;
            if (CLen + tmp->len > STR_LEN || !tmp->len)
                goto next_char;
            ynkCon = 0;
            ins_char(tmp);
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
        return (struct Str) { };

    move(LASTLINE, 0);
    refresh();
    p = strBuf->ptr;
    if (flag & (IN_FILENAME | IN_COMMAND)) {
        SKIP_BLANKS(p);
    }
    if (hist && !(flag & IN_URL) && *p != '\0') {
        char* q = lastHist(hist);
        if (!q || strcmp(q, p))
            pushHist(hist, p);
    }
    if (flag & IN_FILENAME)
        return *expandPath(p);
    else
        return allocStr(p);
}

void addPasswd(char* p, Lineprop* pr, int len, int offset, int limit)
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

void addStr(char* p, Lineprop* pr, int len, int offset, int limit)
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
        delta = wtf_len((uint8_t*)&p[i]);
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

void ins_char(pStr str)
{
    const char *p = str->ptr, *ep = p + str->len;
    Lineprop ctype;
    int len;

    if (CLen + str->len >= STR_LEN)
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
        insC();
        strBuf->ptr[CPos] = *(p++);
        strProp[CPos] = ctype;
        CPos++;
        if (--len) {
            ctype = (ctype & ~PC_WCHAR1) | PC_WCHAR2;
            while (len--) {
                insC();
                strBuf->ptr[CPos] = *(p++);
                strProp[CPos] = ctype;
                CPos++;
            }
        }
    }
}

void _esc(void)
{
    char c;

    switch (c = getch()) {
    case '[':
    case 'O':
        switch (getch()) {
        case 'A':
            _prev();
            break;
        case 'B':
            _next();
            break;
        case 'C':
            _mvR();
            break;
        case 'D':
            _mvL();
            break;
        }
        break;
    case CTRL_I:
    case ' ':
        if (emacs_like_lineedit) {
            _rdcompl();
            cm_clear = FALSE;
            need_redraw = TRUE;
        } else
            _rcompl();
        break;
    case CTRL_D:
        if (!emacs_like_lineedit)
            _rdcompl();
        need_redraw = TRUE;
        break;
    case 'f':
        if (emacs_like_lineedit)
            _mvRw();
        break;
    case 'b':
        if (emacs_like_lineedit)
            _mvLw();
        break;
    case CTRL_H:
        if (emacs_like_lineedit)
            _bsw();
        break;
    default:
        if (wc_char_conv(ESC_CODE) == NULL && wc_char_conv(c) == NULL)
            i_quote = TRUE;
    }
}

void insC(void)
{
    int i;

    Strinsert_char(strBuf, CPos, ' ');
    CLen = strBuf->len;
    for (i = CLen; i > CPos; i--) {
        strProp[i] = strProp[i - 1];
    }
}

void delC(void)
{
    int delta = 1;

    if (CLen == CPos)
        return;
    while (CPos + delta < CLen && strProp[CPos + delta] & PC_WCHAR2)
        delta++;
    for (int i = CPos; i < CLen; i++)
        strProp[i] = strProp[i + delta];

    if (!is_passwd) {
        if (!ynkBuf)
            ynkBuf = Strnew();
        if (!ynkCon)
            Strshrink(ynkBuf, ynkBuf->len); /* TODO(rkta): same as clear? */
        Strinsert_charp_n(ynkBuf, 0, &strBuf->ptr[CPos], delta);
        ynkCon = 1;
    }
    Strdelete(strBuf, CPos, delta);
    CLen -= delta;
}

void _cy(void)
{
    if (rl_paste)
        _paste();
    else
        _mvRw();
}

void _paste(void)
{
    if (!ynkBuf)
        return;
    ins_char(ynkBuf);
    ynkCon = 0;
}

void _mvL(void)
{
    if (CPos > 0)
        CPos--;
    while (CPos > 0 && strProp[CPos] & PC_WCHAR2)
        CPos--;
    ynkCon = 0;
}

void _mvLw(void)
{
    int first = 1;
    while (CPos > 0 && (first || !terminated(strBuf->ptr[CPos - 1]))) {
        CPos--;
        first = 0;
        if (CPos > 0 && strProp[CPos] & PC_WCHAR2)
            CPos--;
        if (is_passwd)
            break;
    }
    ynkCon = 0;
}

void _mvRw(void)
{
    int first = 1;
    while (CPos < CLen && (first || !terminated(strBuf->ptr[CPos - 1]))) {
        CPos++;
        first = 0;
        if (CPos < CLen && strProp[CPos] & PC_WCHAR2)
            CPos++;
        if (is_passwd)
            break;
    }
    ynkCon = 0;
}

void _mvR(void)
{
    if (CPos < CLen)
        CPos++;
    while (CPos < CLen && strProp[CPos] & PC_WCHAR2)
        CPos++;
    ynkCon = 0;
}

void _bs(void)
{
    int y;

    if (CPos > 0) {
        y = ynkCon;
        _mvL();
        ynkCon = y;
        delC();
    }
}

void _bsw(void)
{
    int t = 0, y;

    while (CPos > 0 && !t) {
        y = ynkCon;
        _mvL();
        ynkCon = y;
        t = (!is_passwd && terminated(strBuf->ptr[CPos - 1]));
        delC();
    }
}

void _enter(void)
{
    i_cont = FALSE;
}

void _iword(void)
{
    struct Str str = GetWord(Currentbuf);
    ins_char(&str);
}

void _noop(void)
{
    return;
}

static void
_isrch(void)
{
    ins_char(Strnew_charp(SearchString));
    ynkCon = 0;
}

void _quo(void)
{
    i_quote = TRUE;
}

void _mvB(void)
{
    CPos = 0;
    ynkCon = 0;
}

void _mvE(void)
{
    CPos = CLen;
    ynkCon = 0;
}

void killn(void)
{
    CLen = CPos;
    Strtruncate(strBuf, CLen);
}

void killb(void)
{
    while (CPos > 0)
        _bs();
}

void _inbrk(void)
{
    i_cont = FALSE;
    i_broken = TRUE;
}

void _compl(void)
{
    next_compl(1);
}

void _rcompl(void)
{
    next_compl(-1);
}

void _tcompl(void)
{
    if (cm_mode & CPL_OFF)
        cm_mode = CPL_ON;
    else if (cm_mode & CPL_ON)
        cm_mode = CPL_OFF;
}

void next_compl(int next)
{
    int status;
    int b, a;
    pStr buf;
    pStr s;

    if (cm_mode == CPL_NEVER || cm_mode & CPL_OFF)
        return;
    cm_clear = FALSE;
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
        CAfterBuf = Strsubstr(strBuf, a, strBuf->len - a);
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
    CPos = CBeforeBuf->len + s->len;
    if (CPos > CLen)
        CPos = CLen;
}

void _dcompl(void)
{
    next_dcompl(1);
}

void _rdcompl(void)
{
    next_dcompl(-1);
}

void next_dcompl(int next)
{
    static int col, row;
    static unsigned int len;
    static pStr d;
    int i, j, n, y;
    pStr f;
    char* p;
    struct stat st;
    int comment, nline;

    if (cm_mode == CPL_NEVER || cm_mode & CPL_OFF)
        return;
    cm_disp_clear = FALSE;
    if (CurrentTab)
        displayBuffer(Currentbuf, B_FORCE_REDRAW);
    if (LASTLINE >= 3) {
        comment = TRUE;
        nline = LASTLINE - 2;
    } else if (LASTLINE) {
        comment = FALSE;
        nline = LASTLINE;
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

    cm_next = FALSE;
    next_compl(0);
    if (NCFileBuf == 0)
        return;
    cm_disp_next = 0;

    d = Str_conv_to_system(Strdup(CDirBuf));
    if (d->len > 0 && Strlastchar(d) != '/')
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
        addstr(_("----- Completion list -----"));
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
            if (stat(expandPath(f->ptr)->ptr, &st) != -1 && S_ISDIR(st.st_mode))
                addstr("/");
        }
        y++;
    }
    if (comment && y == LASTLINE - 1) {
        move(y, 0);
        clrtoeolx();
        bold();
        if (emacs_like_lineedit)
            addstr(_("----- Press TAB to continue -----"));
        else
            addstr(_("----- Press CTRL-D to continue -----"));
        boldend();
    }
}

pStr escape_spaces(pStr s)
{
    pStr tmp = NULL;
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

pStr unescape_spaces(pStr s)
{
    pStr tmp = NULL;
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

pStr doComplete(pStr ifn, int* status, int next)
{
    int fl, i;
    const char *fn, *p;
    DIR* d;
    struct dirent* dir;
    struct stat st;

    if (!cm_next) {
        NCFileBuf = 0;
        ifn = Str_conv_to_system(ifn);
        if (cm_mode & CPL_ON)
            ifn = unescape_spaces(ifn);
        CompleteBuf = Strdup(ifn);
        while (Strlastchar(CompleteBuf) != '/' && CompleteBuf->len > 0)
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
                return Str_conv_to_system(CompleteBuf);
            }
        }
        if (CompleteBuf->len == 0) {
            Strcat_char(CompleteBuf, '.');
        }
        if (Strlastchar(CompleteBuf) == '/' && CompleteBuf->len > 1) {
            Strshrink(CompleteBuf, 1);
        }
        if ((d = opendir(expandPath(CompleteBuf->ptr)->ptr)) == NULL) {
            CompleteBuf = Strdup(ifn);
            *status = CPL_FAIL;
            if (cm_mode & CPL_ON)
                CompleteBuf = escape_spaces(CompleteBuf);
            return CompleteBuf;
        }
        fn = mybasename(ifn->ptr);
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
            cm_next = TRUE;
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
    if (CompleteBuf->len && Strlastchar(CompleteBuf) != '/')
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
        if (stat(expandPath(p)->ptr, &st) != -1 && S_ISDIR(st.st_mode))
            Strcat_char(CompleteBuf, '/');
    }
    if (cm_mode & CPL_ON)
        CompleteBuf = escape_spaces(CompleteBuf);
    return Str_conv_from_system(CompleteBuf);
}

void _prev(void)
{
    Hist* hist = CurrentHist;
    char* p;

    if (!hist)
        return;
    if (strCurrentBuf) {
        p = prevHist(hist);
        if (p == NULL)
            return;
    } else {
        p = lastHist(hist);
        if (p == NULL)
            return;
        strCurrentBuf = strBuf;
    }
    if (DecodeURL && (cm_mode & CPL_URL))
        p = url_decode2(p, NULL)->ptr;
    strBuf = Strnew_charp(p);
    CLen = CPos = setStrType(strBuf, strProp);
    offset = 0;
}

void _next(void)
{
    Hist* hist = CurrentHist;
    char* p;

    if (!hist)
        return;
    if (strCurrentBuf == NULL)
        return;
    p = nextHist(hist);
    if (p) {
        if (DecodeURL && (cm_mode & CPL_URL))
            p = url_decode2(p, NULL)->ptr;
        strBuf = Strnew_charp(p);
    } else {
        strBuf = strCurrentBuf;
        strCurrentBuf = NULL;
    }
    CLen = CPos = setStrType(strBuf, strProp);
    offset = 0;
}

int setStrType(pStr str, Lineprop* prop)
{
    Lineprop ctype;
    char *p = str->ptr, *ep = p + str->len;
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

int terminated(const unsigned char c)
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

void _editor(void)
{
    FormItemList fi;
    char* p;

    if (is_passwd)
        return;

    fi.readonly = FALSE;
    fi.value = Strdup(strBuf);
    Strcat_char(fi.value, '\n');

    input_textarea(&fi);

    strBuf = Strnew();
    for (p = fi.value->ptr; *p; p++) {
        if (*p == '\r' || *p == '\n')
            continue;
        Strcat_char(strBuf, *p);
    }
    CLen = CPos = setStrType(strBuf, strProp);
    if (CurrentTab)
        displayBuffer(Currentbuf, B_FORCE_REDRAW);
}
